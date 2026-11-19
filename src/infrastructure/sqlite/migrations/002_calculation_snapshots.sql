-- Project135 SQLite schema migration 002.
-- Immutable calculation snapshots with atomic exact material revision links.

CREATE TABLE IF NOT EXISTS calculations (
    calculation_id       TEXT PRIMARY KEY NOT NULL,
    project_id           TEXT NOT NULL,
    project_name         TEXT NOT NULL,
    product_family       TEXT NOT NULL,
    input_json           TEXT NOT NULL CHECK(length(input_json) > 0),
    result_json          TEXT NOT NULL CHECK(length(result_json) > 0),
    engine_versions_json TEXT NOT NULL CHECK(length(engine_versions_json) > 0),
    content_hash         TEXT NOT NULL CHECK(length(content_hash) = 64),
    author               TEXT NOT NULL,
    created_at_utc       TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_calculations_project_time
    ON calculations(project_id, created_at_utc DESC);

CREATE TRIGGER IF NOT EXISTS calculations_no_update
BEFORE UPDATE ON calculations
BEGIN
    SELECT RAISE(ABORT, 'calculation snapshots are immutable');
END;

CREATE TRIGGER IF NOT EXISTS calculations_no_delete
BEFORE DELETE ON calculations
BEGIN
    SELECT RAISE(ABORT, 'calculation snapshots cannot be deleted');
END;

CREATE TRIGGER IF NOT EXISTS material_usage_validate_calculation
BEFORE INSERT ON material_usage
WHEN NEW.usage_kind = 'calculation'
BEGIN
    SELECT CASE
        WHEN NEW.calculation_id IS NULL OR NOT EXISTS (
            SELECT 1 FROM calculations WHERE calculation_id = NEW.calculation_id
        )
        THEN RAISE(ABORT, 'calculation usage must reference an existing calculation')
    END;
END;
