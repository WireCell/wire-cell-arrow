#ifndef WIRE_CELL_ARROW_ARROWTRACE_H
#define WIRE_CELL_ARROW_ARROWTRACE_H

#include "WireCellIface/ITrace.h"

#include <arrow/api.h>

#include <memory>

namespace WireCell::Arrow {

/// An ITrace backed by one row of a wc.trace / wc.frame (sparse or dense) batch.
///
/// Columns are resolved by name (wc.trace.channel / .tbin / .charge).  Works on:
///   - a standalone wc.trace batch or a sparse wc.frame row: charge is
///     list<float32>, tbin from the wc.trace.tbin column;
///   - a dense wc.frame.dense row: charge is fixed_size_list<float32>, and
///     (since dense omits the per-trace tbin column) tbin is read from the
///     batch schema metadata key "wc.frame.tbin".
/// The batch is retained for this object's lifetime.
///
/// channel()/tbin() read directly (no copy).  charge() returns
/// const std::vector<float>&, which cannot alias Arrow's buffer, so it is
/// lazily materialised and cached on first call.
class ArrowTrace : public WireCell::ITrace {
  public:
    /// Wrap row `row` of `batch`.  Throws std::invalid_argument if the batch
    /// lacks the expected columns or they have the wrong type.
    ArrowTrace(std::shared_ptr<arrow::RecordBatch> batch, int64_t row = 0);
    virtual ~ArrowTrace();

    virtual int channel() const;
    virtual int tbin() const;
    virtual const ChargeSequence& charge() const;

  private:
    std::shared_ptr<arrow::RecordBatch> m_batch;
    int64_t m_row;
    std::shared_ptr<arrow::Int32Array> m_channel;
    std::shared_ptr<arrow::Int32Array> m_tbin_col;   // null for dense frames
    int m_tbin_const{0};                             // used when m_tbin_col is null
    std::shared_ptr<arrow::Array> m_charge;          // ListArray or FixedSizeListArray
    bool m_charge_fixed{false};

    mutable ChargeSequence m_charge_cache;
    mutable bool m_charge_loaded{false};
};

}  // namespace WireCell::Arrow

#endif  // WIRE_CELL_ARROW_ARROWTRACE_H
