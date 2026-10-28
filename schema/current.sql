-- Canonical SQLite schema for new databases.
-- This file is applied only when the database file does not yet exist.
-- Existing databases are validated; their schema is never altered.

CREATE TABLE registry_project
(
    project_id     TEXT PRIMARY KEY,
    canonical_path TEXT NOT NULL UNIQUE,
    db_path        TEXT NOT NULL,
    last_opened_at INTEGER,
    created_at     INTEGER
);

CREATE INDEX idx_registry_project_path
    ON registry_project(canonical_path);

CREATE TABLE project
(
    id               INTEGER PRIMARY KEY,
    canonical_path   TEXT NOT NULL UNIQUE,
    name             TEXT,
    build_dir        TEXT,
    last_indexed_at  INTEGER,
    index_generation INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE index_generation
(
    generation   INTEGER PRIMARY KEY,
    state        TEXT NOT NULL,
    started_at   INTEGER,
    finished_at  INTEGER,
    file_count   INTEGER,
    symbol_count INTEGER,
    error        TEXT
);

CREATE TABLE file
(
    id         INTEGER PRIMARY KEY,
    path       TEXT NOT NULL,
    generation INTEGER NOT NULL REFERENCES index_generation(generation),
    mtime      INTEGER,
    size       INTEGER,
    hash       TEXT,
    UNIQUE(path, generation)
);

CREATE TABLE symbol
(
    id             INTEGER PRIMARY KEY,
    generation     INTEGER NOT NULL REFERENCES index_generation(generation),
    usr            TEXT,
    kind           TEXT NOT NULL,
    name           TEXT NOT NULL,
    qualified_name TEXT,
    file_id        INTEGER REFERENCES file(id),
    parent_id      INTEGER REFERENCES symbol(id),
    parent_usr     TEXT,
    is_definition  INTEGER NOT NULL DEFAULT 0,
    is_declaration INTEGER NOT NULL DEFAULT 0,
    signature      TEXT,
    access         TEXT,
    flags          INTEGER NOT NULL DEFAULT 0
);

CREATE INDEX idx_symbol_usr
    ON symbol(usr);

CREATE INDEX idx_symbol_name
    ON symbol(name);

CREATE INDEX idx_symbol_qualified_name
    ON symbol(qualified_name);

CREATE TABLE symbol_location
(
    id         INTEGER PRIMARY KEY,
    symbol_id  INTEGER NOT NULL REFERENCES symbol(id) ON DELETE CASCADE,
    file_id    INTEGER NOT NULL REFERENCES file(id),
    line       INTEGER NOT NULL,
    column     INTEGER NOT NULL,
    end_line   INTEGER,
    end_column INTEGER
);

CREATE INDEX idx_symbol_location_symbol
    ON symbol_location(symbol_id);

CREATE TABLE compile_command
(
    id        INTEGER PRIMARY KEY,
    file_id   INTEGER NOT NULL REFERENCES file(id) ON DELETE CASCADE,
    directory TEXT,
    arguments TEXT,
    language  TEXT
);

CREATE INDEX idx_compile_command_file
    ON compile_command(file_id);

CREATE TABLE include
(
    id               INTEGER PRIMARY KEY,
    generation       INTEGER NOT NULL REFERENCES index_generation(generation),
    from_file_id     INTEGER NOT NULL REFERENCES file(id) ON DELETE CASCADE,
    to_path          TEXT NOT NULL,
    resolved_file_id INTEGER REFERENCES file(id),
    is_angle         INTEGER NOT NULL DEFAULT 0
);

CREATE INDEX idx_include_from
    ON include(from_file_id);

CREATE INDEX idx_include_resolved
    ON include(resolved_file_id);

CREATE TABLE inheritance
(
    id         INTEGER PRIMARY KEY,
    generation INTEGER NOT NULL REFERENCES index_generation(generation),
    derived_id INTEGER NOT NULL REFERENCES symbol(id) ON DELETE CASCADE,
    base_id    INTEGER REFERENCES symbol(id),
    base_qname TEXT,
    access     TEXT,
    is_virtual INTEGER NOT NULL DEFAULT 0
);

CREATE INDEX idx_inheritance_derived
    ON inheritance(derived_id);

CREATE INDEX idx_inheritance_base
    ON inheritance(base_id);

CREATE TABLE override
(
    id             INTEGER PRIMARY KEY,
    generation     INTEGER NOT NULL REFERENCES index_generation(generation),
    method_id      INTEGER NOT NULL REFERENCES symbol(id) ON DELETE CASCADE,
    base_method_id INTEGER REFERENCES symbol(id)
);

CREATE INDEX idx_override_method
    ON override(method_id);

CREATE INDEX idx_override_base_method
    ON override(base_method_id);

CREATE TABLE reference
(
    id         INTEGER PRIMARY KEY,
    generation INTEGER NOT NULL REFERENCES index_generation(generation),
    symbol_id  INTEGER NOT NULL REFERENCES symbol(id) ON DELETE CASCADE,
    file_id    INTEGER NOT NULL REFERENCES file(id),
    line       INTEGER NOT NULL,
    column     INTEGER NOT NULL,
    kind       TEXT
);

CREATE INDEX idx_reference_symbol
    ON reference(symbol_id);

CREATE TABLE call
(
    id           INTEGER PRIMARY KEY,
    generation   INTEGER NOT NULL REFERENCES index_generation(generation),
    caller_id    INTEGER REFERENCES symbol(id),
    callee_id    INTEGER REFERENCES symbol(id),
    callee_qname TEXT,
    file_id      INTEGER REFERENCES file(id),
    line         INTEGER,
    column       INTEGER
);

CREATE INDEX idx_call_caller
    ON call(caller_id);

CREATE INDEX idx_call_callee
    ON call(callee_id);

CREATE TABLE project_profile
(
    id            INTEGER PRIMARY KEY CHECK (id = 1),
    original_text TEXT NOT NULL,
    compact_text  TEXT NOT NULL
);
