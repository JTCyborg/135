# SQLite persistence schema

This directory contains ordered SQL migrations for the Project135 infrastructure adapter.
The first migration establishes the persistent material identity, immutable revision snapshots,
revision-to-revision lineage, saved product/calculation usage references, and audit events.

## Migration runner contract

The C++ migration runner must:

1. Open the database and enable `PRAGMA foreign_keys = ON` on every connection.
2. Start a transaction before applying a migration.
3. Read the migration file as UTF-8 and calculate SHA-256 over its exact bytes.
4. Check `schema_migrations`: an already-applied version must have the same checksum;
   a mismatch is a hard error and must never be silently reapplied.
5. Execute the migration and insert its version, name, calculated checksum and UTC timestamp
   into `schema_migrations` in the same transaction.
6. Commit only after both the DDL and migration record succeed; otherwise roll back.

The checksum column is intentionally populated by the runner, not embedded as a placeholder
inside the SQL file. This avoids self-referential checksums and makes modified migrations detectable.

## Storage invariants

- A material identity is separate from its immutable revision snapshots.
- Revision IDs and per-material revision numbers are unique.
- A trigger enforces a contiguous linear revision chain and matching record type.
- UPDATE and DELETE against revision snapshots are rejected by SQLite triggers.
- Usage references point to the exact revision used by a product or calculation.
- Archival state and audit events are separate from immutable revision payloads.
- All serialized engineering quantities are SI; display-unit conversion belongs at boundaries.

The schema is a migration artifact, not yet a production repository implementation.
The SQLite C++ adapter, transaction wrapper, migration runner and integration tests remain open work.

## C++ adapter

The first C++ infrastructure slice is built as `Project135::InfrastructureSqlite`.
It provides a move-only SQLite connection with foreign-key enforcement, a busy timeout,
explicit immediate transactions, and a migration runner that hashes exact migration bytes
with OpenSSL SHA-256. Applied migrations are checked before any DDL is replayed; checksum
mismatches fail closed.

Build dependencies are supplied through the vcpkg manifest (`sqlite3`, `openssl`, `nlohmann-json`).
For a local Visual Studio 2026 configure, install the manifest dependencies and pass the
vcpkg toolchain file:

```powershell
vcpkg install --triplet x64-windows
cmake --preset vs2026-desktop-debug -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_INSTALLATION_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build --preset build-desktop-debug
ctest --preset test-desktop-debug
```

The adapter now includes `SqliteMaterialRevisionStore`, implementing the Application Layer's
revision repository and exact-revision usage query contracts. It atomically creates material
identity/revision/audit records, validates SHA-256 against the stored payload bytes, reads a
revision or latest revision, archives/restores identities with audit events, and checks whether
any saved product/calculation references a revision. SQLite triggers enforce immutable snapshots
and linear revision lineage. Canonical JSON generation/validation, a public usage-write API,
import/export, revision comparison/restore, advanced catalog filters and broader concurrency/rollback tests remain unfinished. The adapter uses `canonicalizeJson` to validate and deterministically serialize UTF-8 JSON
(sorted object keys and compact representation), and `sha256Hex` to hash canonical bytes.
Stored snapshots are rejected unless their payload is already in canonical form and the
provided SHA-256 matches. JSON parsing/canonicalization is implemented with nlohmann-json.
