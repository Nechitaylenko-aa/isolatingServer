#include "config_reader.h"

#include "hash.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace cppagent {

namespace {

std::string canonical(const std::string& p) {
    std::error_code ec;
    fs::path abs = fs::absolute(p, ec);
    if (ec) abs = fs::path(p);
    fs::path w = fs::weakly_canonical(abs, ec);
    if (ec) w = abs.lexically_normal();
    return w.string();
}

std::string homeDir() {
    const char* h = std::getenv("HOME");
    return h ? h : "";
}

std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

} // namespace

std::string CConfigReader::expandPath(const std::string& value) {
    std::string v = value;
    if (v == "~") return homeDir();
    if (v.rfind("~/", 0) == 0) return homeDir() + v.substr(1);
    if (v.rfind("$HOME", 0) == 0) return homeDir() + v.substr(5);
    return v;
}

// Разбирает строку rc.conf-стиля: key=value | key="value" | key='value'.
// Возвращает false для пустых строк, комментариев и строк без '='.
bool CConfigReader::parseKeyValue(const std::string& line, std::string& key, std::string& value) {
    std::string s = trim(line);
    if (s.empty() || s[0] == '#') return false;
    auto eq = s.find('=');
    if (eq == std::string::npos) return false;
    key = trim(s.substr(0, eq));
    if (key.empty()) return false;
    std::string val = trim(s.substr(eq + 1));
    if (val.size() >= 2 && (val.front() == '"' || val.front() == '\'') && val.back() == val.front())
        val = val.substr(1, val.size() - 2);
    value = val;
    return true;
}

std::string CConfigReader::dbFileNameFor(const std::string& canonicalRoot) {
    // Детерминированный файл БД по каноническому корню проекта.
    return cpptool::hash_string(canonicalRoot) + ".db";
}

CConfigReader CConfigReader::loadOrCreate(const std::string& configDirIn) {
    CConfigReader c;
    std::string dir = configDirIn.empty() ? (homeDir() + "/.cf_agent") : configDirIn;
    c.cfg_.configDir = canonical(dir);
    c.cfg_.rcPath = c.cfg_.configDir + "/" + kConfigFileName;
    c.cfg_.projectsPath = c.cfg_.configDir + "/" + kProjectsFileName;

    std::error_code ec;
    fs::create_directories(c.cfg_.configDir, ec);

    if (!fs::exists(c.cfg_.rcPath)) {
        std::ofstream out(c.cfg_.rcPath);
        out <<
            "# cf-agent configuration (FreeBSD rc.conf style, файл agent.conf)\n"
            "# Комментарии — с '#'.\n"
            "#\n"
            "# Модели: light — мышление (анализ/обобщение), main — кодер.\n"
            "# Пока обе роли может исполнять одна модель: укажите 'same_as_main'\n"
            "# у light (или 'same_as_light' у main).\n"
            "model_main=\"http://127.0.0.1:8080\"\n"
            "model_main_name=\"\"\n"
            "model_light=\"same_as_main\"\n"
            "model_light_name=\"\"\n"
            "# remote — удалённая модель (задел, пока не используется)\n"
            "# model_remote_type — формат API провайдера:\n"
            "#   openai-compat — OpenAI-совместимый /v1/chat/completions\n"
            "#                   (llama.cpp, vLLM, Ollama, OpenAI, OpenRouter...)\n"
            "#   claude        — Anthropic Messages API (/v1/messages, x-api-key)\n"
            "#                   (реализация добавляется отдельным CApiAnthropic)\n"
            "model_remote_url=\"\"\n"
            "model_remote_type=\"openai-compat\"\n"
            "model_remote_name=\"\"\n"
            "model_remote_key=\"\"\n"
            "# Где хранить БД (вне проектов) и как называть build-каталог по умолчанию\n"
            "db_dir=\"" + c.cfg_.configDir + "/db\"\n"
            "default_build=\"build\"\n"
            "# Политики доступа агента (YES/NO)\n"
            "#   allow_read     — чтение/анализ кода (UNDERSTAND и т.п.)\n"
            "#   allow_write    — правка файлов (SANDBOX_FIX/QUARRY/TEST_GEN)\n"
            "#   allow_exec     — запуск команд (сборка, ninja, ctest)\n"
            "#   allow_subtasks — разрешить агенту порождать подзадачи (см. обсуждение)\n"
            "allow_read=\"YES\"\n"
            "allow_write=\"YES\"\n"
            "allow_exec=\"NO\"\n"
            "allow_subtasks=\"NO\"\n";
    }
    if (!fs::exists(c.cfg_.projectsPath)) {
        std::ofstream out(c.cfg_.projectsPath);
        out <<
            "# cf-agent project map (fstab style)\n"
            "# <project_root>  <build_dir|empty>  <db_file|empty>\n"
            "# build_dir относительный -> от project_root; db_file пустой ->\n"
            "# <db_dir>/<hash(project_root)>.db\n";
    }

    c.loadRc();
    c.applyDefaults();
    c.loadProjects();
    return c;
}

void CConfigReader::loadRc() {
    std::ifstream in(cfg_.rcPath);
    if (!in) return;
    std::string line, key, val;
    std::string mainUrl, mainName, lightUrl, lightName, lightAlias;
    bool lightSameAsMain = false, mainSameAsLight = false;

    while (std::getline(in, line)) {
        if (!parseKeyValue(line, key, val)) continue;
        if (key == "model_main")            mainUrl = val;
        else if (key == "model_main_name")  mainName = val;
        else if (key == "model_light")      { lightUrl = val; if (val == "same_as_main") lightSameAsMain = true; }
        else if (key == "model_light_name") lightName = val;
        else if (key == "model_remote_url")  cfg_.remote.url = val;
        else if (key == "model_remote_type") cfg_.remote.apiType = val;
        else if (key == "model_remote_name") cfg_.remote.name = val;
        else if (key == "model_remote_key")  cfg_.remote.apiKey = val;
        else if (key == "db_dir")           cfg_.dbDir = expandPath(val);
        else if (key == "default_build")    cfg_.defaultBuild = val;
        else if (key == "allow_read")       cfg_.allowRead = (val == "YES" || val == "yes" || val == "1" || val == "true");
        else if (key == "allow_write")      cfg_.allowWrite = (val == "YES" || val == "yes" || val == "1" || val == "true");
        else if (key == "allow_exec")       cfg_.allowExec = (val == "YES" || val == "yes" || val == "1" || val == "true");
        else if (key == "allow_subtasks")   cfg_.allowSubtasks = (val == "YES" || val == "yes" || val == "1" || val == "true");
        else if (key == "model_main" && val == "same_as_light") mainSameAsLight = true;
    }
    if (mainUrl == "same_as_light") mainSameAsLight = true;
    (void)lightAlias;

    cfg_.main.url = mainUrl;
    cfg_.main.name = mainName;
    cfg_.light.url = lightUrl;
    cfg_.light.name = lightName;

    if (lightSameAsMain) { cfg_.light.url = mainUrl; cfg_.light.name = lightName.empty() ? mainName : lightName; }
    if (mainSameAsLight) { cfg_.main.url = lightUrl; cfg_.main.name = mainName.empty() ? lightName : mainName; }
}

void CConfigReader::applyDefaults() {
    if (cfg_.dbDir.empty()) cfg_.dbDir = cfg_.configDir + "/db";
    cfg_.dbDir = canonical(cfg_.dbDir);
    std::error_code ec;
    fs::create_directories(cfg_.dbDir, ec);
    if (cfg_.defaultBuild.empty()) cfg_.defaultBuild = "build";
}

void CConfigReader::loadProjects() {
    std::ifstream in(cfg_.projectsPath);
    if (!in) return;
    std::string line;
    while (std::getline(in, line)) {
        std::string s = trim(line);
        if (s.empty() || s[0] == '#') continue;
        std::istringstream ss(s);
        ProjectEntry e;
        ss >> e.root >> e.buildDir >> e.dbFile;
        if (!e.root.empty()) projects_.push_back(e);
    }
}

std::string CConfigReader::defaultDbPathFor(const std::string& canonicalRoot) const {
    return cfg_.dbDir + "/" + dbFileNameFor(canonicalRoot);
}

ProjectResolution CConfigReader::resolve(const std::string& ccOverride,
                                         const std::string& projectOverride,
                                         const std::string& dbOverride) const {
    ProjectResolution r;

    // (1) Прямой --cc: поведение как раньше, build = родитель cc, БД = override|дефолт.
    if (!ccOverride.empty()) {
        std::string cc = canonical(ccOverride);
        r.compileCommands = cc;
        r.buildDir = fs::path(cc).parent_path().string();
        r.projectRoot = fs::path(r.buildDir).parent_path().string();
        r.dbFile = !dbOverride.empty() ? canonical(dbOverride) : defaultDbPathFor(r.projectRoot);
        r.ok = true;
        return r;
    }

    if (projectOverride.empty()) {
        r.error = "не задан ни --cc <compile_commands.json>, ни --project <root>";
        return r;
    }

    std::string root = canonical(projectOverride);
    if (!fs::exists(root)) {
        r.error = "project root не существует: " + root;
        return r;
    }

    // (2) Ищем в projects.
    std::string buildRel, dbFile;
    for (const auto& e : projects_) {
        if (canonical(e.root) == root) { buildRel = e.buildDir; dbFile = e.dbFile; break; }
    }

    // (3) Нет записи — авто-регистрируем с default_build.
    if (buildRel.empty() && dbFile.empty()) {
        ProjectEntry e;
        e.root = root;
        e.buildDir = cfg_.defaultBuild;
        registerProject(e);
        buildRel = cfg_.defaultBuild;
    }

    r.projectRoot = root;
    r.buildDir = buildRel.empty() ? (root + "/" + cfg_.defaultBuild)
                                  : (fs::path(buildRel).is_absolute() ? buildRel : (root + "/" + buildRel));
    r.buildDir = canonical(r.buildDir);
    r.compileCommands = r.buildDir + "/compile_commands.json";
    r.dbFile = !dbOverride.empty() ? canonical(dbOverride)
               : (!dbFile.empty() ? canonical(fs::path(dbFile).is_absolute() ? dbFile : (root + "/" + dbFile))
                                  : defaultDbPathFor(root));
    r.ok = true;
    return r;
}

bool CConfigReader::refresh_from_config() {
    // Сброс к дефолтам и повторное чтение с диска (конфиг мог быть переписан GUI/TUI).
    cfg_.light = ModelEndpoint{};
    cfg_.main = ModelEndpoint{};
    cfg_.remote = ModelEndpoint{};
    cfg_.allowRead = true; cfg_.allowWrite = true;
    cfg_.allowExec = false; cfg_.allowSubtasks = false;
    projects_.clear();
    if (!fs::exists(cfg_.rcPath)) return false;
    loadRc();
    applyDefaults();
    loadProjects();
    return true;
}

static std::string describeEndpoint(const ModelEndpoint& e) {
    if (!e.configured()) return "";
    if (!e.name.empty()) return e.name + " @ " + e.url;
    return e.url;
}

std::string CConfigReader::get_main_model() const { return describeEndpoint(cfg_.main); }
std::string CConfigReader::get_lite_model() const { return describeEndpoint(cfg_.light); }
std::string CConfigReader::get_remote_model() const { return describeEndpoint(cfg_.remote); }

bool CConfigReader::registerProject(const ProjectEntry& e) const {
    // Не дублируем существующие записи (по каноническому корню).
    std::string root = canonical(e.root);
    for (const auto& p : projects_)
        if (canonical(p.root) == root) return false;

    // Атомарная дописка: temp + rename, чтобы не портить файл при сбое.
    std::string tmp = cfg_.projectsPath + ".tmp";
    {
        std::ifstream in(cfg_.projectsPath);
        std::ofstream out(tmp, std::ios::trunc);
        if (in) out << in.rdbuf();
        out << root << " " << e.buildDir << " " << e.dbFile << "\n";
    }
    std::error_code ec;
    fs::rename(tmp, cfg_.projectsPath, ec);
    return !ec;
}

} // namespace cppagent
