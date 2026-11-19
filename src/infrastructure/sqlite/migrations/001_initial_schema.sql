-- Project135 SQLite schema migration 001.
-- Apply this file inside one SQLite transaction with PRAGMA foreign_keys = ON.
-- Timestamps are ISO-8601 UTC strings. All engineering quantities in payload_json use SI.

CREATE TABLE IF NOT EXISTS schema_migrations (
    version             INTEGER PRIMARY KEY,
    name                TEXT NOT NULL,
    checksum_sha256     TEXT NOT NULL CHECK(length(checksum_sha256) = 64),
    applied_at_utc      TEXT NOT NULL
);

CREATE TABLE materials (
    material_id         TEXT PRIMARY KEY NOT NULL,
    record_type         TEXT NOT NULL,
    archived            INTEGER NOT NULL DEFAULT 0 CHECK(archived IN (0, 1)),
    created_at_utc      TEXT NOT NULL,
    modified_at_utc     TEXT NOT NULL
);

CREATE TABLE material_revisions (
    revision_id         TEXT PRIMARY KEY NOT NULL,
    material_id         TEXT NOT NULL,
    revision_number     INTEGER NOT NULL CHECK(revision_number > 0),
    parent_revision_id  TEXT,
    record_type         TEXT NOT NULL,
    payload_json        TEXT NOT NULL CHECK(length(payload_json) > 0),
    content_hash        TEXT NOT NULL CHECK(length(content_hash) = 64),
    source_reference    TEXT NOT NULL,
    dataset_revision    TEXT NOT NULL,
    author              TEXT NOT NULL,
    created_at_utc      TEXT NOT NULL,
    UNIQUE(material_id, revision_number),
    UNIQUE(material_id, revision_id),
    FOREIGN KEY(material_id) REFERENCES materials(material_id),
    FOREIGN KEY(material_id, parent_revision_id)
        REFERENCES material_revisions(material_id, revision_id)
        DEFERRABLE INITIALLY DEFERRED
);

CREATE INDEX idx_material_revisions_latest
    ON material_revisions(material_id, revision_number DESC);

CREATE TABLE material_usage (
    usage_id            TEXT PRIMARY KEY NOT NULL,
    revision_id         TEXT NOT NULL,
    product_id          TEXT,
    calculation_id      TEXT,
    usage_kind          TEXT NOT NULL CHECK(usage_kind IN ('product', 'calculation')),
    created_at_utc      TEXT NOT NULL,
    CHECK(product_id IS NOT NULL OR calculation_id IS NOT NULL),
    FOREIGN KEY(revision_id) REFERENCES material_revisions(revision_id)
);

CREATE INDEX idx_material_usage_revision ON material_usage(revision_id);

CREATE TABLE audit_log (
    audit_id            INTEGER PRIMARY KEY AUTOINCREMENT,
    entity_type         TEXT NOT NULL,
    entity_id           TEXT NOT NULL,
    event_type          TEXT NOT NULL,
    actor               TEXT NOT NULL,
    occurred_at_utc     TEXT NOT NULL,
    details_json        TEXT NOT NULL
);

CREATE INDEX idx_audit_entity_time
    ON audit_log(entity_type, entity_id, occurred_at_utc);

-- A revision is append-only. Corrections always create a new revision.
CREATE TRIGGER material_revisions_no_update
BEFORE UPDATE ON material_revisions
BEGIN
    SELECT RAISE(ABORT, 'material revisions are immutable');
END;

CREATE TRIGGER material_revisions_no_delete
BEFORE DELETE ON material_revisions
BEGIN
    SELECT RAISE(ABORT, 'material revisions cannot be deleted');
END;

-- Enforce a linear revision chain even when callers bypass Application use cases.
CREATE TRIGGER material_revisions_validate_chain
BEFORE INSERT ON material_revisions
BEGIN
    SELECT CASE
        WHEN NEW.revision_number != COALESCE(
            (SELECT MAX(revision_number) + 1
             FROM material_revisions WHERE material_id = NEW.material_id), 1)
        THEN RAISE(ABORT, 'revision number must extend the latest revision')
    END;
    SELECT CASE
        WHEN NEW.revision_number = 1 AND NEW.parent_revision_id IS NOT NULL
        THEN RAISE(ABORT, 'first revision cannot have a parent')
        WHEN NEW.revision_number > 1 AND NEW.parent_revision_id IS NOT (
            SELECT revision_id FROM material_revisions
            WHERE material_id = NEW.material_id
            ORDER BY revision_number DESC LIMIT 1)
        THEN RAISE(ABORT, 'parent must be the latest revision of this material')
    END;
    SELECT CASE
        WHEN NEW.record_type != (
            SELECT record_type FROM materials WHERE material_id = NEW.material_id)
        THEN RAISE(ABORT, 'revision record type must match material record type')
    END;
END;

-- The migration runner records version, checksum and applied_at_utc after this DDL succeeds.
