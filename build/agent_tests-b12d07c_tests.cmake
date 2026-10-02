add_test( [==[ExecuteService apply: empty edits -> ok no checkpoint]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[ExecuteService apply: empty edits -> ok no checkpoint]==]  )
set_tests_properties( [==[ExecuteService apply: empty edits -> ok no checkpoint]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[ExecuteService apply: empty oldText reports insert]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[ExecuteService apply: empty oldText reports insert]==]  )
set_tests_properties( [==[ExecuteService apply: empty oldText reports insert]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[ExecuteService apply: exact match reports exact]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[ExecuteService apply: exact match reports exact]==]  )
set_tests_properties( [==[ExecuteService apply: exact match reports exact]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[ExecuteService apply: failure on second edit must not leave first file changed]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[ExecuteService apply: failure on second edit must not leave first file changed]==]  )
set_tests_properties( [==[ExecuteService apply: failure on second edit must not leave first file changed]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[ExecuteService apply: file not found -> fail]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[ExecuteService apply: file not found -> fail]==]  )
set_tests_properties( [==[ExecuteService apply: file not found -> fail]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[ExecuteService apply: fuzzy match on whitespace differences]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[ExecuteService apply: fuzzy match on whitespace differences]==]  )
set_tests_properties( [==[ExecuteService apply: fuzzy match on whitespace differences]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[ExecuteService apply: fuzzy prefix match when model collapses newlines]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[ExecuteService apply: fuzzy prefix match when model collapses newlines]==]  )
set_tests_properties( [==[ExecuteService apply: fuzzy prefix match when model collapses newlines]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[ExecuteService apply: hintLine 0 keeps first-occurrence behaviour]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[ExecuteService apply: hintLine 0 keeps first-occurrence behaviour]==]  )
set_tests_properties( [==[ExecuteService apply: hintLine 0 keeps first-occurrence behaviour]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[ExecuteService apply: insert when oldText empty]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[ExecuteService apply: insert when oldText empty]==]  )
set_tests_properties( [==[ExecuteService apply: insert when oldText empty]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[ExecuteService apply: multi-file]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[ExecuteService apply: multi-file]==]  )
set_tests_properties( [==[ExecuteService apply: multi-file]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[ExecuteService apply: oldText not found -> fail]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[ExecuteService apply: oldText not found -> fail]==]  )
set_tests_properties( [==[ExecuteService apply: oldText not found -> fail]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[ExecuteService apply: picks occurrence nearest to hintLine, not first]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[ExecuteService apply: picks occurrence nearest to hintLine\, not first]==]  )
set_tests_properties( [==[ExecuteService apply: picks occurrence nearest to hintLine, not first]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[ExecuteService apply: preserves relative inner indent in multi-line newText]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[ExecuteService apply: preserves relative inner indent in multi-line newText]==]  )
set_tests_properties( [==[ExecuteService apply: preserves relative inner indent in multi-line newText]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[ExecuteService apply: reindents newText when model drops leading indent]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[ExecuteService apply: reindents newText when model drops leading indent]==]  )
set_tests_properties( [==[ExecuteService apply: reindents newText when model drops leading indent]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[ExecuteService apply: removes excess indent when oldText over-indented]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[ExecuteService apply: removes excess indent when oldText over-indented]==]  )
set_tests_properties( [==[ExecuteService apply: removes excess indent when oldText over-indented]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[ExecuteService apply: replace oldText]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[ExecuteService apply: replace oldText]==]  )
set_tests_properties( [==[ExecuteService apply: replace oldText]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[ExecuteService undo: unknown id -> fail]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[ExecuteService undo: unknown id -> fail]==]  )
set_tests_properties( [==[ExecuteService undo: unknown id -> fail]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[GroundService: USR anchor survives line shifts]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[GroundService: USR anchor survives line shifts]==]  )
set_tests_properties( [==[GroundService: USR anchor survives line shifts]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[HttpMainModelBroker: generate plan with real model]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[HttpMainModelBroker: generate plan with real model]==]  )
set_tests_properties( [==[HttpMainModelBroker: generate plan with real model]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[LightModelBroker: classify with real model]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[LightModelBroker: classify with real model]==]  )
set_tests_properties( [==[LightModelBroker: classify with real model]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: 'только проверь' vetoes write primitive]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: 'только проверь' vetoes write primitive]==]  )
set_tests_properties( [==[Orchestrator: 'только проверь' vetoes write primitive]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: INFRA must report failure on real non-zero build]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: INFRA must report failure on real non-zero build]==]  )
set_tests_properties( [==[Orchestrator: INFRA must report failure on real non-zero build]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: INFRA without cursor]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: INFRA without cursor]==]  )
set_tests_properties( [==[Orchestrator: INFRA without cursor]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: InfraHandler build with real buildDir]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: InfraHandler build with real buildDir]==]  )
set_tests_properties( [==[Orchestrator: InfraHandler build with real buildDir]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: InfraHandler graph targets]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: InfraHandler graph targets]==]  )
set_tests_properties( [==[Orchestrator: InfraHandler graph targets]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: SANDBOX_FIX on tiny project passes without confirmation (adaptive thresholds)]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: SANDBOX_FIX on tiny project passes without confirmation (adaptive thresholds)]==]  )
set_tests_properties( [==[Orchestrator: SANDBOX_FIX on tiny project passes without confirmation (adaptive thresholds)]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: SANDBOX_FIX triggers blast_radius needs_input with explicit low thresholds]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: SANDBOX_FIX triggers blast_radius needs_input with explicit low thresholds]==]  )
set_tests_properties( [==[Orchestrator: SANDBOX_FIX triggers blast_radius needs_input with explicit low thresholds]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: SANDBOX_FIX with Stub models]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: SANDBOX_FIX with Stub models]==]  )
set_tests_properties( [==[Orchestrator: SANDBOX_FIX with Stub models]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: UNDERSTAND with Stub model]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: UNDERSTAND with Stub model]==]  )
set_tests_properties( [==[Orchestrator: UNDERSTAND with Stub model]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: blast_radius wide warning]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: blast_radius wide warning]==]  )
set_tests_properties( [==[Orchestrator: blast_radius wide warning]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: clarification cycle blast_radius -> cancel]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: clarification cycle blast_radius -> cancel]==]  )
set_tests_properties( [==[Orchestrator: clarification cycle blast_radius -> cancel]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: clarification cycle blast_radius -> proceed -> answer]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: clarification cycle blast_radius -> proceed -> answer]==]  )
set_tests_properties( [==[Orchestrator: clarification cycle blast_radius -> proceed -> answer]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: clarification cycle depth protection]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: clarification cycle depth protection]==]  )
set_tests_properties( [==[Orchestrator: clarification cycle depth protection]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: clarification replay protection]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: clarification replay protection]==]  )
set_tests_properties( [==[Orchestrator: clarification replay protection]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: dead model + INFRA phrase -> answer via stub (no missing_cursor)]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: dead model + INFRA phrase -> answer via stub (no missing_cursor)]==]  )
set_tests_properties( [==[Orchestrator: dead model + INFRA phrase -> answer via stub (no missing_cursor)]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: dead model + clarification proceed -> writes]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: dead model + clarification proceed -> writes]==]  )
set_tests_properties( [==[Orchestrator: dead model + clarification proceed -> writes]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: dead model + clarification wait -> error model_unavailable_wait]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: dead model + clarification wait -> error model_unavailable_wait]==]  )
set_tests_properties( [==[Orchestrator: dead model + clarification wait -> error model_unavailable_wait]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: dead model + question -> answer via stub UNDERSTAND]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: dead model + question -> answer via stub UNDERSTAND]==]  )
set_tests_properties( [==[Orchestrator: dead model + question -> answer via stub UNDERSTAND]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: dead model + write phrase -> needs_input model-unavailable]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: dead model + write phrase -> needs_input model-unavailable]==]  )
set_tests_properties( [==[Orchestrator: dead model + write phrase -> needs_input model-unavailable]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: explicit 'без правок' vetoes write primitive -> UNDERSTAND]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: explicit 'без правок' vetoes write primitive -> UNDERSTAND]==]  )
set_tests_properties( [==[Orchestrator: explicit 'без правок' vetoes write primitive -> UNDERSTAND]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: failed main plan must not produce green verify]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: failed main plan must not produce green verify]==]  )
set_tests_properties( [==[Orchestrator: failed main plan must not produce green verify]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: full cycle SANDBOX_FIX with real models]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: full cycle SANDBOX_FIX with real models]==]  )
set_tests_properties( [==[Orchestrator: full cycle SANDBOX_FIX with real models]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: full cycle UNDERSTAND with real light model]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: full cycle UNDERSTAND with real light model]==]  )
set_tests_properties( [==[Orchestrator: full cycle UNDERSTAND with real light model]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: needs_input on low confidence (manual simulation)]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: needs_input on low confidence (manual simulation)]==]  )
set_tests_properties( [==[Orchestrator: needs_input on low confidence (manual simulation)]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: vague goal answered -> goal_text replaced and proceeds]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: vague goal answered -> goal_text replaced and proceeds]==]  )
set_tests_properties( [==[Orchestrator: vague goal answered -> goal_text replaced and proceeds]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: vague goal for UNDERSTAND -> no blocking]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: vague goal for UNDERSTAND -> no blocking]==]  )
set_tests_properties( [==[Orchestrator: vague goal for UNDERSTAND -> no blocking]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: vague goal for write primitive -> needs_input goal-vague]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: vague goal for write primitive -> needs_input goal-vague]==]  )
set_tests_properties( [==[Orchestrator: vague goal for write primitive -> needs_input goal-vague]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: write goal WITHOUT no-edit phrase is not vetoed]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: write goal WITHOUT no-edit phrase is not vetoed]==]  )
set_tests_properties( [==[Orchestrator: write goal WITHOUT no-edit phrase is not vetoed]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[StubModelBroker: capitalized Cyrillic reading verb -> UNDERSTAND]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[StubModelBroker: capitalized Cyrillic reading verb -> UNDERSTAND]==]  )
set_tests_properties( [==[StubModelBroker: capitalized Cyrillic reading verb -> UNDERSTAND]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[StubModelBroker: capitalized Cyrillic write verb still writes]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[StubModelBroker: capitalized Cyrillic write verb still writes]==]  )
set_tests_properties( [==[StubModelBroker: capitalized Cyrillic write verb still writes]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[primitiveFromString: model casing/whitespace are tolerated]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[primitiveFromString: model casing/whitespace are tolerated]==]  )
set_tests_properties( [==[primitiveFromString: model casing/whitespace are tolerated]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
set( agent_tests_TESTS [==[
  {
    "class-name" : "",
    "name" : "ExecuteService apply: empty edits -> ok no checkpoint",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_execute_service.cpp",
      "line" : 76
    },
    "tags" :  "execute" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "ExecuteService apply: empty oldText reports insert",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_execute_service.cpp",
      "line" : 139
    },
    "tags" :  "execute", "fuzzy" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "ExecuteService apply: exact match reports exact",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_execute_service.cpp",
      "line" : 124
    },
    "tags" :  "execute", "fuzzy" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "ExecuteService apply: failure on second edit must not leave first file changed",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_execute_service.cpp",
      "line" : 169
    },
    "tags" :  "atomic", "execute" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "ExecuteService apply: file not found -> fail",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_execute_service.cpp",
      "line" : 65
    },
    "tags" :  "execute" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "ExecuteService apply: fuzzy match on whitespace differences",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_execute_service.cpp",
      "line" : 91
    },
    "tags" :  "execute", "fuzzy" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "ExecuteService apply: fuzzy prefix match when model collapses newlines",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_execute_service.cpp",
      "line" : 106
    },
    "tags" :  "execute", "fuzzy" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "ExecuteService apply: hintLine 0 keeps first-occurrence behaviour",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_execute_service.cpp",
      "line" : 293
    },
    "tags" :  "execute", "fuzzy", "hint" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "ExecuteService apply: insert when oldText empty",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_execute_service.cpp",
      "line" : 37
    },
    "tags" :  "execute" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "ExecuteService apply: multi-file",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_execute_service.cpp",
      "line" : 154
    },
    "tags" :  "execute" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "ExecuteService apply: oldText not found -> fail",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_execute_service.cpp",
      "line" : 52
    },
    "tags" :  "execute" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "ExecuteService apply: picks occurrence nearest to hintLine, not first",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_execute_service.cpp",
      "line" : 187
    },
    "tags" :  "execute", "fuzzy", "hint" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "ExecuteService apply: preserves relative inner indent in multi-line newText",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_execute_service.cpp",
      "line" : 266
    },
    "tags" :  "execute", "indent" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "ExecuteService apply: reindents newText when model drops leading indent",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_execute_service.cpp",
      "line" : 223
    },
    "tags" :  "execute", "indent" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "ExecuteService apply: removes excess indent when oldText over-indented",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_execute_service.cpp",
      "line" : 246
    },
    "tags" :  "execute", "indent" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "ExecuteService apply: replace oldText",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_execute_service.cpp",
      "line" : 18
    },
    "tags" :  "execute" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "ExecuteService undo: unknown id -> fail",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_execute_service.cpp",
      "line" : 84
    },
    "tags" :  "execute" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "GroundService: USR anchor survives line shifts",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 838
    },
    "tags" :  "anchor", "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "HttpMainModelBroker: generate plan with real model",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 507
    },
    "tags" :  "!mayfail", "live", "orchestrator" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "LightModelBroker: classify with real model",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 491
    },
    "tags" :  "!mayfail", "live", "orchestrator" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: '\u0442\u043e\u043b\u044c\u043a\u043e \u043f\u0440\u043e\u0432\u0435\u0440\u044c' vetoes write primitive",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 736
    },
    "tags" :  "orchestrator", "stub", "veto" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: INFRA must report failure on real non-zero build",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 647
    },
    "tags" :  "infrafail", "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: INFRA without cursor",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 189
    },
    "tags" :  "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: InfraHandler build with real buildDir",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 369
    },
    "tags" :  "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: InfraHandler graph targets",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 393
    },
    "tags" :  "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: SANDBOX_FIX on tiny project passes without confirmation (adaptive thresholds)",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 129
    },
    "tags" :  "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: SANDBOX_FIX triggers blast_radius needs_input with explicit low thresholds",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 155
    },
    "tags" :  "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: SANDBOX_FIX with Stub models",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 101
    },
    "tags" :  "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: UNDERSTAND with Stub model",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 79
    },
    "tags" :  "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: blast_radius wide warning",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 234
    },
    "tags" :  "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: clarification cycle blast_radius -> cancel",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 297
    },
    "tags" :  "clarify", "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: clarification cycle blast_radius -> proceed -> answer",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 261
    },
    "tags" :  "clarify", "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: clarification cycle depth protection",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 318
    },
    "tags" :  "clarify", "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: clarification replay protection",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 345
    },
    "tags" :  "clarify", "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: dead model + INFRA phrase -> answer via stub (no missing_cursor)",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 412
    },
    "tags" :  "fallback", "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: dead model + clarification proceed -> writes",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 458
    },
    "tags" :  "fallback", "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: dead model + clarification wait -> error model_unavailable_wait",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 474
    },
    "tags" :  "fallback", "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: dead model + question -> answer via stub UNDERSTAND",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 425
    },
    "tags" :  "fallback", "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: dead model + write phrase -> needs_input model-unavailable",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 439
    },
    "tags" :  "fallback", "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: explicit '\u0431\u0435\u0437 \u043f\u0440\u0430\u0432\u043e\u043a' vetoes write primitive -> UNDERSTAND",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 713
    },
    "tags" :  "orchestrator", "stub", "veto" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: failed main plan must not produce green verify",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 627
    },
    "tags" :  "orchestrator", "planfail", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: full cycle SANDBOX_FIX with real models",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 561
    },
    "tags" :  "!mayfail", "live", "orchestrator" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: full cycle UNDERSTAND with real light model",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 531
    },
    "tags" :  "!mayfail", "live", "orchestrator" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: needs_input on low confidence (manual simulation)",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 206
    },
    "tags" :  "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: vague goal answered -> goal_text replaced and proceeds",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 796
    },
    "tags" :  "goalspec", "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: vague goal for UNDERSTAND -> no blocking",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 820
    },
    "tags" :  "goalspec", "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: vague goal for write primitive -> needs_input goal-vague",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 773
    },
    "tags" :  "goalspec", "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: write goal WITHOUT no-edit phrase is not vetoed",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 752
    },
    "tags" :  "orchestrator", "stub", "veto" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "StubModelBroker: capitalized Cyrillic reading verb -> UNDERSTAND",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 689
    },
    "tags" :  "orchestrator", "stub", "utf8" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "StubModelBroker: capitalized Cyrillic write verb still writes",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 704
    },
    "tags" :  "orchestrator", "stub", "utf8" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "primitiveFromString: model casing/whitespace are tolerated",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 673
    },
    "tags" :  "model", "unit" 
  }
]==] [==[ExecuteService apply: empty edits -> ok no checkpoint]==] [==[ExecuteService apply: empty oldText reports insert]==] [==[ExecuteService apply: exact match reports exact]==] [==[ExecuteService apply: failure on second edit must not leave first file changed]==] [==[ExecuteService apply: file not found -> fail]==] [==[ExecuteService apply: fuzzy match on whitespace differences]==] [==[ExecuteService apply: fuzzy prefix match when model collapses newlines]==] [==[ExecuteService apply: hintLine 0 keeps first-occurrence behaviour]==] [==[ExecuteService apply: insert when oldText empty]==] [==[ExecuteService apply: multi-file]==] [==[ExecuteService apply: oldText not found -> fail]==] [==[ExecuteService apply: picks occurrence nearest to hintLine, not first]==] [==[ExecuteService apply: preserves relative inner indent in multi-line newText]==] [==[ExecuteService apply: reindents newText when model drops leading indent]==] [==[ExecuteService apply: removes excess indent when oldText over-indented]==] [==[ExecuteService apply: replace oldText]==] [==[ExecuteService undo: unknown id -> fail]==] [==[GroundService: USR anchor survives line shifts]==] [==[HttpMainModelBroker: generate plan with real model]==] [==[LightModelBroker: classify with real model]==] [==[Orchestrator: 'только проверь' vetoes write primitive]==] [==[Orchestrator: INFRA must report failure on real non-zero build]==] [==[Orchestrator: INFRA without cursor]==] [==[Orchestrator: InfraHandler build with real buildDir]==] [==[Orchestrator: InfraHandler graph targets]==] [==[Orchestrator: SANDBOX_FIX on tiny project passes without confirmation (adaptive thresholds)]==] [==[Orchestrator: SANDBOX_FIX triggers blast_radius needs_input with explicit low thresholds]==] [==[Orchestrator: SANDBOX_FIX with Stub models]==] [==[Orchestrator: UNDERSTAND with Stub model]==] [==[Orchestrator: blast_radius wide warning]==] [==[Orchestrator: clarification cycle blast_radius -> cancel]==] [==[Orchestrator: clarification cycle blast_radius -> proceed -> answer]==] [==[Orchestrator: clarification cycle depth protection]==] [==[Orchestrator: clarification replay protection]==] [==[Orchestrator: dead model + INFRA phrase -> answer via stub (no missing_cursor)]==] [==[Orchestrator: dead model + clarification proceed -> writes]==] [==[Orchestrator: dead model + clarification wait -> error model_unavailable_wait]==] [==[Orchestrator: dead model + question -> answer via stub UNDERSTAND]==] [==[Orchestrator: dead model + write phrase -> needs_input model-unavailable]==] [==[Orchestrator: explicit 'без правок' vetoes write primitive -> UNDERSTAND]==] [==[Orchestrator: failed main plan must not produce green verify]==] [==[Orchestrator: full cycle SANDBOX_FIX with real models]==] [==[Orchestrator: full cycle UNDERSTAND with real light model]==] [==[Orchestrator: needs_input on low confidence (manual simulation)]==] [==[Orchestrator: vague goal answered -> goal_text replaced and proceeds]==] [==[Orchestrator: vague goal for UNDERSTAND -> no blocking]==] [==[Orchestrator: vague goal for write primitive -> needs_input goal-vague]==] [==[Orchestrator: write goal WITHOUT no-edit phrase is not vetoed]==] [==[StubModelBroker: capitalized Cyrillic reading verb -> UNDERSTAND]==] [==[StubModelBroker: capitalized Cyrillic write verb still writes]==] [==[primitiveFromString: model casing/whitespace are tolerated]==])
