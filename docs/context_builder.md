# Context builder: source-gate contract

Status: bounded chat and passport gates are integrated into Intent and ReAct prompt assembly; the code-evidence gate is integrated into evidence-based answer generation. Dynamic tool-result context in ReAct remains handled by its existing loop.

## Responsibility boundary

The context builder prepares input for one model invocation. It coordinates independent sources and applies the final token budget. It does not own their data or implement their internal policies.

Initial sources:

- **Code** — indexed evidence gathered by the existing request/code-analysis path. Evidence sections now pass through `CCodeContextSource` with a 7000-token source allowance and high selection priority; dynamic ReAct tool results remain in the existing tool loop. Code has the highest content priority; the context builder must not silently replace required code with chat history.
- **Project passport** — supplies project rules and decisions through `CProjectProfileContextSource`, with an initial 800-token allowance.
- **Chat** — supplies a concise view of the current task from the active chat area only. Archived topics are not consulted by default.
- **Archive** — belongs to the chat source and is an explicit extension point. Archive-selection policy is deliberately deferred.

The project passport and chat are separate sources. Neither is a summary or storage format for the other.

## Source gate

Each source exposes a gate through `IContextSource::collect_context(request)`.

The builder supplies:

- `task`: the current task for which context is being assembled;
- `max_tokens`: the maximum token allowance requested from that source.

A source owns the selection of its own data and must return a useful, bounded contribution. It should not fill the allowance merely because it is available: brevity is preferred, and further refinement is not worth the cost when the current result is already good enough.

A source returns complete `SContextGroup` units. It may report that its contribution is incomplete, but it must not silently claim completeness when the requested allowance prevented it from supplying relevant information.

The reported token count is diagnostic only. The context builder performs the final combined-budget check because only it sees all contributions and the model/output limits.

Sources must not call the LLM directly. If a future gate needs model-assisted selection or compression, it should use a shared model interface and the same invocation-budget rules rather than create a private model-access path.

## Initial priority rule

Chat has the smallest source allowance by default. Its gate receives an explicit maximum and returns only a concise extract of the **active** task/history. The initial implementation prefers recent turns over older extractive excerpts and reports when its contribution is partial. Current defaults are 600 estimated tokens for chat in Intent, 1200 for chat in ReAct, 800 for the passport, and 7000 for indexed code evidence during answer generation; these are initial constants, not model-specific tuning. Semantic reconciliation of corrections/cancellations remains future work.

The chat gate does not consult the archive unless a later policy explicitly requests it. No archive-retrieval heuristics are part of this scaffold.

Code and other mandatory inputs must not be silently truncated to make room for chat. If mandatory inputs do not fit, context construction fails explicitly or reports the condition to the caller.

## Deletion invariant

A chat branch removed by `shrink_question()` must not be returned by any source gate, including through a previously created summary or cache. The interface does not provide an archive or recovery channel for deleted content.

## Deliberately deferred

Do not implement these branches as part of the initial scaffold:

- deciding whether an archive topic is relevant;
- choosing which archive content to inject or what it replaces;
- LLM-based chat summarization;
- automatic reconciliation of passport facts;
- dynamic per-model source-allocation strategies.

The gates establish where those decisions will live. They do not prejudge their future algorithms.


## Initial implementation scope

`CChatContextSource` is now used by both `CAskService` (Intent/composition prompt) and `CReActAgent`. It returns whole message groups within its own token allowance; the existing `CContextManager` then performs the final combined-budget check. The source does not access SQLite itself: it receives the active history supplied by the caller and never reads archived topics. Extractive compaction is intentionally lossy and is reported as partial when older turns exist. The original history is not mutated.


The passport gate is used by Intent, ReAct, and evidence-based answer generation. If its full text does not fit its source allowance, it returns no partial passport and reports the condition. The code-evidence gate preserves complete evidence-section groups and reports omitted sections when its allowance is insufficient. These source budgets are enforced independently; the existing final `CContextManager` still checks the combined prompt against the actual invocation budget.

## Model budget source of truth and diagnostics

The context-window values are configured in `~/.fcpp-agent/agent.conf` via
`model_main_context_window_tokens`, `model_light_context_window_tokens`, and
`model_remote_context_window_tokens`; output reservations are configured by the
corresponding `*_output_reserve_tokens` keys. `CConfigManager` reads these values
at startup or explicit configuration reload. The agent does **not** discover the
model's context window from provider metadata. A context-window value of `0`
means unknown, so the per-invocation hard prompt cap is used without a model-window
calculation. A light role sharing the main endpoint inherits the main role's
configured window and output reservation when its own values are unset.

For each model invocation, `CContextManager` computes the effective prompt budget
as the stricter of the caller's prompt cap and
`context_window_tokens - output_reserve_tokens`, when a context window is known.
The source allowances (chat, passport, code evidence) are independent upstream
caps, not a replacement for this final combined check. The default token counter is
a rough character-based estimate; it is not a provider-specific tokenizer, so
configured budgets and diagnostics should be treated as estimates unless a real
token counter is injected.

Successful final builds emit `CONTEXT_SELECTION` diagnostics with the estimated
tokens used, effective budget, included group IDs, and excluded group IDs. This
makes budget pressure and priority decisions observable without logging message
contents or secrets.


## Dynamic ReAct tool results

Each ReAct tool round remains an atomic group containing the assistant tool-call
message and its matching tool result message(s). Successful rounds have priority
80, ahead of chat history (priority 10); failed rounds have priority 20 so error
text cannot crowd out successful evidence. Within the same priority, newer rounds
are selected first. The original sequence is restored when selected messages are
assembled for the model.

A single tool output is bounded to 6000 bytes before it is added to the next prompt.
For oversized outputs, the beginning and end are retained with an explicit marker
stating that the middle was omitted. This is a safety bound, not a semantic summary:
if the omitted middle may contain the needed fact, the model should call a narrower
tool query. The context manager still enforces the final per-invocation budget and
logs included/excluded group IDs. The append-only conversation store is not modified.


## Expected negative-path diagnostics in tests

Streaming integration tests deliberately exercise consumer cancellation and a truncated
HTTP/SSE response. Those requests must return failure and must never append a fallback
answer to text already delivered to the caller. The test harness captures and asserts
their diagnostics instead of printing expected failure messages into ordinary local test
runs. Cancellation is logged as a caller-controlled outcome; transport truncation remains
a provider/transport error and is logged as such. GitHub Actions uses
`ctest --output-on-failure`, so output from passing tests is hidden by design.
