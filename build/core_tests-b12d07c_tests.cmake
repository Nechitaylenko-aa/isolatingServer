add_test( [==[CDbManager creates database file and accepts DDL]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/core_tests [==[CDbManager creates database file and accepts DDL]==]  )
set_tests_properties( [==[CDbManager creates database file and accepts DDL]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[CDbManager enables WAL journal mode]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/core_tests [==[CDbManager enables WAL journal mode]==]  )
set_tests_properties( [==[CDbManager enables WAL journal mode]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[CDbManager move transfers the connection]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/core_tests [==[CDbManager move transfers the connection]==]  )
set_tests_properties( [==[CDbManager move transfers the connection]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[CDbManager persists across reopen]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/core_tests [==[CDbManager persists across reopen]==]  )
set_tests_properties( [==[CDbManager persists across reopen]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[CDbManager prepares and binds values]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/core_tests [==[CDbManager prepares and binds values]==]  )
set_tests_properties( [==[CDbManager prepares and binds values]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[CDbManager reports error instead of crashing on bad path]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/core_tests [==[CDbManager reports error instead of crashing on bad path]==]  )
set_tests_properties( [==[CDbManager reports error instead of crashing on bad path]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[CDbManager rolls back an aborted transaction]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/core_tests [==[CDbManager rolls back an aborted transaction]==]  )
set_tests_properties( [==[CDbManager rolls back an aborted transaction]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[CDbManager statement reset allows reuse]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/core_tests [==[CDbManager statement reset allows reuse]==]  )
set_tests_properties( [==[CDbManager statement reset allows reuse]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[CTuCache does not invalidate an unrelated TU]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/core_tests [==[CTuCache does not invalidate an unrelated TU]==]  )
set_tests_properties( [==[CTuCache does not invalidate an unrelated TU]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[CTuCache invalidates on compiler flags change]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/core_tests [==[CTuCache invalidates on compiler flags change]==]  )
set_tests_properties( [==[CTuCache invalidates on compiler flags change]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[CTuCache invalidates the TU that includes the edited header]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/core_tests [==[CTuCache invalidates the TU that includes the edited header]==]  )
set_tests_properties( [==[CTuCache invalidates the TU that includes the edited header]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[CTuCache opening the same fresh database twice does not loop or wipe]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/core_tests [==[CTuCache opening the same fresh database twice does not loop or wipe]==]  )
set_tests_properties( [==[CTuCache opening the same fresh database twice does not loop or wipe]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[CTuCache opens and creates schema]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/core_tests [==[CTuCache opens and creates schema]==]  )
set_tests_properties( [==[CTuCache opens and creates schema]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[CTuCache reports a project-level error when compile_commands is missing]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/core_tests [==[CTuCache reports a project-level error when compile_commands is missing]==]  )
set_tests_properties( [==[CTuCache reports a project-level error when compile_commands is missing]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[CTuCache returns zero refs for unknown USR without error]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/core_tests [==[CTuCache returns zero refs for unknown USR without error]==]  )
set_tests_properties( [==[CTuCache returns zero refs for unknown USR without error]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[CTuCache scans project once, then serves from cache]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/core_tests [==[CTuCache scans project once\, then serves from cache]==]  )
set_tests_properties( [==[CTuCache scans project once, then serves from cache]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[CTuCache sees a new reference added via the header-including TU]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/core_tests [==[CTuCache sees a new reference added via the header-including TU]==]  )
set_tests_properties( [==[CTuCache sees a new reference added via the header-including TU]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[CTuCache survives reopen of the database]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/core_tests [==[CTuCache survives reopen of the database]==]  )
set_tests_properties( [==[CTuCache survives reopen of the database]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[CTuCache treats a deleted tracked header as a dirty TU, not a crash]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/core_tests [==[CTuCache treats a deleted tracked header as a dirty TU\, not a crash]==]  )
set_tests_properties( [==[CTuCache treats a deleted tracked header as a dirty TU, not a crash]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[CTuCache wipes everything when libclang version in meta differs]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/core_tests [==[CTuCache wipes everything when libclang version in meta differs]==]  )
set_tests_properties( [==[CTuCache wipes everything when libclang version in meta differs]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[CTuCache wipes everything when schema version differs]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/core_tests [==[CTuCache wipes everything when schema version differs]==]  )
set_tests_properties( [==[CTuCache wipes everything when schema version differs]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[fnv1a_64 matches known vectors]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/core_tests [==[fnv1a_64 matches known vectors]==]  )
set_tests_properties( [==[fnv1a_64 matches known vectors]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[hash_file on missing file is empty, not a valid hash]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/core_tests [==[hash_file on missing file is empty\, not a valid hash]==]  )
set_tests_properties( [==[hash_file on missing file is empty, not a valid hash]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[hash_file reads actual content]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/core_tests [==[hash_file reads actual content]==]  )
set_tests_properties( [==[hash_file reads actual content]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[hash_flags separates arguments]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/core_tests [==[hash_flags separates arguments]==]  )
set_tests_properties( [==[hash_flags separates arguments]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[hash_string differs for UTF-8 multibyte input]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/core_tests [==[hash_string differs for UTF-8 multibyte input]==]  )
set_tests_properties( [==[hash_string differs for UTF-8 multibyte input]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[hash_string returns stable 16-char hex]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/core_tests [==[hash_string returns stable 16-char hex]==]  )
set_tests_properties( [==[hash_string returns stable 16-char hex]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
set( core_tests_TESTS [==[
  {
    "class-name" : "",
    "name" : "CDbManager creates database file and accepts DDL",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/core/tests/test_db_manager.cpp",
      "line" : 41
    },
    "tags" :  "db" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "CDbManager enables WAL journal mode",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/core/tests/test_db_manager.cpp",
      "line" : 138
    },
    "tags" :  "db" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "CDbManager move transfers the connection",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/core/tests/test_db_manager.cpp",
      "line" : 166
    },
    "tags" :  "db" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "CDbManager persists across reopen",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/core/tests/test_db_manager.cpp",
      "line" : 120
    },
    "tags" :  "db" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "CDbManager prepares and binds values",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/core/tests/test_db_manager.cpp",
      "line" : 53
    },
    "tags" :  "db" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "CDbManager reports error instead of crashing on bad path",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/core/tests/test_db_manager.cpp",
      "line" : 151
    },
    "tags" :  "db" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "CDbManager rolls back an aborted transaction",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/core/tests/test_db_manager.cpp",
      "line" : 96
    },
    "tags" :  "db" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "CDbManager statement reset allows reuse",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/core/tests/test_db_manager.cpp",
      "line" : 75
    },
    "tags" :  "db" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "CTuCache does not invalidate an unrelated TU",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/core/tests/test_tu_cache.cpp",
      "line" : 277
    },
    "tags" :  "cache" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "CTuCache invalidates on compiler flags change",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/core/tests/test_tu_cache.cpp",
      "line" : 296
    },
    "tags" :  "cache" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "CTuCache invalidates the TU that includes the edited header",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/core/tests/test_tu_cache.cpp",
      "line" : 237
    },
    "tags" :  "cache" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "CTuCache opening the same fresh database twice does not loop or wipe",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/core/tests/test_tu_cache.cpp",
      "line" : 412
    },
    "tags" :  "cache" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "CTuCache opens and creates schema",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/core/tests/test_tu_cache.cpp",
      "line" : 177
    },
    "tags" :  "cache" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "CTuCache reports a project-level error when compile_commands is missing",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/core/tests/test_tu_cache.cpp",
      "line" : 345
    },
    "tags" :  "cache" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "CTuCache returns zero refs for unknown USR without error",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/core/tests/test_tu_cache.cpp",
      "line" : 331
    },
    "tags" :  "cache" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "CTuCache scans project once, then serves from cache",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/core/tests/test_tu_cache.cpp",
      "line" : 186
    },
    "tags" :  "cache" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "CTuCache sees a new reference added via the header-including TU",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/core/tests/test_tu_cache.cpp",
      "line" : 255
    },
    "tags" :  "cache" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "CTuCache survives reopen of the database",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/core/tests/test_tu_cache.cpp",
      "line" : 210
    },
    "tags" :  "cache" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "CTuCache treats a deleted tracked header as a dirty TU, not a crash",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/core/tests/test_tu_cache.cpp",
      "line" : 315
    },
    "tags" :  "cache" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "CTuCache wipes everything when libclang version in meta differs",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/core/tests/test_tu_cache.cpp",
      "line" : 358
    },
    "tags" :  "cache" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "CTuCache wipes everything when schema version differs",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/core/tests/test_tu_cache.cpp",
      "line" : 386
    },
    "tags" :  "cache" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "fnv1a_64 matches known vectors",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/core/tests/test_hash.cpp",
      "line" : 12
    },
    "tags" :  "hash" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "hash_file on missing file is empty, not a valid hash",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/core/tests/test_hash.cpp",
      "line" : 60
    },
    "tags" :  "hash" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "hash_file reads actual content",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/core/tests/test_hash.cpp",
      "line" : 36
    },
    "tags" :  "hash" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "hash_flags separates arguments",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/core/tests/test_hash.cpp",
      "line" : 68
    },
    "tags" :  "hash" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "hash_string differs for UTF-8 multibyte input",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/core/tests/test_hash.cpp",
      "line" : 29
    },
    "tags" :  "hash" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "hash_string returns stable 16-char hex",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/core/tests/test_hash.cpp",
      "line" : 20
    },
    "tags" :  "hash" 
  }
]==] [==[CDbManager creates database file and accepts DDL]==] [==[CDbManager enables WAL journal mode]==] [==[CDbManager move transfers the connection]==] [==[CDbManager persists across reopen]==] [==[CDbManager prepares and binds values]==] [==[CDbManager reports error instead of crashing on bad path]==] [==[CDbManager rolls back an aborted transaction]==] [==[CDbManager statement reset allows reuse]==] [==[CTuCache does not invalidate an unrelated TU]==] [==[CTuCache invalidates on compiler flags change]==] [==[CTuCache invalidates the TU that includes the edited header]==] [==[CTuCache opening the same fresh database twice does not loop or wipe]==] [==[CTuCache opens and creates schema]==] [==[CTuCache reports a project-level error when compile_commands is missing]==] [==[CTuCache returns zero refs for unknown USR without error]==] [==[CTuCache scans project once, then serves from cache]==] [==[CTuCache sees a new reference added via the header-including TU]==] [==[CTuCache survives reopen of the database]==] [==[CTuCache treats a deleted tracked header as a dirty TU, not a crash]==] [==[CTuCache wipes everything when libclang version in meta differs]==] [==[CTuCache wipes everything when schema version differs]==] [==[fnv1a_64 matches known vectors]==] [==[hash_file on missing file is empty, not a valid hash]==] [==[hash_file reads actual content]==] [==[hash_flags separates arguments]==] [==[hash_string differs for UTF-8 multibyte input]==] [==[hash_string returns stable 16-char hex]==])
