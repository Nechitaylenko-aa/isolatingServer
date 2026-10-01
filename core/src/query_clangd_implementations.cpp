#include "queries.h"
#include "json.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <filesystem>
#include <functional>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>

// LSP/clangd-based implementation finder.
// Sends textDocument/implementation to a clangd subprocess and returns all
// locations. This works even when the file is a header not in
// compile_commands.json, because clangd indexes the whole project and
// resolves virtual dispatch across TUs.

namespace cpptool {

namespace {
namespace fs = std::filesystem;

// Write a JSON-RPC message to a pipe: Content-Length header + body.
bool writeRpc(FILE* fp, const json& msg) {
    std::string body = msg.dump();
    std::string frame = "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    return fwrite(frame.data(), 1, frame.size(), fp) == frame.size();
}

// Read one JSON-RPC message from a pipe (Content-Length framing).
// Returns empty string on EOF or error.
std::string readRpc(FILE* fp) {
    // Read headers until blank line.
    int contentLength = -1;
    std::string line;
    while (true) {
        int c = fgetc(fp);
        if (c == EOF) return "";
        if (c == '\r') { fgetc(fp); /* skip \n */
            if (line.empty()) break; // blank line = end of headers
            if (line.rfind("Content-Length:", 0) == 0)
                contentLength = std::stoi(line.substr(16));
            line.clear();
        } else {
            line += (char)c;
        }
    }
    if (contentLength <= 0) return "";
    std::string body(contentLength, '\0');
    if ((int)fread(&body[0], 1, contentLength, fp) != contentLength) return "";
    return body;
}

// Drain and discard messages until we see one matching predicate.
// Returns the matching message or empty json on timeout/EOF.
json waitFor(FILE* fp, const std::function<bool(const json&)>& pred, int maxMessages = 60) {
    for (int i = 0; i < maxMessages; ++i) {
        std::string raw = readRpc(fp);
        if (raw.empty()) return json{};
        try {
            json msg = json::parse(raw);
            if (pred(msg)) return msg;
        } catch (...) {}
    }
    return json{};
}

} // namespace

// queryClangdImplementations: launch clangd as a subprocess, send
// textDocument/implementation at (file, line, col), collect Location[] result.
// line and col are 1-based (converted to 0-based for LSP internally).
// Returns {ok, locations:[{file,line,col}], error?}
json queryClangdImplementations(const std::string& compileCommandsPath,
                                const std::string& file,
                                unsigned line,
                                unsigned col) {
    if (file.empty())
        return json{{"ok", false}, {"error", {{"code", "empty_file"}}}};

    // Derive project root from compile_commands.json location.
    fs::path ccPath = fs::absolute(compileCommandsPath);
    std::string projectRoot = ccPath.parent_path().string();

    // Build file URI.
    auto toUri = [](const std::string& path) -> std::string {
        return "file://" + fs::absolute(path).string();
    };
    std::string fileUri = toUri(file);

    // Spawn clangd. We use popen2-style via a shell pipe pair.
    // clangd reads JSON-RPC on stdin, writes on stdout.
    // --background-index=false: answer from AST only, no wait for full index build.
    // --log=error: suppress progress spam on stderr.
    std::string cmd = "clangd --compile-commands-dir=" + projectRoot
                    + " --background-index=false --log=error --limit-results=0"
                    + " --offset-encoding=utf-8 2>/dev/null";

    // We need bidirectional pipes. Use popen with a helper pipe pair via /proc/self/fd trick,
    // or simply use popen + a separate read pipe. The portable approach on Linux:
    // create two anonymous pipes and fork/exec.
    int toClangd[2], fromClangd[2];
    if (pipe(toClangd) != 0 || pipe(fromClangd) != 0)
        return json{{"ok", false}, {"error", {{"code", "pipe_failed"}, {"message", "не удалось создать pipes"}}}};

    pid_t pid = fork();
    if (pid < 0) {
        return json{{"ok", false}, {"error", {{"code", "fork_failed"}, {"message", "fork() failed"}}}};
    }
    if (pid == 0) {
        // Child: wire pipes to stdin/stdout.
        dup2(toClangd[0], STDIN_FILENO);
        dup2(fromClangd[1], STDOUT_FILENO);
        close(toClangd[0]); close(toClangd[1]);
        close(fromClangd[0]); close(fromClangd[1]);
        // redirect stderr to /dev/null
        int devNull = open("/dev/null", O_WRONLY);
        if (devNull >= 0) dup2(devNull, STDERR_FILENO);
        execlp("clangd", "clangd",
               ("--compile-commands-dir=" + projectRoot).c_str(),
               "--background-index=false", "--log=error",
               "--limit-results=0", "--offset-encoding=utf-8",
               nullptr);
        _exit(1);
    }
    // Parent.
    close(toClangd[0]);
    close(fromClangd[1]);

    FILE* toFp   = fdopen(toClangd[1],  "w");
    FILE* fromFp = fdopen(fromClangd[0], "r");
    if (!toFp || !fromFp) {
        kill(pid, SIGKILL);
        return json{{"ok", false}, {"error", {{"code", "fdopen_failed"}}}};
    }

    int reqId = 1;

    // 1. initialize
    writeRpc(toFp, json{
        {"jsonrpc", "2.0"}, {"id", reqId++}, {"method", "initialize"},
        {"params", {
            {"processId", (int)getpid()},
            {"rootUri", toUri(projectRoot)},
            {"capabilities", json::object()},
            {"initializationOptions", {{"compilationDatabasePath", projectRoot}}}
        }}
    });
    fflush(toFp);

    // Wait for initialize result (id==1).
    waitFor(fromFp, [](const json& m){ return m.contains("id") && m["id"] == 1; });

    // 2. initialized notification
    writeRpc(toFp, json{{"jsonrpc", "2.0"}, {"method", "initialized"}, {"params", json::object()}});

    // 3. textDocument/didOpen (clangd needs the document open to respond to requests)
    // Read file content for didOpen.
    std::string fileContent;
    {
        FILE* f = fopen(file.c_str(), "r");
        if (f) {
            char buf[4096];
            while (size_t n = fread(buf, 1, sizeof(buf), f)) fileContent.append(buf, n);
            fclose(f);
        }
    }
    writeRpc(toFp, json{
        {"jsonrpc", "2.0"}, {"method", "textDocument/didOpen"},
        {"params", {{"textDocument", {
            {"uri", fileUri},
            {"languageId", "cpp"},
            {"version", 1},
            {"text", fileContent}
        }}}}
    });
    fflush(toFp);

    // Wait for clangd to finish parsing the opened file.
    waitFor(fromFp, [&fileUri](const json& m){
        return m.value("method", "") == "textDocument/publishDiagnostics"
            && m.contains("params")
            && m["params"].value("uri", "") == fileUri;
    }, 200);

    // --background-index=false means clangd only knows files opened in this session.
    // For textDocument/implementation to find overrides of a pure virtual method in a
    // header, clangd must have parsed the .cpp files that contain the subclasses.
    // We open all project files from compile_commands.json so clangd builds a full
    // in-session index before we ask for implementations.
    {
        std::ifstream ccIn(compileCommandsPath);
        if (ccIn) {
            json ccJson;
            try { ccIn >> ccJson; } catch (...) { ccJson = json::array(); }
            for (auto& entry : ccJson) {
                std::string srcFile = entry.value("file", "");
                if (srcFile.empty()) continue;
                std::string srcUri = toUri(srcFile);
                if (srcUri == fileUri) continue; // already opened
                std::string srcContent;
                FILE* sf = fopen(srcFile.c_str(), "r");
                if (!sf) continue;
                char buf[4096];
                while (size_t n = fread(buf, 1, sizeof(buf), sf)) srcContent.append(buf, n);
                fclose(sf);
                // Detect language from extension.
                std::string lang = "cpp";
                if (srcFile.size() > 2 && srcFile.substr(srcFile.size()-2) == ".c") lang = "c";
                writeRpc(toFp, json{
                    {"jsonrpc", "2.0"}, {"method", "textDocument/didOpen"},
                    {"params", {{"textDocument", {
                        {"uri", srcUri}, {"languageId", lang},
                        {"version", 1}, {"text", srcContent}
                    }}}}
                });
            }
            fflush(toFp);
            // Drain notifications until clangd has processed all opened files.
            // We wait for publishDiagnostics count to reach the number of opened files,
            // or until no more messages arrive within the budget.
            waitFor(fromFp, [](const json& m){
                // Just drain — stop when we hit a lull (maxMessages exhausted).
                return false;
            }, static_cast<int>(ccJson.size()) * 4 + 40);
        }
    }

    // 4. textDocument/implementation — LSP uses 0-based line/col.
    int lspLine = (line > 0) ? (int)(line - 1) : 0;
    int lspCol  = (col  > 0) ? (int)(col  - 1) : 0;
    writeRpc(toFp, json{
        {"jsonrpc", "2.0"}, {"id", reqId}, {"method", "textDocument/implementation"},
        {"params", {
            {"textDocument", {{"uri", fileUri}}},
            {"position", {{"line", lspLine}, {"character", lspCol}}}
        }}
    });
    fflush(toFp);

    int implId = reqId;

    // Wait for the implementation response.
    json implResp = waitFor(fromFp, [implId](const json& m){
        return m.contains("id") && m["id"] == implId;
    }, 120);

    // 5. shutdown
    writeRpc(toFp, json{{"jsonrpc","2.0"},{"id", reqId+1},{"method","shutdown"},{"params",nullptr}});
    fflush(toFp);
    fclose(toFp);
    fclose(fromFp);
    int status = 0;
    waitpid(pid, &status, 0);

    if (implResp.is_null() || !implResp.contains("result"))
        return json{{"ok", false}, {"error", {{"code", "no_response"}, {"message", "clangd не вернул ответ на textDocument/implementation"}}}};

    auto& result = implResp["result"];
    json locations = json::array();

    auto parseLocation = [](const json& loc) -> json {
        std::string uri = loc.value("uri", "");
        // Strip file:// prefix.
        std::string path = (uri.rfind("file://", 0) == 0) ? uri.substr(7) : uri;
        int ln  = loc.contains("range") ? (int)loc["range"]["start"].value("line", 0) + 1 : 0;
        int ch  = loc.contains("range") ? (int)loc["range"]["start"].value("character", 0) + 1 : 0;
        return json{{"file", path}, {"line", ln}, {"column", ch}};
    };

    if (result.is_array()) {
        for (auto& loc : result) locations.push_back(parseLocation(loc));
    } else if (result.is_object() && result.contains("uri")) {
        locations.push_back(parseLocation(result));
    }

    return json{{"ok", true}, {"locations", locations}, {"source", "clangd/textDocument/implementation"}};
}

} // namespace cpptool