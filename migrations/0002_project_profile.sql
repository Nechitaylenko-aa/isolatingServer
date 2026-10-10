-- Persistent project passport. The same migration set is applied to
-- registry and per-project databases; this table is used only in index.db.
CREATE TABLE IF NOT EXISTS project_profile
(
    id            INTEGER PRIMARY KEY CHECK (id = 1),
    original_text TEXT NOT NULL,
    compact_text  TEXT NOT NULL
);
