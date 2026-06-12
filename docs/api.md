# wire-cell-arrow API guide

All functions/classes live in namespace `WireCell::Arrow` (capital `A`; an
unqualified `arrow::` is Apache Arrow).  Converters return `arrow::Result<…>`;
check `.ok()` (or use `ARROW_ASSIGN_OR_RAISE` / `.ValueOrDie()` in tests).

```cpp
#include "wire_cell_arrow/Converters.h"   // to_arrow(...), *_schema(), Ops via Ops.h
#include "wire_cell_arrow/ArrowFrame.h"   // and ArrowTrace/Depo/Tensor/DepoSet/TensorSet
#include "wire_cell_arrow/Ops.h"
```

## ITrace ↔ Arrow

```cpp
WireCell::ITrace::pointer trace = /* … */;

auto r = WireCell::Arrow::to_arrow(trace);          // -> Result<shared_ptr<RecordBatch>>
if (!r.ok()) { /* handle r.status() */ }
std::shared_ptr<arrow::RecordBatch> batch = *r;     // 1 row, schema wc.trace

WireCell::Arrow::ArrowTrace at(batch, /*row=*/0);   // ITrace backed by the batch
int ch = at.channel();
const std::vector<float>& q = at.charge();          // materialised+cached once
```

## IFrame → Arrow (sparse and dense)

```cpp
WireCell::IFrame::pointer frame = /* … */;

// General case: any traces.
auto sb = WireCell::Arrow::to_arrow_sparse(frame);          // -> Result<FrameTables>
WireCell::Arrow::FrameTables ft = *sb;
// ft.traces / ft.frame_tags / ft.trace_tags / ft.cmm  (each a shared_ptr<Table>)

// Dense case (rectangular block): guard with is_dense().
if (WireCell::Arrow::is_dense(frame)) {
    auto db = WireCell::Arrow::to_arrow_dense(frame);       // Status::Invalid if not dense
    WireCell::Arrow::FrameTables dft = *db;                 // ft.traces is wc.frame.dense
}
```

## Arrow → IFrame

```cpp
WireCell::Arrow::ArrowFrame af(ft);          // wraps the FrameTables bundle
int id = af.ident();
double t = af.time(), dt = af.tick();
auto traces = af.traces();                   // ITrace::shared_vector (lazy, cached)
const auto& even = af.tagged_traces("even"); // trace indices for a tag
const auto& summ = af.trace_summary("even"); // aligned summary (empty if none)
WireCell::Waveform::ChannelMaskMap cmm = af.masks();
```

`ArrowFrame` works for sparse and dense traces tables transparently (dense
`tbin` is read from `wc.frame.tbin` metadata).

## IDepoSet ↔ Arrow

```cpp
auto r = WireCell::Arrow::to_arrow(deposet);          // -> Result<shared_ptr<Table>> (wc.deposet)
WireCell::Arrow::ArrowDepoSet ads(*r);
auto depos = ads.depos();                             // IDepo::shared_vector (lazy)
WireCell::IDepo::pointer prior = depos->at(0)->prior();   // nested chain walked on demand
```

## ITensorSet ↔ Arrow

```cpp
auto r = WireCell::Arrow::to_arrow(tensorset);        // -> Result<shared_ptr<Table>> (wc.tensorset)
WireCell::Arrow::ArrowTensorSet ats(*r);
auto tensors = ats.tensors();                         // ITensor::shared_vector (lazy)
const std::byte* p = tensors->at(0)->data();          // pointer into the Arrow buffer (zero-copy)
auto shape = tensors->at(0)->shape();
```

## Collection operators

```cpp
// Decompose (per-row zero-copy slices) and reassemble.
auto traces = *WireCell::Arrow::frame_to_traces(ft.traces);     // vector<shared_ptr<RecordBatch>>
auto frame  = *WireCell::Arrow::traces_to_frame(traces, {id, t, dt});  // -> Table (FrameMeta{ident,time,tick})

auto depos  = *WireCell::Arrow::deposet_to_depos(deposet_table);
auto dset   = *WireCell::Arrow::depos_to_deposet(depos, /*ident=*/7);

auto tens   = *WireCell::Arrow::tensorset_to_tensors(tensorset_table);
auto tset   = *WireCell::Arrow::tensors_to_tensorset(tens, /*ident=*/9, /*metadata_json=*/"{}");
```

## Verifying a schema by its metadata

```cpp
auto md = table->schema()->metadata();
if (md && md->Get("arrow.schema").ValueOr("") == "wc.frame") { /* … */ }
```

Schema factories are also exposed for validation/building:
`trace_schema()`, `depo_schema()`, `deposet_schema(ident)`, `tensor_schema()`,
`tensorset_schema(ident, metadata_json)`, `depo_struct_type()`.

## Patterns for downstream serializers

- **Parquet / Feather / IPC**: feed the `Table`/`RecordBatch` objects directly to
  `arrow::ipc` or `parquet::arrow`.  Parquet dictionary-encodes low-cardinality
  string columns (e.g. CMM labels) automatically — no manual deduplication.
- **HDF5**: get a tensor's bytes via `ArrowTensor::data()`/`size()` (or the
  `wc.tensor.data` `LargeBinaryArray`) and hand them to `H5Dwrite` with shape
  from `shape()` and the HDF5 type chosen from `dtype()`.  `H5Dwrite` copies, so
  buffer alignment is irrelevant.
- **ROOT RNtuple**: map each column to an RNtuple field; the flat `wc.*` columns
  correspond directly, and `wc.tensor.data` can be a byte-collection field.
- **In-place typed math** (Eigen/libtorch): only safe under the alignment
  contract in `docs/design.md` (per-tensor batches + `ensure_alignment = 64`);
  otherwise copy or use unaligned maps.
