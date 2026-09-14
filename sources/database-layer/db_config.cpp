// db/db_config.cpp
//
// Отклонение от буквального псевдокода в database.md/ТЗ: там createConnection
// объявлен как `static` внутри db_config.h. Со `static` (внутренняя линковка)
// функция была бы недоступна из db_thread.cpp при отдельной компиляции TU -
// пришлось бы либо инлайнить её в каждый .cpp, либо держать db_thread.cpp и
// db_config.cpp одной TU. Вместо этого - обычная функция с внешней линковкой,
// объявленная в db_config.h и определённая здесь. Поведение и сигнатура не
// меняются, меняется только спецификатор линковки.

#include "db_config.h"

void setupDBconfig(SDBConnection& config)
{
    // Жёстко зашитые значения (Artem: "статически прямо в коде забью").
    // Заполнить реальными host/user/pass/dbname перед сборкой прод-версии.
    config.host   = "192.168.1.101";  // TODO: production host
    config.dbname = "nymphaea";  // TODO: production dbname
    config.user   = "artem";  // TODO: production user
    config.pass   = "masterkey";  // TODO: production password
    config.port   = 3306;
    config.ssl_config.is_enabled = false; // шифрование соединения с БД не используется на этом этапе
}

CAbstractConnection* createConnection(const SDBConnection& connect)
{
    return CAbstractConnection::createDatabaseInstance(E_DB_TYPE::EDT_MYSQL, const_cast<SDBConnection *>(&connect));
}
