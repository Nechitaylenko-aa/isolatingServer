#include <catch2/catch_test_macros.hpp>
#include "task_router.h"
#include "model_profile.h"
#include "api_ai.h"

#include <filesystem>
#include <memory>
#include <string>

using namespace cppagent;
namespace fs = std::filesystem;

namespace {
RouteInput base() { RouteInput in; in.primitiveStr = "SANDBOX_FIX"; in.remoteAvailable = false; return in; }
}

TEST_CASE("routeTask: deterministic details go to NONE (no model)", "[router]") {
    RouteInput in = base();
    for (Detail d : {Detail::CALL_CHAIN, Detail::DEPENDENCY_MAP, Detail::TODO_SCAN,
                     Detail::COMPLEXITY_HOTSPOT, Detail::TYPE_USAGE}) {
        in.detail = d;
        CHECK(routeTask(in).executor == Executor::NONE);
    }
}

TEST_CASE("routeTask: large refactor/architecture goes REMOTE, or refuses without remote", "[router]") {
    RouteInput in = base();
    in.detail = Detail::EXTRACT_CLASS;
    in.remoteAvailable = false;
    CHECK(routeTask(in).executor == Executor::REFUSE_LOCAL_INCAPABLE);

    in.remoteAvailable = true;
    CHECK(routeTask(in).executor == Executor::REMOTE);

    // QUARRY_DESIGN -> remote независимо от detail
    RouteInput q = base();
    q.primitiveStr = "QUARRY_DESIGN";
    q.detail = Detail::API_DESIGN;
    q.remoteAvailable = true;
    CHECK(routeTask(q).executor == Executor::REMOTE);
}

TEST_CASE("routeTask: Category C requires remote", "[router]") {
    RouteInput in = base();
    in.detail = Detail::SECURITY_AUDIT;
    in.remoteAvailable = false;
    CHECK(routeTask(in).executor == Executor::REFUSE_LOCAL_INCAPABLE);
    in.remoteAvailable = true;
    CHECK(routeTask(in).executor == Executor::REMOTE);
}

TEST_CASE("routeTask: small edit within local profile -> LOCAL", "[router]") {
    RouteInput in = base();
    in.detail = Detail::LOCAL_FIX;
    in.localCapable = true;
    in.distinctFiles = 1;
    in.totalFiles = 60;
    in.localMaxEditFiles = 3;
    in.remoteAvailable = true;
    CHECK(routeTask(in).executor == Executor::LOCAL);
}

TEST_CASE("routeTask: wide edit -> SPLIT when remote available", "[router]") {
    RouteInput in = base();
    in.detail = Detail::LOCAL_FIX;
    in.localCapable = true;
    in.distinctFiles = 10; // > localMaxEditFiles
    in.totalFiles = 60;
    in.localMaxEditFiles = 3;
    in.remoteAvailable = true;
    CHECK(routeTask(in).executor == Executor::SPLIT);

    // без remote — честный отказ, а не слепая попытка
    in.remoteAvailable = false;
    CHECK(routeTask(in).executor == Executor::REFUSE_LOCAL_INCAPABLE);
}

TEST_CASE("routeTask: read-only goes LOCAL; if local incapable -> remote/refuse", "[router]") {
    RouteInput in = base();
    in.detail = Detail::GENERAL_QUESTION;
    in.localCapable = true;
    CHECK(routeTask(in).executor == Executor::LOCAL);

    in.localCapable = false;
    in.remoteAvailable = true;
    CHECK(routeTask(in).executor == Executor::REMOTE);
    in.remoteAvailable = false;
    CHECK(routeTask(in).executor == Executor::REFUSE_LOCAL_INCAPABLE);
}

// --- ModelProfile (офлайн, через FakeApi) ---

namespace {
class ProbeFake : public IApiAI {
public:
    bool validJson = true;
    bool ok = true;
    ChatResult chat(const ChatRequest&) override {
        ChatResult r; r.ok = ok;
        r.text = validJson ? "{\"ok\": true}" : "не json";
        return r;
    }
    std::string name() const override { return "probe-fake"; }
};
}

TEST_CASE("ModelProfile: probe measures JSON compliance and caches", "[profile]") {
    fs::path dir = fs::temp_directory_path() / ("cf_agent_prof_" + std::to_string(::rand()));
    std::error_code ec; fs::remove_all(dir, ec);

    ProbeFake good; good.validJson = true;
    ModelProfile p = ModelProfile::probe(good, "test-model-q8", dir.string(), nullptr);
    CHECK(p.probed);
    CHECK(p.jsonCompliance == 1.0);
    CHECK(p.capableCoding);

    // кэш записан и читается
    ModelProfile loaded = ModelProfile::load(dir.string(), "test-model-q8");
    CHECK(loaded.probed);
    CHECK(loaded.jsonCompliance == 1.0);

    // плохая модель — низкая послушность
    ProbeFake bad; bad.validJson = false;
    ModelProfile p2 = ModelProfile::probe(bad, "weak-model", dir.string(), nullptr);
    CHECK(p2.jsonCompliance == 0.0);
    CHECK(!p2.capableCoding);

    fs::remove_all(dir, ec);
}

TEST_CASE("ModelProfile: remote verdict overrides heuristic", "[profile]") {
    fs::path dir = fs::temp_directory_path() / ("cf_agent_prof_r_" + std::to_string(::rand()));
    std::error_code ec; fs::remove_all(dir, ec);

    ProbeFake local; local.validJson = false; // эвристика сказала бы "не тянет"
    class VerdictFake : public IApiAI {
    public:
        ChatResult chat(const ChatRequest&) override {
            ChatResult r; r.ok = true;
            r.text = "{\"capable_coding\": true, \"capable_reasoning\": true, \"verdict\": \"ок\"}";
            return r;
        }
        std::string name() const override { return "verdict"; }
    } remote;

    ModelProfile p = ModelProfile::probe(local, "m1", dir.string(), &remote);
    CHECK(p.source == "remote");
    CHECK(p.capableCoding); // вердикт remote переопределил эвристику
    fs::remove_all(dir, ec);
}
