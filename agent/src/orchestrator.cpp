#include "orchestrator.h"
#include "handlers.h"
#include "ground_service.h"
#include "main_model_broker.h"
#include <sstream>
#include <iomanip>
#include <utility>
#include <map>
#include <vector>
#include <algorithm>
#include <cctype>

namespace cppagent {
namespace {

// std::tolower из <cctype> в локали "C" не трогает кириллицу (UTF-8 многобайтный
// проходит как есть): "Без правок" не находилось по паттерну "без правок".
// Понижаем регистр только для А-Я и Ё (2-байтные последовательности UTF-8).
std::string utf8LowerCyrillic(std::string s) {
    std::string out; out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80) { out += (char)std::tolower(c); i += 1; continue; }
        if (i + 1 < s.size()) {
            unsigned char c2 = (unsigned char)s[i+1];
            if (c == 0xD0 && c2 >= 0x90 && c2 <= 0x9F) { out += (char)0xD0; out += (char)(c2 + 0x20); i += 2; continue; } // А..П
            if (c == 0xD0 && c2 >= 0xA0 && c2 <= 0xAF) { out += (char)0xD1; out += (char)(c2 - 0x20); i += 2; continue; } // Р..Я
            if (c == 0xD0 && c2 == 0x81) { out += (char)0xD1; out += (char)0x91; i += 2; continue; } // Ё
            if (c == 0xD1) { out.append(s, i, 2); i += 2; continue; } // уже строчная
        }
        out += (char)c; i += 1;
    }
    return out;
}
json errorOutcome(const std::string& code, const std::string& msg, const std::string& domain){
    return json{{"protocol_version","1.0"},{"type","error"},{"error",{{"code",code},{"message",msg},{"domain",domain}}}};
}
json needsInputOutcome(const std::string& questionId, const std::string& text, const json& options){
    return json{
        {"protocol_version","1.0"},
        {"type","needs_input"},
        {"needs_input", {
            {"question_id", questionId},
            {"text", text},
            {"options", options},
            {"free_text_allowed", true}
        }}
    };
}

// --- Clarify: question_id — это ключ продолжения ---
// Он говорит не только ЧЕГО хотел юзер, но и ОТКУДА продолжать, минуя шаг,
// где вопрос родился (иначе повторный run заново спросит то же самое).
constexpr const char* kQClassifyLow = "classify-confidence-low";
constexpr const char* kQBlastWide   = "blast-radius-wide";
constexpr const char* kQModelDown   = "model-unavailable"; // модель легла, но эвристика хочет ПИСАТЬ
constexpr const char* kQGoalVague   = "goal-vague";        // goal_text не содержит конкретного объекта/критерия правки

// Ограничение рекурсии: сколько раз можно задать вопрос в рамках одной задачи.
// Защита от зацикливания, если handler после ответа снова спросит.
constexpr int kMaxClarifyDepth = 3;

struct Clarification {
    bool present = false;
    std::string questionId;
    std::string chosenOptionId;
    std::string freeText;
    json history = json::array(); // уже отвеченные question_id (копит клиент)
    std::string choice() const { return !chosenOptionId.empty() ? chosenOptionId : freeText; }
};

Clarification parseClarification(const json& task){
    Clarification c;
    if(task.contains("clarification_history") && task["clarification_history"].is_array())
        c.history = task["clarification_history"];
    if(!task.contains("clarification") || !task["clarification"].is_object()) return c;
    const json& cl = task["clarification"];
    c.present = true;
    c.questionId = cl.value("question_id","");
    c.chosenOptionId = cl.value("chosen_option_id","");
    c.freeText = cl.value("free_text","");
    return c;
}

bool alreadyAnswered(const json& history, const std::string& questionId){
    for(const auto& h : history)
        if(h.is_object() && h.value("question_id","") == questionId) return true;
    return false;
}

int clarifyDepth(const json& history){
    int d = 0;
    for(const auto& h : history)
        if(h.is_object() && h.contains("question_id")) d++;
    return d;
}

// Декоратор ответа: если это needs_input, добавляем history_entry — готовый
// элемент для следующего запроса, плюс depth. Клиент просто добавит entry в
// clarification_history и заполнит chosen_option_id ИЛИ free_text.
json decorateNeedsInput(json out, const Clarification& clar){
    if(out.value("type","") != "needs_input") return out;
    std::string qid = out["needs_input"].value("question_id","");
    if(qid.empty()) return out;

    out["needs_input"]["history_entry"] = json{
        {"question_id", qid},
        {"chosen_option_id", nullptr},
        {"free_text", nullptr}
    };

    int d = clarifyDepth(clar.history) + 1;
    out["needs_input"]["depth"] = d;
    if(d >= kMaxClarifyDepth)
        out["needs_input"]["warning"] = "Достигнут лимит уточнений (" + std::to_string(kMaxClarifyDepth) + "), следующий вопрос будет отклонён";
    return out;
}

json canceledOutcome(const std::string& questionId, const std::string& text){
    return json{
        {"protocol_version","1.0"},
        {"type","canceled"},
        {"canceled", {{"question_id",questionId},{"text",text}}}
    };
}
} // anon

json Orchestrator::run(const json& task){
    std::string file = task.contains("cursor") ? task["cursor"].value("file","") : "";
    int line = task.contains("cursor") ? task["cursor"].value("line",0) : 0;
    int col  = task.contains("cursor") ? task["cursor"].value("column",0) : 0;
    // Якорь: если вызывающий передал USR символа, line/col пересчитываются из него.
    // line/col в задаче — снимок и протухает от любой правки файла выше символа.
    std::string anchorUsr = task.contains("cursor") ? task["cursor"].value("usr","") : "";
    std::string goalText = task.value("goal_text","");
    std::string explicitMode = task.value("explicit_mode","");

    // (1) Приём ответа на уточняющий вопрос — ДО классификации.
    Clarification clar = parseClarification(task);
    int depth = clarifyDepth(clar.history);
    bool answeredClassify = false;
    bool answeredGoalVague = false; // цель уточнена через goal-vague — не спрашивать повторно
    std::string forcedPrimStr;

    if(clar.present){
        // (1a) защита от рекурсии: history уже полна — дальше нельзя
        if(depth >= kMaxClarifyDepth)
            return errorOutcome("clarification_depth_exceeded","превышен лимит уточнений (" + std::to_string(kMaxClarifyDepth) + ")","protocol");
        if(clar.questionId.empty())
            return errorOutcome("clarification_missing_question_id","task.clarification.question_id обязателен","protocol");
        if(alreadyAnswered(clar.history, clar.questionId))
            return errorOutcome("clarification_replay","вопрос \"" + clar.questionId + "\" уже отвечен (есть в clarification_history)","protocol");

        if(clar.questionId == kQClassifyLow){
            std::string pick = clar.choice();
            if(pick.empty())
                return errorOutcome("clarification_empty","нужен chosen_option_id или free_text","protocol");
            bool ok=false;
            Primitive chosen = primitiveFromString(pick, ok);
            if(!ok)
                return errorOutcome("clarification_invalid_option","\"" + pick + "\" — не один из 6 примитивов","protocol");
            forcedPrimStr = primitiveToString(chosen);
            answeredClassify = true;
        } else if(clar.questionId == kQModelDown){
            std::string pick = clar.choice();
            if(pick.empty())
                return errorOutcome("clarification_empty","нужен chosen_option_id или free_text","protocol");
            if(pick == "wait")
                return errorOutcome("model_unavailable_wait","пользователь отложил задачу: модель классификации недоступна","environment");
            bool ok=false;
            Primitive chosen = primitiveFromString(pick, ok);
            if(!ok)
                return errorOutcome("clarification_invalid_option","\"" + pick + "\" — не один из 6 примитивов","protocol");
            forcedPrimStr = primitiveToString(chosen);
            answeredClassify = true;
        } else if(clar.questionId == kQBlastWide){
            std::string pick = clar.choice();
            if(pick == "cancel")
                return canceledOutcome(clar.questionId, "отменено пользователем: правка затронет слишком много мест");
            if(pick != "proceed")
                // fail-safe: НЕ трактуем непонятный ввод как согласие на широкую правку.
                // Ошибка тут дешева, молчаливое согласие — дорого.
                return errorOutcome("clarification_invalid_option",
                    "для широкого изменения нужен явный вариант \"proceed\" или \"cancel\", получено: \"" + pick + "\"","protocol");
            // pick == "proceed" — подтверждено, идём дальше
        } else if(clar.questionId == kQGoalVague){
            std::string pick = clar.choice();
            if(pick == "cancel")
                return canceledOutcome(clar.questionId, "отменено пользователем: цель слишком расплывчата");
            // Любой непустой ответ (не "cancel") — это новая конкретная формулировка от юзера.
            // Перезаписываем goal_text уточнением и идём дальше.
            if(pick.empty())
                return errorOutcome("clarification_empty","нужна конкретная формулировка (что именно нужно сделать)","protocol");
            goalText = pick; // заменяем расплывчатую цель уточнённой
            answeredGoalVague = true; // не спрашивать повторно
        } else {
            return errorOutcome("clarification_unknown_question","неизвестный question_id: \"" + clar.questionId + "\"","protocol");
        }
    }

    // Если ответ уже дан — модель НЕ спрашиваем: выбор юзера и есть примитив.
    // Иначе — либо один вызов, либо self-consistency (N прогонов при temp>0).
    // Включается через task.classify_self_consistency: {repeat, temperature, minority_threshold}.
    // Пример: {"repeat":5,"temperature":0.7,"minority_threshold":0.2}
    // repeat<=1 или отсутствие поля — старый путь (один вызов, verbalized confidence).
    PrimitiveGuess guess;
    json scDebug; // для debug_facts если self-consistency сработал
    bool scAsked = false;
    if(answeredClassify){
        bool ok=false;
        Primitive chosen = primitiveFromString(forcedPrimStr, ok);
        guess = PrimitiveGuess{chosen, forcedPrimStr, "выбор пользователя (ответ на " + clar.questionId + ")", false};
    } else {
        json scCfg = task.value("classify_self_consistency", json::object());
        int scRepeat = scCfg.value("repeat", 0);
        double scTemp = scCfg.value("temperature", 0.7);
        double scMinority = scCfg.value("minority_threshold", 0.2);
        bool useSC = scCfg.is_object() && scRepeat > 1;
        if(useSC){
            // Временно меняем температуру у Light, гоняем N раз, восстанавливаем.
            double savedTemp = 0.1;
            bool isLight = false;
            if(auto* light = dynamic_cast<LightModelBroker*>(&model_)){
                savedTemp = light->temperature();
                light->setTemperature(scTemp);
                isLight = true;
            }
            std::map<std::string,int> counter;
            std::map<std::string,std::string> sampleRationale;
            bool anyUnavailable = false;
            std::string unavailableReason;
            for(int i=0;i<scRepeat;++i){
                PrimitiveGuess g = model_.classifyIntent(goalText);
                if(g.modelUnavailable){ anyUnavailable = true; unavailableReason = g.rationale; break; }
                counter[g.primitiveStr]++;
                if(sampleRationale.find(g.primitiveStr)==sampleRationale.end())
                    sampleRationale[g.primitiveStr]=g.rationale;
            }
            if(isLight){
                if(auto* light = dynamic_cast<LightModelBroker*>(&model_))
                    light->setTemperature(savedTemp);
            }
            if(anyUnavailable){
                guess = PrimitiveGuess{Primitive::UNDERSTAND, "", unavailableReason, true};
            } else {
                // Находим мажоритарный примитив
                std::string majority; int maxCount=0;
                for(auto& [k,v]: counter) if(v>maxCount){ maxCount=v; majority=k; }
                int total=scRepeat;
                double minorityFrac = total>0 ? double(total-maxCount)/double(total) : 0.0;
                // Формируем debug
                json dist=json::object(); for(auto& [k,v]: counter) dist[k]=v;
                scDebug = json{{"repeat",scRepeat},{"temperature",scTemp},{"distribution",dist},{"minority_frac",minorityFrac},{"majority",majority}};
                if(minorityFrac >= scMinority && counter.size() > 1){
                    // Честный сигнал неуверенности — спрашиваем, опции = топ-2 по частоте
                    std::vector<std::pair<std::string,int>> sorted(counter.begin(), counter.end());
                    std::sort(sorted.begin(), sorted.end(), [](auto& a, auto& b){ return a.second > b.second; });
                    std::string top1 = sorted[0].first;
                    std::string top2 = sorted.size()>1 ? sorted[1].first : top1;
                    std::ostringstream q;
                    q << "Модель не уверена (self-consistency: " << scRepeat << " прогонов при temp=" << scTemp << ").\n"
                      << "goal_text: \"" << goalText << "\"\n"
                      << "Распределение: ";
                    for(size_t i=0;i<sorted.size();++i){
                        if(i) q << ", ";
                        q << sorted[i].first << "x" << sorted[i].second;
                    }
                    q << " (minority " << std::fixed << std::setprecision(0) << (minorityFrac*100) << "%). Что ты имел в виду?";
                    json opts = json::array({
                        {{"id",top1}, {"label",top1 + " (" + std::to_string(sorted[0].second) + "/" + std::to_string(total) + ")"}},
                        {{"id",top2}, {"label",top2 + " (" + std::to_string(sorted[1].second) + "/" + std::to_string(total) + ")"}}
                    });
                    json out = needsInputOutcome("classify-confidence-low", q.str(), opts);
                    out["debug_facts"] = scDebug;
                    return decorateNeedsInput(out, clar);
                }
                // Стабильно — берём мажоритарный как основной guess
                bool ok=false;
                Primitive p = primitiveFromString(majority, ok);
                guess = PrimitiveGuess{p, majority, sampleRationale[majority], false};
                guess.confidence = total>0 ? double(maxCount)/double(total) : 1.0;
                if(counter.size() > 1){
                    std::vector<std::pair<std::string,int>> s2(counter.begin(), counter.end());
                    std::sort(s2.begin(), s2.end(), [](auto& a, auto& b){return a.second>b.second;});
                    guess.alternativePrimitiveStr = s2[1].first;
                }
            }
            scAsked = true; // чтобы не дублировать старый verbalized-confidence путь
        }
        if(!useSC){
            guess = model_.classifyIntent(goalText);
        }
    }
    // Модель недоступна: РАНЬШЕ молча падали в UNDERSTAND, из-за чего "пересобери cmake"
    // при мёртвой light давал error: missing_cursor (требовали курсор у чтения). Теперь
    // сначала зовём детерминированную эвристику. Чтение (UNDERSTAND/INFRA) можно взять
    // молча — оно не пишет на диск. Запись — нельзя: спрашиваем явно, а не деградируем тихо.
    if(!answeredClassify && guess.modelUnavailable){
        StubModelBroker stub;
        PrimitiveGuess sb = stub.classifyIntent(goalText);
        const bool writeish = (sb.primitive != Primitive::UNDERSTAND && sb.primitive != Primitive::INFRA);
        if(writeish){
            std::ostringstream q;
            q << "Модель классификации недоступна (" << guess.rationale << ").\n"
              << "Детерминированная эвристика предположила " << sb.primitiveStr
              << " — это ПРАВКА, файлы на диске будут изменены.\n"
              << "Выбери примитив для продолжения или отложи задачу.";
            json opts = json::array({
                {{"id", sb.primitiveStr}, {"label", sb.primitiveStr + " (эвристика, будет запись)"}},
                {{"id","UNDERSTAND"}, {"label","UNDERSTAND (только чтение, безопасно)"}},
                {{"id","wait"}, {"label","Отложить, модель недоступна"}}
            });
            json out = needsInputOutcome(kQModelDown, q.str(), opts);
            out["debug_facts"] = json{{"model_unavailable", true},{"stub_guess", sb.primitiveStr},{"stub_rationale", sb.rationale}};
            return decorateNeedsInput(out, clar);
        }
        // Чтение — берём молча, сохраняя флаг недоступности (handlers пометят deterministic_fallback)
        std::string downReason = guess.rationale;
        guess = sb;
        guess.modelUnavailable = true;
        guess.rationale = sb.rationale + " (модель недоступна: " + downReason + ")";
    }
    Primitive prim = guess.primitive;
    std::string primStr = primitiveToString(prim);

    // Вето-слой (детерминированный, не полагается на модель): если в цели ЯВНО сказано
    // "без правок / не меняй / ничего не менять / только проверь" — любой write-примитив
    // понижается до UNDERSTAND. Модель видит имена методов и склонна выбрать SANDBOX_FIX,
    // игнорируя глагол "проверь": это тот же класс дыры — задача исполняется не как
    // заявлено. Явный запрет записи в тексте сильнее догадки классификатора.
    if(prim != Primitive::UNDERSTAND && prim != Primitive::INFRA){
        std::string lt = utf8LowerCyrillic(goalText);
        bool noEdit = (lt.find("без правок") != std::string::npos)
                   || (lt.find("не меняй") != std::string::npos)
                   || (lt.find("ничего не мен") != std::string::npos)
                   || (lt.find("не изменяй") != std::string::npos)
                   || (lt.find("только провер") != std::string::npos)
                   || (lt.find("только прочита") != std::string::npos)
                   || (lt.find("не редактируй") != std::string::npos);
        if(noEdit){
            guess.rationale += " [вето: цель явно запрещает правки -> UNDERSTAND]";
            prim = Primitive::UNDERSTAND;
            primStr = primitiveToString(prim);
            guess.primitive = prim;
            guess.primitiveStr = primStr;
        }
    }

    // Дыра, а не косметика: агент может быть УВЕРЕН в выборе примитива ("сделай лучше" — это
    // явно правка, confidence=0.8) и при этом в goal_text нет ЧТО улучшать. Неполный запрос
    // тогда исполняется как полный — модель ВЫДУМЫВАЕТ объект правки и записывает результат.
    // confidence и goalIsSpecific — РАЗНЫЕ измерения (см. model_broker.cpp), проверяем оба.
    // Не трогаем UNDERSTAND/INFRA: там нет риска записи вслепую.
    const bool writeishPrim = (prim != Primitive::UNDERSTAND && prim != Primitive::INFRA);
    if(!answeredClassify && !answeredGoalVague && !guess.modelUnavailable && writeishPrim && !guess.goalIsSpecific){
        std::ostringstream q;
        q << "Цель не содержит конкретного объекта правки: \"" << goalText << "\"\n"
          << "Модель классифицировала это как " << primStr << " (правка), но неясно, ЧТО именно менять и когда правка готова.\n"
          << "Уточни: какой метод/файл/поведение нужно изменить и по какому критерию?";
        json opts = json::array({
            {{"id","cancel"}, {"label","Отменить — цель слишком расплывчата"}}
        });
        json out = needsInputOutcome(kQGoalVague, q.str(), opts);
        out["debug_facts"] = json{{"primitive", primStr},{"goal_specific", false}};
        return decorateNeedsInput(out, clar);
    }

    // Тип B (fallback): verbalized confidence — оставляем как запасной путь, если self-consistency не включали.
    // На 7B он не срабатывает (всегда 0.8), но на 12GiB может быть полезен. Не мешает SC, т.к. SC уже вернул бы needs_input выше.
    if(!scAsked && !guess.modelUnavailable && guess.confidence < 0.6 && !guess.alternativePrimitiveStr.empty()){
        std::ostringstream q;
        q << "Модель не уверена. Что ты имел в виду?\n"
          << "goal_text: \"" << goalText << "\"\n\n"
          << "Модель выбрала " << primStr << " (уверенность " << std::fixed << std::setprecision(0) << (guess.confidence*100) << "%), "
          << "но альтернатива — " << guess.alternativePrimitiveStr << ".";
        json opts = json::array({
            {{"id",primStr}, {"label",primStr + " (как модель выбрала)"}},
            {{"id",guess.alternativePrimitiveStr}, {"label",guess.alternativePrimitiveStr + " (альтернатива)"}}
        });
        return decorateNeedsInput(needsInputOutcome("classify-confidence-low", q.str(), opts), clar);
    }

    // INFRA — без Ground, без курсора, без compile_commands
    if(prim==Primitive::INFRA){
        HandlerContext ctx{task, guess, primStr, compileCommandsPath_, {}, GroundResult{}, explicitMode, mainModel_};
        auto h = makeHandler(prim);
        json out = h->handle(ctx);
        if(out.contains("answered_by")) out["answered_by"]["role"] = guess.modelUnavailable ? "deterministic_fallback" : model_.roleName();
        if(clar.present && out.contains("debug_facts"))
            out["debug_facts"]["clarification"] = json{{"question_id",clar.questionId},{"choice",clar.choice()}};
        return out;
    }

    if(file.empty())
        return errorOutcome("missing_cursor","task.cursor обязателен для примитива "+primStr,"environment");

    GroundService groundSvc(compileCommandsPath_);

    // Резолв якоря ДО Ground: USR надежнее line/col, которые могли устареть.
    // Если символа больше нет — это явная ошибка, а не молчаливый промах в "?".
    if(!anchorUsr.empty()){
        json cur = groundSvc.resolveCursorByUsr(file, anchorUsr);
        if(!cur.value("ok", false)){
            json e = cur.value("error", json::object());
            std::string code = e.value("code", "cursor_anchor_lost");
            std::string msg  = e.value("message", "не удалось разрешить cursor.usr в текущие координаты");
            return errorOutcome(code, msg + " (переснимите курсор и передайте актуальные line/col или usr)", "environment");
        }
        line = cur.value("line", line);
        col  = cur.value("column", col);
    }

    GroundResult ground;
    // UNDERSTAND — лёгкий путь: только flags+locate, без symbolRefs/outline/scale
    if(prim==Primitive::UNDERSTAND)
        ground = groundSvc.buildGroundLight(file, line, col);
    else
        ground = groundSvc.buildGround(file, line, col);

    if(!ground.ok){
        json err = ground.error.value("error", json::object());
        // пробрасываем код/сообщение из core как есть
        std::string code = err.value("code", ground.error.value("code","ground_failed"));
        std::string msg  = err.value("message", ground.error.value("message","core Ground failed"));
        // различаем flags vs locate по коду
        std::string domain = "environment";
        return errorOutcome(code, msg, domain);
    }

    // Тип A: детерминированный порог по blast_radius (БЕЗ модели).
    // Если правка затронет слишком много мест — спросить до генерации.
    //
    // Пороги адаптивны к размеру проекта: фиксированный relative-порог (0.25)
    // бессмысленен на 1-3 файловом проекте (там любая правка = 30-100%), и слишком
    // мягок на 500-файловом монолите (0.25 = 125 файлов, туда влезет что угодно).
    // Правило: чем больше проект, тем МЕНЬШЕ относительный порог допустим (в абсолютных
    // числах разрешаем больше файлов, но в процентах — меньше), а совсем маленькие
    // проекты используют только абсолютный порог по refs, не по доле файлов.
    {
        int refsFound = ground.scaleFacts.value("refs_found_total", 0);
        int distinctFiles = ground.scaleFacts.value("distinct_files_with_refs", 0);
        int totalFiles = ground.scaleFacts.value("total_files_in_project", 0);
        std::string scope = ground.scaleFacts.value("scope", "");

        // Абсолютный порог по refs — можно переопределить явно, иначе адаптивная лестница
        // по размеру проекта (маленькие проекты не должны требовать 50 refs, чтобы сработать
        // защита — там уже 5-10 refs это половина кодовой базы).
        int defaultAbs;
        if(totalFiles <= 5)        defaultAbs = 8;
        else if(totalFiles <= 20)  defaultAbs = 20;
        else if(totalFiles <= 100) defaultAbs = 50;
        else                        defaultAbs = 100;
        const int THR_ABS = task.value("blast_radius_threshold", defaultAbs);

        // Относительный порог по доле файлов — тоже адаптивный: крупные проекты не должны
        // спрашивать при 25% (это может быть сотни файлов), маленькие не должны спрашивать
        // просто потому что там мало файлов вообще.
        double defaultRel;
        if(totalFiles <= 3)        defaultRel = 1.01; // отключаем relative-порог полностью, доверяем только абсолютному
        else if(totalFiles <= 10)  defaultRel = 0.5;
        else if(totalFiles <= 50)  defaultRel = 0.3;
        else                        defaultRel = 0.15;
        const double THR_REL = task.value("blast_radius_relative_threshold", defaultRel);

        double frac = totalFiles > 0 ? double(distinctFiles) / double(totalFiles) : 0.0;
        bool hitAbs = (refsFound >= THR_ABS);
        bool hitRel = (totalFiles > 0 && frac >= THR_REL);
        // Размазанность: сигнал сам по себе, но только для проектов покрупнее (>5 файлов) —
        // на совсем маленьких проектах 2 файла — это норма, а не «широкое изменение».
        bool hitSpread = (distinctFiles >= 3 && totalFiles > 10 && distinctFiles >= totalFiles / 4);
        bool risky = hitAbs || hitRel || hitSpread;
        std::string hitReason;
        if(hitAbs) hitReason = "absolute refs " + std::to_string(refsFound) + " >= " + std::to_string(THR_ABS) + " (adaptive threshold for project size " + std::to_string(totalFiles) + ")";
        else if(hitRel) hitReason = "relative " + std::to_string(distinctFiles) + "/" + std::to_string(totalFiles) + " (" + std::to_string(int(frac*100)) + "%) >= " + std::to_string(int(THR_REL*100)) + "%";
        else if(hitSpread) hitReason = "spread " + std::to_string(distinctFiles) + "/" + std::to_string(totalFiles) + " files (>=25% of a medium/large project)";
        bool alreadyAck = (clar.present && clar.questionId == kQBlastWide);
        if(risky && !alreadyAck){
            std::ostringstream q;
            q << "Изменение затронет " << refsFound << " мест в " << distinctFiles << " из " << totalFiles << " файлов (";
            if(totalFiles>0) q << std::fixed << std::setprecision(0) << (frac*100) << "% проекта"; else q << scope;
            q << ", scope: " << scope << ", причина: " << hitReason << ").\n"
              << "Это широкое изменение — продолжить или отменить?";
            json opts = json::array({
                {{"id","proceed"}, {"label","Продолжить, я уверен"}},
                {{"id","cancel"}, {"label","Отменить, слишком широко"}}
            });
            json out = needsInputOutcome("blast-radius-wide", q.str(), opts);
            out["debug_facts"] = json{{"primitive",primStr},{"refs_found_total",refsFound},{"distinct_files_with_refs",distinctFiles},{"total_files_in_project",totalFiles},{"frac",frac},{"scope",scope},{"threshold_abs",THR_ABS},{"threshold_rel",THR_REL},{"hit_reason",hitReason}};
            return decorateNeedsInput(out, clar);
        }
    }

    HandlerContext ctx{task, guess, primStr, compileCommandsPath_, {}, ground, explicitMode, mainModel_};
    auto handler = makeHandler(prim);
    json out = handler->handle(ctx);
    if(out.contains("answered_by")) out["answered_by"]["role"] = guess.modelUnavailable ? "deterministic_fallback" : model_.roleName();
    // диагностика классификатора — чтобы видеть confidence/alternative в каждом ответе
    if(out.contains("debug_facts")){
        out["debug_facts"]["classify_confidence"] = guess.confidence;
        out["debug_facts"]["classify_alternative"] = guess.alternativePrimitiveStr;
        if(!scDebug.is_null()) out["debug_facts"]["self_consistency"] = scDebug;
        if(clar.present)
            out["debug_facts"]["clarification"] = json{{"question_id",clar.questionId},{"choice",clar.choice()}};
    } else if(!scDebug.is_null()){
        out["debug_facts"] = json{{"self_consistency", scDebug}, {"classify_confidence", guess.confidence}};
    }
    return out;
}

} // namespace cppagent
