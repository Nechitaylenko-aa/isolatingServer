// db/db_config.h
//
// Статический конфиг подключения к БД (database.md) + фабрика соединения.
// Единственное место в проекте, где фигурируют логин/пароль к БД - по
// решению Artem: захардкожено, не вынесено в отдельный файл конфигурации.

#pragma once

#include "db_types_fwd.h"   // SDBConnection
#include "../include/CDatabaseModel.h" // CAbstractConnection, E_DB_TYPE

// Заполняет config захардкоженными значениями подключения. Вызывается один
// раз при старте CDBThread (см. db_thread.cpp).
void setupDBconfig(SDBConnection& config);

// Создаёт соединение через фабрику CAbstractConnection. Реализация фабрики -
// вне зоны ответственности этой группы (см. database_model.h).
CAbstractConnection* createConnection(const SDBConnection& connect);
