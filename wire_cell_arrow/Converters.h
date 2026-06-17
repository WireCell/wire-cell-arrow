#ifndef WIRE_CELL_ARROW_CONVERTERS_H
#define WIRE_CELL_ARROW_CONVERTERS_H

#include "WireCellIface/ITrace.h"
#include "WireCellIface/IDepo.h"
#include "WireCellIface/IDepoSet.h"
#include "WireCellIface/ITensor.h"
#include "WireCellIface/ITensorSet.h"
#include "WireCellIface/IFrame.h"

#include <arrow/api.h>

#include <string>

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

/// The wc.deposet schema: identical columns to wc.depo, but schema metadata
/// arrow.schema = "wc.deposet" and wc.deposet.ident = <ident>.
std::shared_ptr<arrow::Schema> deposet_schema(int ident);

/// Convert an IDepoSet to a wc.deposet Table: one row per active depo (same row
/// schema as wc.depo, priors nested per row), set ident in schema metadata.
arrow::Result<std::shared_ptr<arrow::Table>>
to_arrow(const WireCell::IDepoSet::pointer& deposet);

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

/// The wc.tensorset schema: identical columns to wc.tensor, with schema
/// metadata arrow.schema = "wc.tensorset", wc.tensorset.ident, and (when the
/// set has metadata) wc.tensorset.metadata JSON.
std::shared_ptr<arrow::Schema> tensorset_schema(int ident, const std::string& metadata_json = {});

/// Convert an ITensorSet to a wc.tensorset Table: one row per tensor (same row
/// schema as wc.tensor), set ident + metadata in schema metadata.
arrow::Result<std::shared_ptr<arrow::Table>>
to_arrow(const WireCell::ITensorSet::pointer& tensorset);

// ---------------------------------------------------------------------------
// Semantic schema versioning / governance (ddm-c3s.9)
//
// Every wc.* schema carries its semantic version in schema metadata under
// `kSchemaVersionKey`, paired with the `kSchemaNameKey` ("arrow.schema") name.
// The version is a single integer (a "generation"): additive, backward-safe
// changes keep it; a breaking change bumps it.  See docs/schema-versioning.md
// for the compatibility policy and the schema registry.
// ---------------------------------------------------------------------------

/// The semantic schema version produced by this build of wire-cell-arrow.
inline constexpr int kSchemaVersion = 1;
inline constexpr char kSchemaNameKey[] = "arrow.schema";
inline constexpr char kSchemaVersionKey[] = "arrow.schema.version";

/// The semantic schema version recorded in `schema`'s metadata, or 0 when the
/// schema is unversioned (legacy, pre-versioning).
int schema_version(const std::shared_ptr<arrow::Schema>& schema);

/// Reader-side version gate (deliverable 4).  Accepts a schema whose version is
/// <= this build's kSchemaVersion — older files stay forward-readable because
/// schema changes within a major version are additive — and accepts an
/// unversioned (0) schema best-effort.  Throws std::runtime_error when the
/// schema's version is NEWER than this build understands.  `context` names the
/// schema for the diagnostic.
void require_readable_schema(const std::shared_ptr<arrow::Schema>& schema,
                             const std::string& context);

// ---------------------------------------------------------------------------
// wc.frame
// ---------------------------------------------------------------------------

/// Encode/decode a double as a bit-exact hexfloat string (C++ to_chars/from_chars,
/// chars_format::hex, no "0x" prefix).  Used for frame scalars (time, tick) in
/// schema metadata, where Arrow only stores strings.
std::string hexfloat(double v);
double parse_hexfloat(const std::string& s);

/// Collapse a (typically single-chunk) Table into one RecordBatch, preserving
/// schema metadata.  Used by the set/frame facades to index rows.  A 0-row
/// table yields an empty batch.
arrow::Result<std::shared_ptr<arrow::RecordBatch>>
table_to_batch(const std::shared_ptr<arrow::Table>& table);

/// A wc.frame as a bundle of standalone Arrow tables (per the ddm-li8 decision):
///   traces     : the wc.frame / wc.frame.dense trace table (frame scalars
///                ident/time/tick live in its schema metadata)
///   frame_tags : wc.frame.frame_tags     (0 rows when none)
///   trace_tags : wc.frame.trace_tags     (0 rows when none)
///   cmm        : wc.frame.cmm            (0 rows when the CMM is empty)
struct FrameTables {
    std::shared_ptr<arrow::Table> traces;
    std::shared_ptr<arrow::Table> frame_tags;
    std::shared_ptr<arrow::Table> trace_tags;
    std::shared_ptr<arrow::Table> cmm;
};

/// Convert an IFrame to its sparse wc.frame bundle.  Each trace is one row of
/// the traces table (charge as list<float32>); ident/time/tick are stored in
/// the traces-table schema metadata (arrow.schema=wc.frame).  Tags and CMM are
/// emitted as the companion tables.  General-case converter (any traces).
///
/// (The issue's stated single-Table return predates the ddm-li8 bundle
/// decision; this returns the full bundle.)
arrow::Result<FrameTables>
to_arrow_sparse(const WireCell::IFrame::pointer& frame);

/// True if the frame can be stored densely: every trace has the same charge
/// length AND the same tbin (a rectangular block).  An empty frame is dense.
/// Dense storage hoists the common tbin into metadata, so a uniform tbin is
/// required (not just a uniform sample count).
bool is_dense(const WireCell::IFrame::pointer& frame);

/// Convert a dense IFrame to its wc.frame.dense bundle: one row per channel,
/// charge as fixed_size_list<float32>[nticks]; the common tbin and nticks live
/// in the traces-table schema metadata (arrow.schema=wc.frame.dense) alongside
/// ident/time/tick.  Tags/CMM companion tables are identical to the sparse
/// form.  Returns Status::Invalid if the frame is not dense.
arrow::Result<FrameTables>
to_arrow_dense(const WireCell::IFrame::pointer& frame);

}  // namespace WireCell::Arrow

#endif  // WIRE_CELL_ARROW_CONVERTERS_H
