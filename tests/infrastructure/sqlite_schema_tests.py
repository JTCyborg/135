from pathlib import Path
import sqlite3
import unittest


MIGRATION = (
    Path(__file__).resolve().parents[2]
    / "src"
    / "infrastructure"
    / "sqlite"
    / "migrations"
    / "001_initial_schema.sql"
)


class SqliteMaterialSchemaTests(unittest.TestCase):
    def setUp(self):
        self.connection = sqlite3.connect(":memory:")
        self.connection.execute("PRAGMA foreign_keys = ON")
        self.connection.executescript(MIGRATION.read_text(encoding="utf-8"))
        self.connection.execute(
            """INSERT INTO materials
               (material_id, record_type, created_at_utc, modified_at_utc)
               VALUES ('MAT-1', 'core-material', '2026-01-01T00:00:00Z',
                       '2026-01-01T00:00:00Z')"""
        )

    def tearDown(self):
        self.connection.close()

    def insert_revision(self, revision_id, number, parent_id=None):
        self.connection.execute(
            """INSERT INTO material_revisions
               (revision_id, material_id, revision_number, parent_revision_id,
                record_type, payload_json, content_hash, source_reference,
                dataset_revision, author, created_at_utc)
               VALUES (?, 'MAT-1', ?, ?, 'core-material', '{}', ?, 'fixture',
                       'fixture-1', 'test', '2026-01-01T00:00:00Z')""",
            (revision_id, number, parent_id, "a" * 64),
        )

    def test_revision_chain_and_exact_usage_reference(self):
        self.insert_revision("REV-1", 1)
        self.insert_revision("REV-2", 2, "REV-1")
        self.connection.execute(
            """INSERT INTO material_usage
               (usage_id, revision_id, product_id, usage_kind, created_at_utc)
               VALUES ('USE-1', 'REV-1', 'PRODUCT-1', 'product',
                       '2026-01-02T00:00:00Z')"""
        )
        row = self.connection.execute(
            "SELECT revision_id FROM material_usage WHERE usage_id='USE-1'"
        ).fetchone()
        self.assertEqual(row[0], "REV-1")

    def test_revision_snapshot_is_immutable(self):
        self.insert_revision("REV-1", 1)
        with self.assertRaises(sqlite3.IntegrityError):
            self.connection.execute(
                "UPDATE material_revisions SET payload_json='{}' WHERE revision_id='REV-1'"
            )
        with self.assertRaises(sqlite3.IntegrityError):
            self.connection.execute(
                "DELETE FROM material_revisions WHERE revision_id='REV-1'"
            )

    def test_revision_number_must_be_contiguous(self):
        with self.assertRaises(sqlite3.IntegrityError):
            self.insert_revision("REV-2", 2, "REV-1")

    def test_parent_must_be_latest_revision_of_same_material(self):
        self.insert_revision("REV-1", 1)
        self.insert_revision("REV-2", 2, "REV-1")
        with self.assertRaises(sqlite3.IntegrityError):
            self.insert_revision("REV-3", 3, "REV-1")

    def test_usage_cannot_reference_missing_revision(self):
        with self.assertRaises(sqlite3.IntegrityError):
            self.connection.execute(
                """INSERT INTO material_usage
                   (usage_id, revision_id, calculation_id, usage_kind, created_at_utc)
                   VALUES ('USE-X', 'MISSING', 'CALC-1', 'calculation',
                           '2026-01-02T00:00:00Z')"""
            )


if __name__ == "__main__":
    unittest.main(verbosity=2)
