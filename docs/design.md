# wire-cell-arrow design

## Motivation

Wire-Cell Toolkit (WCT) moves data as C++ interface objects (`IData` subclasses:
`ITrace`, `IFrame`, `IDepo`, `IDepoSet`, `ITensor`, `ITensorSet`).  Apache Arrow
gives us a columnar, language-neutral, zero-copy-friendly in-memory format with
mature IPC/Parquet/Feather serialization and a large analysis ecosystem
(pandas, DuckDB, polars).  This package defines a faithful, round-trippable
mapping between the two so WCT data can be stored, shared, and analyzed as Arrow
— and read back as live WCT objects without bulk copies.

## Naming conventions

- Every Arrow object records its logical type in **schema-level metadata** under
  the key `arrow.schema`, e.g. `arrow.schema = "wc.frame"`.
- Field (column) names are **fully prefixed** with the type, e.g.
  `wc.trace.channel`, `wc.depo.x`, `wc.frame.cmm.bin_start`.  This keeps columns
  self-describing and lets a single-object schema be reused verbatim as the rows
  of its collection.
- Per-object scalars that are constant across a collection live in schema
  metadata, not columns.  Doubles are stored as **hexfloat** (`std::to_chars`
  `chars_format::hex`) for bit-exact round-trips; ints as decimal strings.

## Schemas

### wc.trace
One row per trace: `wc.trace.channel:int32`, `wc.trace.tbin:int32`,
`wc.trace.charge:list<float32>` (all non-null).  `float32` matches WCT's
`ITrace::ChargeSequence` element type exactly, so charge copies with no
conversion.  Row position is the implicit trace index referenced by frame tags.

### wc.frame (sparse and dense)
A frame is a **bundle** of standalone Arrow tables (`FrameTables`):

- **traces** — the trace table.  Sparse (`arrow.schema=wc.frame`) reuses the
  three `wc.trace.*` columns, so `frame → traces` is a zero-copy column
  projection.  Dense (`arrow.schema=wc.frame.dense`) has one row per channel
  with `wc.trace.charge:fixed_size_list<float32>[nticks]`; the common `tbin`
  and `nticks` are hoisted into schema metadata (dense requires a uniform tbin).
  The dense `fixed_size_list` child buffer is a contiguous `N×nticks` row-major
  matrix, a natural bridge to `wc.tensor`.  `ident`/`time`/`tick` are in the
  traces-table schema metadata.
- **frame_tags** (`wc.frame.frame_tags`) — `tag:utf8`.
- **trace_tags** (`wc.frame.trace_tags`) — `tag:utf8`,
  `trace_index:list<int64>`, `summary:list<float64>` (**nullable**: null when a
  tag carries no summary; otherwise element-aligned with `trace_index`).
- **cmm** (`wc.frame.cmm`) — the `ChannelMaskMap` **flattened** to one row per
  `(label, channel, range)`: `label:utf8`, `channel:int32`,
  `bin_start:int32`, `bin_end:int32` (half-open `[start,end)`); 0 rows when empty.

Rationale: flat native columns (not nested maps or JSON blobs) keep tags/CMM
queryable by Arrow compute / DuckDB / pandas and make the C++ builders simple.
`trace_index` references the implicit trace row order, so converters must
preserve trace order.

### wc.depo / wc.deposet
One shared row schema: `time,charge,energy,x,y,z,extent_long,extent_tran:float64`,
`id,pdg:int32`, plus `wc.depo.priors:list<struct<…>>`.  The `prior()` chain is
nested **most-recent-first** in the `priors` list (empty when none) — WCT uses
priors as a per-depo linear provenance forest (no shared priors), which maps
exactly to nesting.  `energy()` is persisted (WCT's numpy serialization drops
it).  `wc.deposet` is the same columns with `arrow.schema=wc.deposet` and
`wc.deposet.ident` in metadata; `deposet ↔ depos` is a metadata-only relabel.
`float64` for coordinates preserves sub-mm precision at meter-scale detector
coordinates through drift.

### wc.tensor / wc.tensorset
One shared row schema: `data:large_binary` (the raw byte buffer verbatim),
`dtype:utf8`, `shape:list<int64>`, `order:list<int64>` (empty = C order),
`metadata:utf8` (nullable, JSON).  Raw bytes are dtype- and order-agnostic, so
this covers complex (`c8`/`c16`) and any `element_type()` with no per-dtype code
and composes cleanly into the **heterogeneous** `ITensorSet` (mixed shapes and
dtypes are just per-row values).  `wc.tensorset` adds `wc.tensorset.ident` and
`wc.tensorset.metadata` (set-level JSON) in schema metadata.

#### Alignment / zero-copy contract
`large_binary` uses int64 offsets.  To reinterpret a tensor's bytes in place
(Eigen aligned `Map`, libtorch `from_blob`) the pointer must be element-aligned;
two hazards: (a) in a packed multi-tensor column, row *i* starts at the
cumulative byte length of prior rows (not generally aligned); (b) IPC-read
buffers are only ~8-byte aligned by default.  For safe in-place typed access,
serialize a tensorset as **one single-tensor RecordBatch per tensor** and read
with `IpcReadOptions{ensure_alignment = 64}` (in-memory built buffers are
already 64-byte aligned).  Copying consumers (e.g. `H5Dwrite`) are unaffected.

## The ArrowXxx facade pattern

Each `ArrowXxx` implements a WCT interface by reading directly from a wrapped
Arrow object, which it retains for its lifetime.  Scalar/int accessors read
Arrow values with no copy.  Accessors that return `const std::vector<T>&` or
`const Point&` (e.g. `ITrace::charge()`, `IDepo::pos()`) **cannot** alias an
Arrow buffer — a `std::vector`/`Point` owns its storage — so those are lazily
materialised into a cached member on first call (one copy, reused thereafter).
Collection facades build the per-element facades lazily and cache them; the
`tbin`/`Point`/chain are reconstructed from columns or metadata.

## Collection operators

`Ops.{hpp,cpp}` provides decompose/reassemble pairs (`frame_to_traces` /
`traces_to_frame`, etc.).  Decomposition is a per-row `RecordBatch::Slice`
(zero-copy: slices share the parent buffers); reassembly concatenates the row
batches column-wise and relabels the schema metadata.  These act on the trace /
depo / tensor *row table*; frame tag/CMM companion tables travel alongside.

## Known limitations

- **Zero-copy waveform read** is not possible at the current WCT interface:
  `ITrace::charge()` returns `const std::vector<float>&`, forcing a one-time copy
  in `ArrowTrace`.  A span/pointer accessor on `ITrace` would remove it.
- **`SimpleTensor::metadata() const`** dereferences a null `unique_ptr` when the
  tensor was built without metadata (a WCT bug); construct such tensors with an
  explicit empty `Configuration`, or fix upstream.
- **Cross-endian** portability of `wc.tensor.data` is not handled (raw bytes are
  native-endian; WCT targets little-endian x86).  IPC byte-swaps typed columns
  but not opaque binary; record a `byteorder` if ever needed.
- **`traces_to_frame`** assembles a sparse-style trace table; it does not
  re-derive dense `tbin`/`nticks` metadata.
