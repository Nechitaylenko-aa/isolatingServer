#pragma once

#include <string>
#include <vector>

namespace cppagent {

// Единственный источник правды для имён файлов конфига. Нигде больше строки
// "agent.conf"/"projects" не хардкодятся — иначе при переименовании рассинхрон.
inline constexpr const char* kConfigFileName = "agent.conf";
inline constexpr const char* kProjectsFileName = "projects";

// cf-agent configuration (FreeBSD rc.conf style, no JSON/YAML).
//
// Каталог по умолчанию: ~/.cf_agent
//   agent.conf — key="value", '#' — комментарий, ~ разворачивается
//                (синтаксис в духе FreeBSD rc.conf, но файл называется agent.conf)
//   projects  — fstab-стиль: <project_root> <build_dir|"" > <db_file|"">
//
// DB по умолчанию: <db_dir>/<hash(project_root)>.db — детерминированно, без
// обязательной записи в projects. Маппинг нужен только для нестандартных
// build-каталогов/явного расположения БД.

struct ModelEndpoint {
    std::string url;      // пусто = не задан
    std::string name;
    std::string apiType;  // формат API: "openai-compat" | "claude" (пусто = openai-compat)
    std::string apiKey;   // для remote
    bool configured() const { return !url.empty(); }
};

struct ProjectEntry {
    std::string root;      // канонический project root
    std::string buildDir;  // абсолютный (может быть пустым -> defaultBuild)
    std::string dbFile;    // абсолютный override (может быть пустым)
};

struct AgentConfig {
    ModelEndpoint light;
    ModelEndpoint main;
    ModelEndpoint remote;
    std::string dbDir;         // абсолютный
    std::string defaultBuild;  // напр. "build"
    std::string configDir;     // абсолютный
    std::string rcPath;
    std::string projectsPath;
    // Политики доступа (YES/NO). Дефолты: чтение/запись разрешены, exec/subtasks нет.
    bool allowRead{true};
    bool allowWrite{true};
    bool allowExec{false};
    bool allowSubtasks{false};
};

struct ProjectResolution {
    bool ok{false};
    std::string error;
    std::string projectRoot;
    std::string buildDir;
    std::string compileCommands;
    std::string dbFile;
};

class CConfigReader {
public:
    // Загружает (при отсутствии — создаёт с комментариями) конфиг из configDir.
    static CConfigReader loadOrCreate(const std::string& configDir);

    const AgentConfig& config() const { return cfg_; }
    const std::vector<ProjectEntry>& projects() const { return projects_; }

    // ccOverride (--cc) — прямое поведение, совместимость.
    // projectOverride (--project) — корень проекта, ищем/регистрируем в projects.
    // dbOverride (--db) — сильнее всего.
    ProjectResolution resolve(const std::string& ccOverride,
                              const std::string& projectOverride,
                              const std::string& dbOverride) const;

    // Дописать проект в projects (атомарно). true если запись добавлена.
    bool registerProject(const ProjectEntry& e) const;

    // Перечитать конфиг с диска (GUI/TUI правит agent.conf/проекты в рантайме).
    // Не создаёт файлы; возвращает false, если конфиг недоступен.
    bool refresh_from_config();

    // Для GUI/TUI: человекочитаемое описание моделей ("name @ url" | url | "").
    std::string get_main_model() const;
    std::string get_lite_model() const;
    std::string get_remote_model() const;

    // --- чистая логика (публична для тестов) ---
    static bool parseKeyValue(const std::string& line, std::string& key, std::string& value);
    static std::string expandPath(const std::string& value);   // ~, $HOME
    static std::string dbFileNameFor(const std::string& canonicalRoot);

private:
    AgentConfig cfg_;
    std::vector<ProjectEntry> projects_;
    void loadRc();
    void loadProjects();
    void applyDefaults();
    std::string defaultDbPathFor(const std::string& canonicalRoot) const;
};

} // namespace cppagent
