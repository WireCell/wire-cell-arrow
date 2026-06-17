# wire-cell-arrow

Arrow schema definitions and converters for Wire-Cell Toolkit (WCT) data types.

This package provides:
- Apache Arrow schemas for WCT `IData` types (`IFrame`, `ITrace`, `IDepo`, `IDepoSet`, `ITensor`, `ITensorSet`)
- Converters from WCT `IData` objects to Arrow `Table`/`RecordBatch` objects
- `ArrowXxx` concrete classes implementing WCT interfaces as lazy facades over Arrow objects
- Collection operators for decomposing and reassembling Arrow container types

See `docs/design.md` for schema rationale, `docs/api.md` for usage examples,
`docs/fidelity.md` for the per-type round-trip fidelity notes (what each
converter preserves, normalizes, or drops), and `docs/schema-versioning.md` for
the semantic schema version, compatibility policy, and schema registry.

## Prerequisites

- Wire-Cell Toolkit (WCT)
- Apache Arrow (`arrow-cpp` via Spack)
- C++23 compiler (GCC 15+ via Spack view)

## Build

The `default` CMake preset pins the Spack GCC 15 toolchain via `$CC`/`$CXX`
(the Debian system GCC 12 lacks the C++23 features the WCT/phlex headers need).
Set them from the Spack store — the umbrella `.envrc` does this:

```bash
export CC="$(spack -e wcph location -i gcc@15)/bin/gcc"
export CXX="$(spack -e wcph location -i gcc@15)/bin/g++"

cmake --preset default -S source/wire-cell-arrow -B builds/wire-cell-arrow
cmake --build builds/wire-cell-arrow
ctest --test-dir builds/wire-cell-arrow
```

## Components

- `wire_cell_arrow/Converters.{hpp,cpp}` — schema factories (`trace_schema`,
  `depo_schema`, `tensor_schema`, …) and `to_arrow(...)` converters (WCT → Arrow).
- `wire_cell_arrow/Arrow{Trace,Depo,Tensor,Frame,DepoSet,TensorSet}.{hpp,cpp}` —
  lazy facades implementing the WCT interfaces over Arrow objects (Arrow → WCT).
- `wire_cell_arrow/Ops.{hpp,cpp}` — collection operators (decompose/reassemble).

The C++ namespace is `WireCell::Arrow` (capital `A`, so an unqualified `arrow::`
refers to Apache Arrow).
