# Converter round-trip fidelity notes

This document is the per-type record of what survives the `native -> Arrow ->
native` round trip for every WCT type converted by `wire-cell-arrow` (the "N
axis" of the narrow-waist design; see `design.md` and `phlex-file-io-design.md`
section 4). The narrow-waist collapse is only valid if each native object
survives this trip, so subtle data loss hides in these converters — not in the
downstream technology serializers.

For each type below: **Preserved** = bit/value-exact after round trip,
**Normalized** = representation changed but information equivalent,
**Dropped** = intentionally not carried (with the reason).

The harness that enforces these claims is `test/test_roundtrip.cpp` (+
`test/test_helpers.h`): for every type it asserts both
`RecordBatch/Table::ValidateFull()` on the produced Arrow **and**
`from_arrow(to_arrow(x))` deep-equals `x` across all accessors. `ValidateFull()`
is mandatory because `RecordBatch::Make`/`Table::Make` do not check nested
type equality — a `list`/`struct` child mismatch only surfaces at IPC.

---

## ITrace → `wc.trace` (RecordBatch, one row per trace)

- **Preserved:** `channel` (int), `tbin` (int), `charge` (`list<float32>`).
  `float32` is exactly WCT's `ChargeSequence` element type, so charge values
  are stored without conversion.
- **Normalized:** none.
- **Dropped:** nothing — `ITrace` has no other state.
- **Facade note:** `ArrowTrace` cannot zero-copy `charge()` because the WCT
  accessor returns `const std::vector<float>&` (owned storage); it lazily
  materializes and caches the vector on first call. Scalar accessors
  (`channel`/`tbin`) are read directly from the Arrow arrays. See
  `arrow-facade-charge-copy`.

## IDepo → `wc.depo` (RecordBatch, one row per depo)

- **Preserved:** `time`, `charge`, `energy`, `pos.{x,y,z}`, `extent_long`,
  `extent_tran` (all `float64`); `id`, `pdg` (`int32`). The `prior()` chain is
  preserved in full as a `list<struct<...>>` column `wc.depo.priors`,
  most-recent-first (`priors[0]` = immediate prior); the struct fields mirror
  the flat depo columns.
- **Normalized:** the recursive `prior()` linked list becomes a flat ordered
  list of structs (semantically identical; order encodes the chain).
- **Dropped:** nothing. Note `energy()` IS persisted here even though WCT's own
  numpy serialization drops it — a deliberate fidelity gain of this path.

## ITensor → `wc.tensor` (RecordBatch, one row per tensor)

- **Preserved:** raw byte buffer `data` (`large_binary`, copied verbatim,
  `size()` bytes), `dtype` (utf8), `shape` (`list<int64>`), `order`
  (`list<int64>`; empty = C order). `metadata` (utf8) carries the
  `Configuration` as JSON, `null` when empty.
- **Normalized:** `metadata` Configuration ⇄ JSON string. Per the ddm-x29
  decision these are real columns (not schema metadata).
- **Dropped:** nothing.
- **Footgun:** `WireCell::Aux::SimpleTensor::metadata() const` dereferences a
  possibly-null pointer (a WCT bug). Construct test tensors with an explicit
  `Configuration()` to avoid the abort. `ArrowTensor::metadata()` is safe
  (returns a null `Configuration` when the column is null). See
  `wct-simpletensor-metadata-null-deref`.

## IFrame (sparse) → `wc.frame` bundle (4 tables)

A frame is a *bundle* (`FrameTables`): a `traces` table plus `frame_tags`,
`trace_tags`, `cmm` companion tables. All four travel together as the uniform
`TableGroup` once wrapped for Phlex.

- **Preserved:**
  - `traces` table: `wc.trace.{channel,tbin,charge}`, one row per trace.
  - `ident` (decimal), `time`, `tick` (**hexfloat**, so the `double`s are
    bit-exact) — stored in the **traces-table schema metadata**
    (`arrow.schema = wc.frame`).
  - `frame_tags`: `wc.frame.frame_tags.tag`.
  - `trace_tags`: `wc.frame.trace_tags.{tag, trace_index (list<int64>),
    summary (list<float64>)}`. `trace_index` references the **implicit** trace
    row order in the `traces` table.
  - `cmm` (ChannelMaskMap): `wc.frame.cmm.{label, channel, bin_start, bin_end}`
    as half-open `[bin_start, bin_end)` ranges; 0 rows when the CMM is empty.
- **Normalized:** the in-memory tag/CMM maps are flattened to relational rows
  and reconstructed by `ArrowFrame` from the companion tables. Tag *lists* are
  compared as sets in the harness (tag identity, not ordering, is meaningful).
- **Distinction kept:** a `summary` list value of `null` means "tagged with no
  summary"; an empty (length-0) list means "summary present but empty". These
  are not the same and both round-trip.
- **Dropped:** nothing for a representable frame. Companion tables may be null
  on input to `ArrowFrame`; they are then treated as empty.

## IFrame (dense) → `wc.frame.dense` bundle

Same four tables; only the traces table differs.

- **Preserved:** `charge` as `fixed_size_list<float32>[nticks]`, one row per
  channel; the common `tbin` and `nticks` live in the traces-table schema
  metadata alongside `ident`/`time`/`tick`. Companion tables are identical to
  the sparse form.
- **Precondition:** `to_arrow_dense` returns `Status::Invalid` unless the frame
  is *dense* — every trace has the **same** charge length **and** the **same**
  `tbin` (a rectangular block; an empty frame counts as dense). A uniform
  sample count alone is not sufficient. Use `is_dense(frame)` to choose the
  encoding.
- **Dropped:** nothing; dense is a lossless re-encoding of a rectangular frame.

## IDepoSet → `wc.deposet` (Table, one row per active depo)

- **Preserved:** `ident` (schema metadata `wc.deposet.ident`); the active depos
  as rows with exactly the `wc.depo` column set (priors nested per row). Full
  per-depo fidelity is as for `IDepo` above.
- **Normalized:** the set becomes a table; ordering of rows follows the depo
  vector.
- **Dropped:** nothing.

## ITensorSet → `wc.tensorset` (Table, one row per tensor)

- **Preserved:** `ident` and the set-level `metadata` (JSON) in schema
  metadata; each tensor as a row with the `wc.tensor` column set (full
  per-tensor fidelity as above).
- **Normalized:** set ⇄ table; set metadata Configuration ⇄ JSON.
- **Dropped:** nothing.

---

## Cross-cutting invariants

- **Empty collections** round-trip (0-row tables / empty frames / empty
  deposets / empty tensorsets are all exercised).
- **Floating point:** charges are `float32` (WCT's native element type, exact);
  frame `time`/`tick` use hexfloat text in metadata to keep the `double`
  bit-exact. Depo/tensor scalar `float64`s are stored as native `float64`.
- **`null` vs empty** is a real distinction in nullable list columns (notably
  `trace_tags.summary`) and is preserved.
- **Validation:** every converter output must pass `ValidateFull()`; nested
  `list`/`struct` columns (`wc.depo.priors`, `trace_tags.*`, dense charge) are
  the high-risk spots. See `arrow-nested-list-struct-builder`.
