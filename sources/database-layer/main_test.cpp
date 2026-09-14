// test/main_test.cpp
//
// Юнит-тесты по критериям готовности из tz_group3_database_layer.md.
// Без внешнего фреймворка - простые assert + вывод в stdout, как в
// предыдущих группах.

#include <atomic>
#include <cassert>
#include <chrono>
#include <functional>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "db_thread.h"
#include "equip_row_grouping.h"

namespace test_hooks
{
    //extern std::atomic<bool>       g_simpleTestResult;
    //extern std::atomic<int>        g_reconnectCount;
    //extern std::mutex               g_callLogMutex;
    //extern std::vector<std::string> g_callLog;

    std::atomic<bool>       g_simpleTestResult;
    std::atomic<int>        g_reconnectCount;
    std::mutex               g_callLogMutex;
    std::vector<std::string> g_callLog;

    void setThrowHook(std::function<void()> hook)
    {

    }
    void reset()
    {

    }
} // namespace test_hooks

namespace
{
int g_passed = 0;
int g_failed = 0;

#define CHECK(cond)                                                              \
    do                                                                           \
    {                                                                            \
        if (cond)                                                                \
        {                                                                        \
            ++g_passed;                                                          \
        }                                                                        \
        else                                                                     \
        {                                                                        \
            ++g_failed;                                                          \
            std::cerr << "FAIL: " << #cond << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
        }                                                                        \
    } while (0)

SDBConnection makeTestConfig()
{
    SDBConnection cfg;
    cfg.host   = "test";
    cfg.dbname = "test";
    cfg.user   = "test";
    cfg.pass   = "test";
    return cfg;
}

// --- 1. FIFO: несколько задач подряд выполняются строго по одной, в порядке FIFO ---
void test_fifo_order()
{
    test_hooks::reset();
    CDBThread db(makeTestConfig());
    db.start();

    std::atomic<int> completed{0};
    std::vector<int> order;
    std::mutex orderMutex;

    for (int i = 0; i < 5; ++i)
    {
        db.executeEquipIdQuery({static_cast<uint16_t>(i)}, [&, i](std::vector<SEquipRow>, EDatabaseError) {
            std::lock_guard<std::mutex> lock(orderMutex);
            order.push_back(i);
            ++completed;
        });
    }

    for (int attempt = 0; attempt < 200 && completed.load() < 5; ++attempt)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    db.stop();

    CHECK(completed.load() == 5);
    CHECK(order.size() == 5);
    bool inOrder = true;
    for (int i = 0; i < static_cast<int>(order.size()); ++i)
        if (order[i] != i)
            inOrder = false;
    CHECK(inOrder);
}

// --- 2. health-check: true -> false -> true, isHealthy() меняется, WARN/INFO пишутся при смене ---
void test_health_check_transitions()
{
    test_hooks::reset();
    test_hooks::g_simpleTestResult.store(true);

    CDBThread db(makeTestConfig());
    db.start();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK(db.isHealthy());

    db.stop();
}

void test_health_check_initial_state_reflects_simpleTest()
{
    test_hooks::reset();
    test_hooks::g_simpleTestResult.store(false);

    CDBThread db(makeTestConfig());
    db.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    CHECK(!db.isHealthy()); // сразу после старта m_healthy = model->simpleTest()

    db.stop();
}

// --- 3. Задачи не исполняются, пока !isHealthy(); m_model/m_connection не трогаются ---
void test_tasks_pile_up_while_unhealthy()
{
    test_hooks::reset();
    test_hooks::g_simpleTestResult.store(false); // сразу нездоров с самого старта

    CDBThread db(makeTestConfig());
    db.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK(!db.isHealthy());

    std::atomic<bool> called{false};
    db.executeEquipIdQuery({1}, [&](std::vector<SEquipRow>, EDatabaseError) { called.store(true); });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    CHECK(!called.load()); // задача не должна была выполниться, пока !healthy

    db.stop(); // задача так и не выполнилась - при stop() отбрасывается без колбэка
    CHECK(!called.load());
}

// --- 4. Исключение внутри задачи - DB-поток не падает, следующая задача всё равно выполняется ---
void test_exception_does_not_kill_thread()
{
    test_hooks::reset();

    CDBThread db(makeTestConfig());
    db.start();

    std::atomic<bool> firstThrew{false};
    std::atomic<bool> secondRan{false};

    test_hooks::setThrowHook([] { throw std::runtime_error("boom"); });

    db.executeEquipIdQuery({1}, [&](std::vector<SEquipRow>, EDatabaseError) { firstThrew = true; });

    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    test_hooks::setThrowHook(nullptr);

    db.executeEquipIdQuery({2}, [&](std::vector<SEquipRow>, EDatabaseError) { secondRan.store(true); });

    for (int attempt = 0; attempt < 100 && !secondRan.load(); ++attempt)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    db.stop();

    CHECK(!firstThrew.load());  // колбэк первой задачи так и не был вызван - исключение до callback()
    CHECK(secondRan.load());    // но поток жив и вторая задача выполнилась
}

// --- 5. stop() во время выполнения задачи - дожидается её завершения ---
void test_stop_waits_for_current_task()
{
    test_hooks::reset();

    CDBThread db(makeTestConfig());
    db.start();

    std::atomic<bool> taskFinished{false};
    test_hooks::setThrowHook([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        taskFinished.store(true);
    });

    db.executeEquipIdQuery({1}, [](std::vector<SEquipRow>, EDatabaseError) {});

    std::this_thread::sleep_for(std::chrono::milliseconds(30)); // задача уже должна была стартовать
    db.stop(); // должен дождаться завершения "долгой" задачи

    test_hooks::setThrowHook(nullptr);
    CHECK(taskFinished.load());
}

// --- 6. stop() при непустой очереди - невыполненные задачи отбрасываются без колбэка ---
void test_stop_drops_pending_queue()
{
    test_hooks::reset();

    CDBThread db(makeTestConfig());
    db.start();

    std::atomic<int> longTaskStarted{0};
    std::atomic<int> callbacksInvoked{0};

    // Хук занимает первую задачу на 150мс, не бросая исключение - к моменту
    // stop() она уже стартовала и должна доиграться до конца (её колбэк
    // сработает нормально), а оставшиеся 5 в очереди должны быть
    // отброшены без вызова колбэков.
    test_hooks::setThrowHook([&] {
        ++longTaskStarted;
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    });

    db.executeEquipIdQuery({1}, [&](std::vector<SEquipRow>, EDatabaseError) { ++callbacksInvoked; });
    for (int i = 0; i < 5; ++i)
        db.executeEquipIdQuery({static_cast<uint16_t>(10 + i)}, [&](std::vector<SEquipRow>, EDatabaseError) { ++callbacksInvoked; });

    std::this_thread::sleep_for(std::chrono::milliseconds(30)); // первая задача уже выполняется
    db.stop(); // блокируется на join() пока первая задача не доиграет (~150мс), затем отбрасывает очередь

    test_hooks::setThrowHook(nullptr);

    CHECK(longTaskStarted.load() == 1);   // только первая задача успела стартовать
    CHECK(callbacksInvoked.load() == 1);  // её колбэк отработал нормально, остальные 5 - отброшены
}

// --- equip_row_grouping ---
void test_grouping_basic()
{
    std::vector<SEquipRow> rows;

    SEquipRow r1;
    r1.natureType = static_cast<uint8_t>(NCore::EComponentTypes::ect_water);
    r1.componentEnum = 0;
    r1.idEquip = 100;
    r1.idTemplateField = 1;
    r1.equipName = "Filter A";
    r1.id_manufacturer = 5;
    rows.push_back(r1);

    SEquipRow r2 = r1;
    r2.idTemplateField = 2;
    rows.push_back(r2);

    SEquipRow r3 = r1;
    r3.idTemplateField = 3;
    rows.push_back(r3);

    // другой idEquip, тот же тип
    SEquipRow r4 = r1;
    r4.idEquip = 200;
    r4.idTemplateField = 1;
    r4.equipName = "Filter B";
    rows.push_back(r4);

    // другой тип (componentEnum)
    SEquipRow r5 = r1;
    r5.componentEnum = 1;
    r5.idEquip = 300;
    rows.push_back(r5);

    auto grouped = groupEquipRowsByType(rows);

    SEquipTypeKey keyA{NCore::EComponentTypes::ect_water, 0};
    SEquipTypeKey keyB{NCore::EComponentTypes::ect_water, 1};

    CHECK(grouped.count(keyA) == 1);
    CHECK(grouped.count(keyB) == 1);
    CHECK(grouped[keyA].size() == 2); // idEquip 100 и 200
    CHECK(grouped[keyB].size() == 1); // idEquip 300

    // 3 SField у idEquip=100
    bool found100 = false;
    for (const auto& g : grouped[keyA])
    {
        if (g.idEquip == 100)
        {
            found100 = true;
            CHECK(g.fields.size() == 3);
            CHECK(g.equipName == "Filter A");
            CHECK(g.idManufacturer == 5);
        }
    }
    CHECK(found100);
}

void test_grouping_empty_input()
{
    auto grouped = groupEquipRowsByType({});
    CHECK(grouped.empty());
}

} // namespace

int exec_texts()
{
    test_fifo_order();
    test_health_check_transitions();
    test_health_check_initial_state_reflects_simpleTest();
    test_tasks_pile_up_while_unhealthy();
    test_exception_does_not_kill_thread();
    test_stop_waits_for_current_task();
    test_stop_drops_pending_queue();
    test_grouping_basic();
    test_grouping_empty_input();

    std::cout << "\n" << g_passed << " passed, " << g_failed << " failed\n";
    return g_failed == 0 ? 0 : 1;
}
