# Context builder: source-gate contract

Status: architectural scaffold. Source-specific branching and semantic summarization are intentionally not implemented here.

## Responsibility boundary

The context builder prepares input for one model invocation. It coordinates independent sources and applies the final token budget. It does not own their data or implement their internal policies.

Initial sources:

- **Code** — supplied by the existing request/code-analysis path. Code has the highest content priority; the context builder must not silently replace required code with chat history.
- **Project passport** — supplies a short, applicable selection of long-lived project rules and decisions.
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

Chat has the smallest source allowance by default. Its gate receives an explicit maximum and returns only a concise extract of the **active** task/history. The first implementation should prefer recent messages and preserve corrections/cancellations; later messages may supersede earlier statements wholly or partially.

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
