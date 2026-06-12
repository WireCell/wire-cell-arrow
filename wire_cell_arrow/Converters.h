#ifndef WIRE_CELL_ARROW_CONVERTERS_H
#define WIRE_CELL_ARROW_CONVERTERS_H

#include "WireCellIface/ITrace.h"
#include "WireCellIface/IDepo.h"
#include "WireCellIface/ITensor.h"

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

/// The struct<> element type used for the wc.depo.priors list: one field per
/// per-depo column (bare names: time, charge, energy, x, y, z, extent_long,
/// extent_tran : float64; id, pdg : int32), all non-null.
std::shared_ptr<arrow::DataType> depo_struct_type();

/// The canonical Arrow schema for a wc.depo RecordBatch: one row per depo.
///
/// Columns (all non-null): wc.depo.{time,charge,energy,x,y,z,extent_long,
/// extent_tran} float64, wc.depo.{id,pdg} int32, and
/// wc.depo.priors list<struct<...>> (the prior() chain, most-recent-first,
/// empty when no prior).  Schema metadata arrow.schema = "wc.depo".
///
/// This same column set is reused as the rows of wc.deposet.
std::shared_ptr<arrow::Schema> depo_schema();

/// Convert a single IDepo to a one-row RecordBatch conforming to wc.depo.
///
/// The depo's own fields populate the flat columns; the prior() chain is
/// flattened into the wc.depo.priors list as structs, most-recent-first
/// (priors[0] = immediate prior).  energy() is persisted (it is dropped by the
/// WCT numpy serialization).
arrow::Result<std::shared_ptr<arrow::RecordBatch>>
to_arrow(const WireCell::IDepo::pointer& depo);

/// The canonical Arrow schema for a wc.tensor RecordBatch: one row per tensor.
///
/// Columns: wc.tensor.data large_binary (non-null), wc.tensor.dtype utf8
/// (non-null), wc.tensor.shape list<int64> (non-null), wc.tensor.order
/// list<int64> (non-null, empty = C order), wc.tensor.metadata utf8 (nullable).
/// Schema metadata arrow.schema = "wc.tensor".  This same column set is reused
/// as the rows of wc.tensorset.
std::shared_ptr<arrow::Schema> tensor_schema();

/// Convert a single ITensor to a one-row RecordBatch conforming to wc.tensor.
///
/// The raw byte buffer (ITensor::data(), size() bytes) is copied verbatim into
/// a large_binary value; dtype/shape/order go to their columns and metadata()
/// is serialized to JSON (null when empty).  Note: per the ddm-x29 decision
/// these are COLUMNS, not schema metadata (the issue text predates that
/// decision).
arrow::Result<std::shared_ptr<arrow::RecordBatch>>
to_arrow(const WireCell::ITensor::pointer& tensor);

}  // namespace WireCell::Arrow

#endif  // WIRE_CELL_ARROW_CONVERTERS_H
