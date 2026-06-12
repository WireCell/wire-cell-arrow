#ifndef WIRE_CELL_ARROW_CONVERTERS_H
#define WIRE_CELL_ARROW_CONVERTERS_H

#include "WireCellIface/ITrace.h"

#include <arrow/api.h>

#include <memory>

namespace WireCell::Arrow {

// Note: this package's namespace is WireCell::Arrow (capital A) specifically so
// that the unqualified `arrow::` below refers to Apache Arrow, not this
// namespace.  Do not rename to a lowercase `arrow`.

/// The canonical Arrow schema for a wc.trace RecordBatch: one row per trace.
///
/// Columns:
///   wc.trace.channel : int32          (non-null)
///   wc.trace.tbin    : int32          (non-null)
///   wc.trace.charge  : list<float32>  (non-null list; items never null in
///                                       practice, child left Arrow-default
///                                       nullable so builder output matches)
/// Schema metadata: arrow.schema = "wc.trace".
///
/// This same column set is reused as the per-trace columns of wc.frame
/// (sparse), so frame<->trace projection is zero-copy.
std::shared_ptr<arrow::Schema> trace_schema();

/// Convert a single ITrace to a one-row RecordBatch conforming to wc.trace.
///
/// Copies channel, tbin and the charge sequence.  An empty charge becomes a
/// zero-length (not null) list.  float32 matches WCT's ChargeSequence element
/// type exactly, so charge values are appended without conversion.
arrow::Result<std::shared_ptr<arrow::RecordBatch>>
to_arrow(const WireCell::ITrace::pointer& trace);

}  // namespace WireCell::Arrow

#endif  // WIRE_CELL_ARROW_CONVERTERS_H
