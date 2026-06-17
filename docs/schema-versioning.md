# Semantic Arrow schema versioning & governance

The intermediate Arrow representation embeds **semantic** schema names and
metadata (`arrow.schema = wc.frame`, frame scalars in schema metadata, etc.).
These names + column conventions are the de-facto **on-disk contract**,
independent of which technology (HDF5, Parquet, RNTuple, …) stores the bytes.
This document defines how that contract is versioned so old files stay readable
as the schemas evolve.

## Mechanism

Every `wc.*` schema carries its version in schema metadata, alongside its name:

| key                    | meaning                                   |
|------------------------|-------------------------------------------|
| `arrow.schema`         | semantic schema name (e.g. `wc.frame`)    |
| `arrow.schema.version` | integer schema *generation* (e.g. `1`)    |

The build's current value is `WireCell::Arrow::kSchemaVersion` (Converters.h);
the keys are `kSchemaNameKey` / `kSchemaVersionKey`. All schemas are produced
through one helper (`semantic_metadata()` in Converters.cpp), so versioning is
uniform and cannot be forgotten for a new schema.

The version is a single integer, not semver. Think of it as a **generation
counter** for the whole `wc.*` schema family: it advances only on a *breaking*
change (see below). Within a generation, schemas may still gain additive,
backward-safe extensions without a bump.

## Compatibility policy

A change is **additive / backward-safe** (NO version bump) when an old reader
can still read new files and a new reader can still read old files:

- adding a **nullable** column, or a column with a documented default;
- adding new **schema-metadata keys** (readers ignore unknown keys);
- adding a new, independent schema (e.g. a new companion table) that existing
  readers do not require;
- relaxing a constraint (e.g. accepting more values in an existing column).

A change is **breaking** (REQUIRES a version bump) when it invalidates the
old contract:

- removing or renaming a column or metadata key that readers rely on;
- changing a column's Arrow type, units, or semantics;
- making a previously-nullable column required, or vice-versa in a way that
  changes meaning;
- changing the meaning of an existing schema-metadata key (e.g. the encoding of
  `wc.frame.time`).

When a breaking change is made: bump `kSchemaVersion`, record the new row in the
registry below with a note on what changed, and (if feasible) teach readers to
handle the prior generation.

## Reader behavior on version mismatch

The reader gate is `WireCell::Arrow::require_readable_schema(schema, context)`,
called by every facade constructor (`ArrowFrame`, `ArrowDepoSet`,
`ArrowTensorSet`, `ArrowTrace`, `ArrowDepo`, `ArrowTensor`):

| file version vs build | behavior                                             |
|-----------------------|------------------------------------------------------|
| `file == build`       | accept (fully supported)                             |
| `file <  build`       | accept (older files are forward-readable: additive)  |
| `file >  build`       | **reject** — throw `std::runtime_error`; the message names the schema and asks the user to upgrade wire-cell-arrow |
| unversioned (`0`)     | accept best-effort (legacy, pre-versioning files)    |

`schema_version(schema)` returns the recorded integer, or `0` when the schema
carries no version (legacy). Rejecting *newer* versions is the safe default: a
newer generation may have changed required fields this build cannot interpret.

## Schema registry

Current generation: **`kSchemaVersion = 1`**. All schemas below are at version 1.

| schema (`arrow.schema`)  | kind         | produced by                         | notes |
|--------------------------|--------------|-------------------------------------|-------|
| `wc.trace`               | RecordBatch  | `to_arrow(ITrace)`                  | channel/tbin/charge |
| `wc.depo`                | RecordBatch  | `to_arrow(IDepo)`                   | priors as `list<struct>` |
| `wc.deposet`             | Table        | `to_arrow(IDepoSet)`                | `wc.deposet.ident` in metadata |
| `wc.tensor`              | RecordBatch  | `to_arrow(ITensor)`                 | raw bytes + dtype/shape/order |
| `wc.tensorset`           | Table        | `to_arrow(ITensorSet)`              | `wc.tensorset.{ident,metadata}` |
| `wc.frame`               | Table (traces) | `to_arrow_sparse(IFrame)`         | scalars (ident/time/tick) in metadata |
| `wc.frame.dense`         | Table (traces) | `to_arrow_dense(IFrame)`          | + `wc.frame.{tbin,nticks}` |
| `wc.frame.frame_tags`    | Table        | frame bundle companion              | |
| `wc.frame.trace_tags`    | Table        | frame bundle companion              | nullable `summary` |
| `wc.frame.cmm`           | Table        | frame bundle companion              | half-open `[bin_start,bin_end)` |

The per-column / per-metadata conventions for each schema are documented in
`docs/fidelity.md` (and the beads memory `arrow-frame-companion-table-naming`);
this document governs only their *versioning*.

## Tests

`test/test_schema_version.cpp` asserts: every schema carries `kSchemaVersion`;
a current-version product round-trips through its facade; a reader rejects a
deliberately-bumped (newer) version; and an unversioned (legacy) schema is
accepted best-effort.
