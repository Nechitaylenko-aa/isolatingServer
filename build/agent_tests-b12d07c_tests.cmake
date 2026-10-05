add_test( [==[CConfigReader: access policies default and override]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[CConfigReader: access policies default and override]==]  )
set_tests_properties( [==[CConfigReader: access policies default and override]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[CConfigReader: creates default agent.conf and projects when missing]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[CConfigReader: creates default agent.conf and projects when missing]==]  )
set_tests_properties( [==[CConfigReader: creates default agent.conf and projects when missing]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[CConfigReader: db_file override from projects wins]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[CConfigReader: db_file override from projects wins]==]  )
set_tests_properties( [==[CConfigReader: db_file override from projects wins]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[CConfigReader: light aliases to main when same_as_main]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[CConfigReader: light aliases to main when same_as_main]==]  )
set_tests_properties( [==[CConfigReader: light aliases to main when same_as_main]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[CConfigReader: model getters and refresh_from_config]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[CConfigReader: model getters and refresh_from_config]==]  )
set_tests_properties( [==[CConfigReader: model getters and refresh_from_config]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[CConfigReader: parses agent.conf-style key/value with comments]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[CConfigReader: parses agent.conf-style key/value with comments]==]  )
set_tests_properties( [==[CConfigReader: parses agent.conf-style key/value with comments]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[CConfigReader: parses remote endpoint (type/url/name/key)]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[CConfigReader: parses remote endpoint (type/url/name/key)]==]  )
set_tests_properties( [==[CConfigReader: parses remote endpoint (type/url/name/key)]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[CConfigReader: resolve --cc keeps build=parent, db deterministic]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[CConfigReader: resolve --cc keeps build=parent\, db deterministic]==]  )
set_tests_properties( [==[CConfigReader: resolve --cc keeps build=parent, db deterministic]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[CConfigReader: resolve --project auto-registers and maps db]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[CConfigReader: resolve --project auto-registers and maps db]==]  )
set_tests_properties( [==[CConfigReader: resolve --project auto-registers and maps db]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
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
add_test( [==[HttpMainModelBroker parses plan/actions via injected IApiAI]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[HttpMainModelBroker parses plan/actions via injected IApiAI]==]  )
set_tests_properties( [==[HttpMainModelBroker parses plan/actions via injected IApiAI]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[HttpMainModelBroker reports provider down]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[HttpMainModelBroker reports provider down]==]  )
set_tests_properties( [==[HttpMainModelBroker reports provider down]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[HttpMainModelBroker: generate plan with real model]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[HttpMainModelBroker: generate plan with real model]==]  )
set_tests_properties( [==[HttpMainModelBroker: generate plan with real model]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[LightModelBroker parses classifier JSON via injected IApiAI]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[LightModelBroker parses classifier JSON via injected IApiAI]==]  )
set_tests_properties( [==[LightModelBroker parses classifier JSON via injected IApiAI]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[LightModelBroker surfaces provider error as unavailable]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[LightModelBroker surfaces provider error as unavailable]==]  )
set_tests_properties( [==[LightModelBroker surfaces provider error as unavailable]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[LightModelBroker tolerates non-JSON and reports unavailable]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[LightModelBroker tolerates non-JSON and reports unavailable]==]  )
set_tests_properties( [==[LightModelBroker tolerates non-JSON and reports unavailable]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[LightModelBroker: classify with real model]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[LightModelBroker: classify with real model]==]  )
set_tests_properties( [==[LightModelBroker: classify with real model]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[LightModelBroker: real-model primitive accuracy on dataset]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[LightModelBroker: real-model primitive accuracy on dataset]==]  )
set_tests_properties( [==[LightModelBroker: real-model primitive accuracy on dataset]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[ModelProfile: probe measures JSON compliance and caches]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[ModelProfile: probe measures JSON compliance and caches]==]  )
set_tests_properties( [==[ModelProfile: probe measures JSON compliance and caches]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[ModelProfile: remote verdict overrides heuristic]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[ModelProfile: remote verdict overrides heuristic]==]  )
set_tests_properties( [==[ModelProfile: remote verdict overrides heuristic]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: 'только проверь' vetoes write primitive]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: 'только проверь' vetoes write primitive]==]  )
set_tests_properties( [==[Orchestrator: 'только проверь' vetoes write primitive]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: CALL_CHAIN lists callers via cached call graph]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: CALL_CHAIN lists callers via cached call graph]==]  )
set_tests_properties( [==[Orchestrator: CALL_CHAIN lists callers via cached call graph]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: CODE_SMELL heuristic on a method]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: CODE_SMELL heuristic on a method]==]  )
set_tests_properties( [==[Orchestrator: CODE_SMELL heuristic on a method]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: CursorBinding NEEDED_MISSING asks for cursor, retry proceeds]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: CursorBinding NEEDED_MISSING asks for cursor\, retry proceeds]==]  )
set_tests_properties( [==[Orchestrator: CursorBinding NEEDED_MISSING asks for cursor, retry proceeds]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: CursorBinding NOT_NEEDED skips Ground, answers without cursor]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: CursorBinding NOT_NEEDED skips Ground\, answers without cursor]==]  )
set_tests_properties( [==[Orchestrator: CursorBinding NOT_NEEDED skips Ground, answers without cursor]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: DEPENDENCY_MAP reports file dependents]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: DEPENDENCY_MAP reports file dependents]==]  )
set_tests_properties( [==[Orchestrator: DEPENDENCY_MAP reports file dependents]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: GENERAL_QUESTION is delegated read-only to main model]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: GENERAL_QUESTION is delegated read-only to main model]==]  )
set_tests_properties( [==[Orchestrator: GENERAL_QUESTION is delegated read-only to main model]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: INFRA must report failure on real non-zero build]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: INFRA must report failure on real non-zero build]==]  )
set_tests_properties( [==[Orchestrator: INFRA must report failure on real non-zero build]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: INFRA without cursor]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: INFRA without cursor]==]  )
set_tests_properties( [==[Orchestrator: INFRA without cursor]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: InfraHandler build with real buildDir]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: InfraHandler build with real buildDir]==]  )
set_tests_properties( [==[Orchestrator: InfraHandler build with real buildDir]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: InfraHandler graph targets]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: InfraHandler graph targets]==]  )
set_tests_properties( [==[Orchestrator: InfraHandler graph targets]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: NOT_NEEDED normalizes method-bound Detail to GENERAL_QUESTION]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: NOT_NEEDED normalizes method-bound Detail to GENERAL_QUESTION]==]  )
set_tests_properties( [==[Orchestrator: NOT_NEEDED normalizes method-bound Detail to GENERAL_QUESTION]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: SANDBOX_FIX on tiny project passes without confirmation (adaptive thresholds)]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: SANDBOX_FIX on tiny project passes without confirmation (adaptive thresholds)]==]  )
set_tests_properties( [==[Orchestrator: SANDBOX_FIX on tiny project passes without confirmation (adaptive thresholds)]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: SANDBOX_FIX triggers blast_radius needs_input with explicit low thresholds]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: SANDBOX_FIX triggers blast_radius needs_input with explicit low thresholds]==]  )
set_tests_properties( [==[Orchestrator: SANDBOX_FIX triggers blast_radius needs_input with explicit low thresholds]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: SANDBOX_FIX with Stub models]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: SANDBOX_FIX with Stub models]==]  )
set_tests_properties( [==[Orchestrator: SANDBOX_FIX with Stub models]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[Orchestrator: TODO_SCAN finds markers deterministically]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: TODO_SCAN finds markers deterministically]==]  )
set_tests_properties( [==[Orchestrator: TODO_SCAN finds markers deterministically]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
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
add_test( [==[Orchestrator: cursor on method A while goal names method B -> needs_input relevance]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: cursor on method A while goal names method B -> needs_input relevance]==]  )
set_tests_properties( [==[Orchestrator: cursor on method A while goal names method B -> needs_input relevance]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
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
add_test( [==[Orchestrator: real project (reminder) UNDERSTAND on live model]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[Orchestrator: real project (reminder) UNDERSTAND on live model]==]  )
set_tests_properties( [==[Orchestrator: real project (reminder) UNDERSTAND on live model]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
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
add_test( [==[StubModelBroker: fills Detail along with Primitive]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[StubModelBroker: fills Detail along with Primitive]==]  )
set_tests_properties( [==[StubModelBroker: fills Detail along with Primitive]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[computeRelevance: GENERAL_QUESTION is cursor-irrelevant (priority over NO_CONTEXT)]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[computeRelevance: GENERAL_QUESTION is cursor-irrelevant (priority over NO_CONTEXT)]==]  )
set_tests_properties( [==[computeRelevance: GENERAL_QUESTION is cursor-irrelevant (priority over NO_CONTEXT)]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[computeRelevance: GENERAL_QUESTION is never forced UNRELATED by method name]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[computeRelevance: GENERAL_QUESTION is never forced UNRELATED by method name]==]  )
set_tests_properties( [==[computeRelevance: GENERAL_QUESTION is never forced UNRELATED by method name]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[computeRelevance: NO_CONTEXT unrelated for method-bound details]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[computeRelevance: NO_CONTEXT unrelated for method-bound details]==]  )
set_tests_properties( [==[computeRelevance: NO_CONTEXT unrelated for method-bound details]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[computeRelevance: bare words without '(' are not method mentions]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[computeRelevance: bare words without '(' are not method mentions]==]  )
set_tests_properties( [==[computeRelevance: bare words without '(' are not method mentions]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[computeRelevance: method context is EXACT unless goal names another method]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[computeRelevance: method context is EXACT unless goal names another method]==]  )
set_tests_properties( [==[computeRelevance: method context is EXACT unless goal names another method]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[computeRelevance: non-method contexts are NEARBY]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[computeRelevance: non-method contexts are NEARBY]==]  )
set_tests_properties( [==[computeRelevance: non-method contexts are NEARBY]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[extractFirstJsonObject skips braces inside strings]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[extractFirstJsonObject skips braces inside strings]==]  )
set_tests_properties( [==[extractFirstJsonObject skips braces inside strings]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[primitiveFromString: model casing/whitespace are tolerated]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[primitiveFromString: model casing/whitespace are tolerated]==]  )
set_tests_properties( [==[primitiveFromString: model casing/whitespace are tolerated]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[relevanceToString covers all values]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[relevanceToString covers all values]==]  )
set_tests_properties( [==[relevanceToString covers all values]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[routeTask: Category C requires remote]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[routeTask: Category C requires remote]==]  )
set_tests_properties( [==[routeTask: Category C requires remote]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[routeTask: deterministic details go to NONE (no model)]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[routeTask: deterministic details go to NONE (no model)]==]  )
set_tests_properties( [==[routeTask: deterministic details go to NONE (no model)]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[routeTask: large refactor/architecture goes REMOTE, or refuses without remote]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[routeTask: large refactor/architecture goes REMOTE\, or refuses without remote]==]  )
set_tests_properties( [==[routeTask: large refactor/architecture goes REMOTE, or refuses without remote]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[routeTask: read-only goes LOCAL; if local incapable -> remote/refuse]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[routeTask: read-only goes LOCAL; if local incapable -> remote/refuse]==]  )
set_tests_properties( [==[routeTask: read-only goes LOCAL; if local incapable -> remote/refuse]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[routeTask: small edit within local profile -> LOCAL]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[routeTask: small edit within local profile -> LOCAL]==]  )
set_tests_properties( [==[routeTask: small edit within local profile -> LOCAL]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[routeTask: wide edit -> SPLIT when remote available]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[routeTask: wide edit -> SPLIT when remote available]==]  )
set_tests_properties( [==[routeTask: wide edit -> SPLIT when remote available]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[selectAction: UNRELATED/AMBIGUOUS force NEEDS_INPUT_RELEVANCE]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[selectAction: UNRELATED/AMBIGUOUS force NEEDS_INPUT_RELEVANCE]==]  )
set_tests_properties( [==[selectAction: UNRELATED/AMBIGUOUS force NEEDS_INPUT_RELEVANCE]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[selectAction: call-graph details map to call actions]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[selectAction: call-graph details map to call actions]==]  )
set_tests_properties( [==[selectAction: call-graph details map to call actions]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[selectAction: decision table with non-blocking relevance]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[selectAction: decision table with non-blocking relevance]==]  )
set_tests_properties( [==[selectAction: decision table with non-blocking relevance]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[selectAction: default relevance arg keeps backward-compatible behaviour]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[selectAction: default relevance arg keeps backward-compatible behaviour]==]  )
set_tests_properties( [==[selectAction: default relevance arg keeps backward-compatible behaviour]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[selectAction: dependency/quality details map to their actions]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[selectAction: dependency/quality details map to their actions]==]  )
set_tests_properties( [==[selectAction: dependency/quality details map to their actions]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
add_test( [==[selectAction: type-usage/complexity/duplicates mapping]==] /home/artem/projects/outer/AI-agent/cpp-tool/build/agent_tests [==[selectAction: type-usage/complexity/duplicates mapping]==]  )
set_tests_properties( [==[selectAction: type-usage/complexity/duplicates mapping]==] PROPERTIES WORKING_DIRECTORY /home/artem/projects/outer/AI-agent/cpp-tool/build SKIP_RETURN_CODE 4)
set( agent_tests_TESTS [==[
  {
    "class-name" : "",
    "name" : "CConfigReader: access policies default and override",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_config_reader.cpp",
      "line" : 54
    },
    "tags" :  "config" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "CConfigReader: creates default agent.conf and projects when missing",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_config_reader.cpp",
      "line" : 109
    },
    "tags" :  "config" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "CConfigReader: db_file override from projects wins",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_config_reader.cpp",
      "line" : 153
    },
    "tags" :  "config" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "CConfigReader: light aliases to main when same_as_main",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_config_reader.cpp",
      "line" : 38
    },
    "tags" :  "config" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "CConfigReader: model getters and refresh_from_config",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_config_reader.cpp",
      "line" : 74
    },
    "tags" :  "config" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "CConfigReader: parses agent.conf-style key/value with comments",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_config_reader.cpp",
      "line" : 25
    },
    "tags" :  "config" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "CConfigReader: parses remote endpoint (type/url/name/key)",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_config_reader.cpp",
      "line" : 94
    },
    "tags" :  "config" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "CConfigReader: resolve --cc keeps build=parent, db deterministic",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_config_reader.cpp",
      "line" : 119
    },
    "tags" :  "config" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "CConfigReader: resolve --project auto-registers and maps db",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_config_reader.cpp",
      "line" : 135
    },
    "tags" :  "config" 
  }]==] [==[
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
      "line" : 938
    },
    "tags" :  "anchor", "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "HttpMainModelBroker parses plan/actions via injected IApiAI",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_api_ai.cpp",
      "line" : 78
    },
    "tags" :  "api" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "HttpMainModelBroker reports provider down",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_api_ai.cpp",
      "line" : 97
    },
    "tags" :  "api" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "HttpMainModelBroker: generate plan with real model",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 555
    },
    "tags" :  "!mayfail", "live", "orchestrator" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "LightModelBroker parses classifier JSON via injected IApiAI",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_api_ai.cpp",
      "line" : 44
    },
    "tags" :  "api" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "LightModelBroker surfaces provider error as unavailable",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_api_ai.cpp",
      "line" : 68
    },
    "tags" :  "api" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "LightModelBroker tolerates non-JSON and reports unavailable",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_api_ai.cpp",
      "line" : 60
    },
    "tags" :  "api" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "LightModelBroker: classify with real model",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 506
    },
    "tags" :  "!mayfail", "live", "orchestrator" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "LightModelBroker: real-model primitive accuracy on dataset",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 528
    },
    "tags" :  "!mayfail", "live", "orchestrator" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "ModelProfile: probe measures JSON compliance and caches",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_task_router.cpp",
      "line" : 107
    },
    "tags" :  "profile" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "ModelProfile: remote verdict overrides heuristic",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_task_router.cpp",
      "line" : 131
    },
    "tags" :  "profile" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: '\u0442\u043e\u043b\u044c\u043a\u043e \u043f\u0440\u043e\u0432\u0435\u0440\u044c' vetoes write primitive",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 836
    },
    "tags" :  "orchestrator", "stub", "veto" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: CALL_CHAIN lists callers via cached call graph",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 1097
    },
    "tags" :  "calls", "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: CODE_SMELL heuristic on a method",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 1159
    },
    "tags" :  "analysis", "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: CursorBinding NEEDED_MISSING asks for cursor, retry proceeds",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 1175
    },
    "tags" :  "cursorbinding", "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: CursorBinding NOT_NEEDED skips Ground, answers without cursor",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 1061
    },
    "tags" :  "cursorbinding", "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: DEPENDENCY_MAP reports file dependents",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 1122
    },
    "tags" :  "analysis", "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: GENERAL_QUESTION is delegated read-only to main model",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 1018
    },
    "tags" :  "delegate", "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: INFRA must report failure on real non-zero build",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 747
    },
    "tags" :  "infrafail", "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: INFRA without cursor",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 203
    },
    "tags" :  "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: InfraHandler build with real buildDir",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 383
    },
    "tags" :  "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: InfraHandler graph targets",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 408
    },
    "tags" :  "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: NOT_NEEDED normalizes method-bound Detail to GENERAL_QUESTION",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 1077
    },
    "tags" :  "cursorbinding", "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: SANDBOX_FIX on tiny project passes without confirmation (adaptive thresholds)",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 143
    },
    "tags" :  "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: SANDBOX_FIX triggers blast_radius needs_input with explicit low thresholds",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 169
    },
    "tags" :  "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: SANDBOX_FIX with Stub models",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 115
    },
    "tags" :  "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: TODO_SCAN finds markers deterministically",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 1138
    },
    "tags" :  "analysis", "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: UNDERSTAND with Stub model",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 93
    },
    "tags" :  "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: blast_radius wide warning",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 248
    },
    "tags" :  "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: clarification cycle blast_radius -> cancel",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 311
    },
    "tags" :  "clarify", "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: clarification cycle blast_radius -> proceed -> answer",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 275
    },
    "tags" :  "clarify", "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: clarification cycle depth protection",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 332
    },
    "tags" :  "clarify", "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: clarification replay protection",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 359
    },
    "tags" :  "clarify", "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: cursor on method A while goal names method B -> needs_input relevance",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 998
    },
    "tags" :  "orchestrator", "relevance", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: dead model + INFRA phrase -> answer via stub (no missing_cursor)",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 427
    },
    "tags" :  "fallback", "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: dead model + clarification proceed -> writes",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 473
    },
    "tags" :  "fallback", "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: dead model + clarification wait -> error model_unavailable_wait",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 489
    },
    "tags" :  "fallback", "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: dead model + question -> answer via stub UNDERSTAND",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 440
    },
    "tags" :  "fallback", "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: dead model + write phrase -> needs_input model-unavailable",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 454
    },
    "tags" :  "fallback", "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: explicit '\u0431\u0435\u0437 \u043f\u0440\u0430\u0432\u043e\u043a' vetoes write primitive -> UNDERSTAND",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 813
    },
    "tags" :  "orchestrator", "stub", "veto" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: failed main plan must not produce green verify",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 727
    },
    "tags" :  "orchestrator", "planfail", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: full cycle SANDBOX_FIX with real models",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 609
    },
    "tags" :  "!mayfail", "live", "orchestrator" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: full cycle UNDERSTAND with real light model",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 579
    },
    "tags" :  "!mayfail", "live", "orchestrator" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: needs_input on low confidence (manual simulation)",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 220
    },
    "tags" :  "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: real project (reminder) UNDERSTAND on live model",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 655
    },
    "tags" :  "!mayfail", "live", "orchestrator", "realproject" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: vague goal answered -> goal_text replaced and proceeds",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 896
    },
    "tags" :  "goalspec", "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: vague goal for UNDERSTAND -> no blocking",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 920
    },
    "tags" :  "goalspec", "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: vague goal for write primitive -> needs_input goal-vague",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 873
    },
    "tags" :  "goalspec", "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "Orchestrator: write goal WITHOUT no-edit phrase is not vetoed",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 852
    },
    "tags" :  "orchestrator", "stub", "veto" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "StubModelBroker: capitalized Cyrillic reading verb -> UNDERSTAND",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 789
    },
    "tags" :  "orchestrator", "stub", "utf8" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "StubModelBroker: capitalized Cyrillic write verb still writes",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 804
    },
    "tags" :  "orchestrator", "stub", "utf8" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "StubModelBroker: fills Detail along with Primitive",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 1205
    },
    "tags" :  "detail", "orchestrator", "stub" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "computeRelevance: GENERAL_QUESTION is cursor-irrelevant (priority over NO_CONTEXT)",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_task_classification.cpp",
      "line" : 15
    },
    "tags" :  "classify", "unit" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "computeRelevance: GENERAL_QUESTION is never forced UNRELATED by method name",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_task_classification.cpp",
      "line" : 43
    },
    "tags" :  "classify", "unit" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "computeRelevance: NO_CONTEXT unrelated for method-bound details",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_task_classification.cpp",
      "line" : 9
    },
    "tags" :  "classify", "unit" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "computeRelevance: bare words without '(' are not method mentions",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_task_classification.cpp",
      "line" : 61
    },
    "tags" :  "classify", "unit" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "computeRelevance: method context is EXACT unless goal names another method",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_task_classification.cpp",
      "line" : 23
    },
    "tags" :  "classify", "unit" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "computeRelevance: non-method contexts are NEARBY",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_task_classification.cpp",
      "line" : 51
    },
    "tags" :  "classify", "unit" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "extractFirstJsonObject skips braces inside strings",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_api_ai.cpp",
      "line" : 33
    },
    "tags" :  "api" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "primitiveFromString: model casing/whitespace are tolerated",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_orchestrator_integration.cpp",
      "line" : 773
    },
    "tags" :  "model", "unit" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "relevanceToString covers all values",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_task_classification.cpp",
      "line" : 163
    },
    "tags" :  "classify", "unit" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "routeTask: Category C requires remote",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_task_router.cpp",
      "line" : 43
    },
    "tags" :  "router" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "routeTask: deterministic details go to NONE (no model)",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_task_router.cpp",
      "line" : 17
    },
    "tags" :  "router" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "routeTask: large refactor/architecture goes REMOTE, or refuses without remote",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_task_router.cpp",
      "line" : 26
    },
    "tags" :  "router" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "routeTask: read-only goes LOCAL if local incapable -> remote/refuse",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_task_router.cpp",
      "line" : 78
    },
    "tags" :  "router" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "routeTask: small edit within local profile -> LOCAL",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_task_router.cpp",
      "line" : 52
    },
    "tags" :  "router" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "routeTask: wide edit -> SPLIT when remote available",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_task_router.cpp",
      "line" : 63
    },
    "tags" :  "router" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "selectAction: UNRELATED/AMBIGUOUS force NEEDS_INPUT_RELEVANCE",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_task_classification.cpp",
      "line" : 72
    },
    "tags" :  "classify", "unit" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "selectAction: call-graph details map to call actions",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_task_classification.cpp",
      "line" : 118
    },
    "tags" :  "classify", "unit" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "selectAction: decision table with non-blocking relevance",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_task_classification.cpp",
      "line" : 81
    },
    "tags" :  "classify", "unit" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "selectAction: default relevance arg keeps backward-compatible behaviour",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_task_classification.cpp",
      "line" : 112
    },
    "tags" :  "classify", "unit" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "selectAction: dependency/quality details map to their actions",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_task_classification.cpp",
      "line" : 128
    },
    "tags" :  "classify", "unit" 
  }]==] [==[
  {
    "class-name" : "",
    "name" : "selectAction: type-usage/complexity/duplicates mapping",
    "source-location" : 
    {
      "filename" : "/home/artem/projects/outer/AI-agent/cpp-tool/agent/tests/test_task_classification.cpp",
      "line" : 145
    },
    "tags" :  "classify", "unit" 
  }
]==] [==[CConfigReader: access policies default and override]==] [==[CConfigReader: creates default agent.conf and projects when missing]==] [==[CConfigReader: db_file override from projects wins]==] [==[CConfigReader: light aliases to main when same_as_main]==] [==[CConfigReader: model getters and refresh_from_config]==] [==[CConfigReader: parses agent.conf-style key/value with comments]==] [==[CConfigReader: parses remote endpoint (type/url/name/key)]==] [==[CConfigReader: resolve --cc keeps build=parent, db deterministic]==] [==[CConfigReader: resolve --project auto-registers and maps db]==] [==[ExecuteService apply: empty edits -> ok no checkpoint]==] [==[ExecuteService apply: empty oldText reports insert]==] [==[ExecuteService apply: exact match reports exact]==] [==[ExecuteService apply: failure on second edit must not leave first file changed]==] [==[ExecuteService apply: file not found -> fail]==] [==[ExecuteService apply: fuzzy match on whitespace differences]==] [==[ExecuteService apply: fuzzy prefix match when model collapses newlines]==] [==[ExecuteService apply: hintLine 0 keeps first-occurrence behaviour]==] [==[ExecuteService apply: insert when oldText empty]==] [==[ExecuteService apply: multi-file]==] [==[ExecuteService apply: oldText not found -> fail]==] [==[ExecuteService apply: picks occurrence nearest to hintLine, not first]==] [==[ExecuteService apply: preserves relative inner indent in multi-line newText]==] [==[ExecuteService apply: reindents newText when model drops leading indent]==] [==[ExecuteService apply: removes excess indent when oldText over-indented]==] [==[ExecuteService apply: replace oldText]==] [==[ExecuteService undo: unknown id -> fail]==] [==[GroundService: USR anchor survives line shifts]==] [==[HttpMainModelBroker parses plan/actions via injected IApiAI]==] [==[HttpMainModelBroker reports provider down]==] [==[HttpMainModelBroker: generate plan with real model]==] [==[LightModelBroker parses classifier JSON via injected IApiAI]==] [==[LightModelBroker surfaces provider error as unavailable]==] [==[LightModelBroker tolerates non-JSON and reports unavailable]==] [==[LightModelBroker: classify with real model]==] [==[LightModelBroker: real-model primitive accuracy on dataset]==] [==[ModelProfile: probe measures JSON compliance and caches]==] [==[ModelProfile: remote verdict overrides heuristic]==] [==[Orchestrator: 'только проверь' vetoes write primitive]==] [==[Orchestrator: CALL_CHAIN lists callers via cached call graph]==] [==[Orchestrator: CODE_SMELL heuristic on a method]==] [==[Orchestrator: CursorBinding NEEDED_MISSING asks for cursor, retry proceeds]==] [==[Orchestrator: CursorBinding NOT_NEEDED skips Ground, answers without cursor]==] [==[Orchestrator: DEPENDENCY_MAP reports file dependents]==] [==[Orchestrator: GENERAL_QUESTION is delegated read-only to main model]==] [==[Orchestrator: INFRA must report failure on real non-zero build]==] [==[Orchestrator: INFRA without cursor]==] [==[Orchestrator: InfraHandler build with real buildDir]==] [==[Orchestrator: InfraHandler graph targets]==] [==[Orchestrator: NOT_NEEDED normalizes method-bound Detail to GENERAL_QUESTION]==] [==[Orchestrator: SANDBOX_FIX on tiny project passes without confirmation (adaptive thresholds)]==] [==[Orchestrator: SANDBOX_FIX triggers blast_radius needs_input with explicit low thresholds]==] [==[Orchestrator: SANDBOX_FIX with Stub models]==] [==[Orchestrator: TODO_SCAN finds markers deterministically]==] [==[Orchestrator: UNDERSTAND with Stub model]==] [==[Orchestrator: blast_radius wide warning]==] [==[Orchestrator: clarification cycle blast_radius -> cancel]==] [==[Orchestrator: clarification cycle blast_radius -> proceed -> answer]==] [==[Orchestrator: clarification cycle depth protection]==] [==[Orchestrator: clarification replay protection]==] [==[Orchestrator: cursor on method A while goal names method B -> needs_input relevance]==] [==[Orchestrator: dead model + INFRA phrase -> answer via stub (no missing_cursor)]==] [==[Orchestrator: dead model + clarification proceed -> writes]==] [==[Orchestrator: dead model + clarification wait -> error model_unavailable_wait]==] [==[Orchestrator: dead model + question -> answer via stub UNDERSTAND]==] [==[Orchestrator: dead model + write phrase -> needs_input model-unavailable]==] [==[Orchestrator: explicit 'без правок' vetoes write primitive -> UNDERSTAND]==] [==[Orchestrator: failed main plan must not produce green verify]==] [==[Orchestrator: full cycle SANDBOX_FIX with real models]==] [==[Orchestrator: full cycle UNDERSTAND with real light model]==] [==[Orchestrator: needs_input on low confidence (manual simulation)]==] [==[Orchestrator: real project (reminder) UNDERSTAND on live model]==] [==[Orchestrator: vague goal answered -> goal_text replaced and proceeds]==] [==[Orchestrator: vague goal for UNDERSTAND -> no blocking]==] [==[Orchestrator: vague goal for write primitive -> needs_input goal-vague]==] [==[Orchestrator: write goal WITHOUT no-edit phrase is not vetoed]==] [==[StubModelBroker: capitalized Cyrillic reading verb -> UNDERSTAND]==] [==[StubModelBroker: capitalized Cyrillic write verb still writes]==] [==[StubModelBroker: fills Detail along with Primitive]==] [==[computeRelevance: GENERAL_QUESTION is cursor-irrelevant (priority over NO_CONTEXT)]==] [==[computeRelevance: GENERAL_QUESTION is never forced UNRELATED by method name]==] [==[computeRelevance: NO_CONTEXT unrelated for method-bound details]==] [==[computeRelevance: bare words without '(' are not method mentions]==] [==[computeRelevance: method context is EXACT unless goal names another method]==] [==[computeRelevance: non-method contexts are NEARBY]==] [==[extractFirstJsonObject skips braces inside strings]==] [==[primitiveFromString: model casing/whitespace are tolerated]==] [==[relevanceToString covers all values]==] [==[routeTask: Category C requires remote]==] [==[routeTask: deterministic details go to NONE (no model)]==] [==[routeTask: large refactor/architecture goes REMOTE, or refuses without remote]==] [==[routeTask: read-only goes LOCAL]==] [==[ if local incapable -> remote/refuse]==] [==[routeTask: small edit within local profile -> LOCAL]==] [==[routeTask: wide edit -> SPLIT when remote available]==] [==[selectAction: UNRELATED/AMBIGUOUS force NEEDS_INPUT_RELEVANCE]==] [==[selectAction: call-graph details map to call actions]==] [==[selectAction: decision table with non-blocking relevance]==] [==[selectAction: default relevance arg keeps backward-compatible behaviour]==] [==[selectAction: dependency/quality details map to their actions]==] [==[selectAction: type-usage/complexity/duplicates mapping]==])
