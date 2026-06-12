# wire-cell-arrow

Arrow schema definitions and converters for Wire-Cell Toolkit (WCT) data types.

This package provides:
- Apache Arrow schemas for WCT `IData` types (`IFrame`, `ITrace`, `IDepo`, `IDepoSet`, `ITensor`, `ITensorSet`)
- Converters from WCT `IData` objects to Arrow `Table`/`RecordBatch` objects
- `ArrowXxx` concrete classes implementing WCT interfaces as lazy facades over Arrow objects
- Collection operators for decomposing and reassembling Arrow container types

See `docs/design.md` for schema rationale and `docs/api.md` for usage examples.

## Prerequisites

- Wire-Cell Toolkit (WCT)
- Apache Arrow (`arrow-cpp` via Spack)
- C++23 compiler (GCC 15+ via Spack view)

## Build

```bash
cmake --preset default -S source/wire-cell-arrow -B builds/wire-cell-arrow
cmake --build builds/wire-cell-arrow
ctest --test-dir builds/wire-cell-arrow
```
