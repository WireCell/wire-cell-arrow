#ifndef WIRE_CELL_ARROW_ARROWTRACE_H
#define WIRE_CELL_ARROW_ARROWTRACE_H

#include "WireCellIface/ITrace.h"

#include <arrow/api.h>

#include <memory>

namespace WireCell::Arrow {

/// An ITrace backed by one row of a wc.trace (or wc.frame) RecordBatch.
///
/// Columns are resolved by name (wc.trace.channel / .tbin / .charge), so this
/// facade works on a standalone wc.trace batch or directly on a row of a
/// wc.frame (sparse) batch.  The batch (and thus its Arrow buffers) is retained
/// for this object's lifetime.
///
/// channel() and tbin() read directly from the int32 columns (no copy).
///
/// NOTE on charge(): ITrace::charge() returns `const std::vector<float>&`,
/// which cannot alias Arrow's buffer (a std::vector owns its storage).  So
/// charge() lazily materialises a cached vector on first call (one copy) and
/// returns a reference to it; repeated calls do not re-copy.  True zero-copy
/// waveform access would require an ITrace accessor returning a span/pointer.
class ArrowTrace : public WireCell::ITrace {
  public:
    /// Wrap row `row` of `batch`.  Throws std::invalid_argument if the batch
    /// lacks the expected wc.trace columns or they have the wrong type.
    ArrowTrace(std::shared_ptr<arrow::RecordBatch> batch, int64_t row = 0);
    virtual ~ArrowTrace();

    virtual int channel() const;
    virtual int tbin() const;
    virtual const ChargeSequence& charge() const;

  private:
    std::shared_ptr<arrow::RecordBatch> m_batch;
    int64_t m_row;
    std::shared_ptr<arrow::Int32Array> m_channel;
    std::shared_ptr<arrow::Int32Array> m_tbin;
    std::shared_ptr<arrow::ListArray> m_charge_list;

    mutable ChargeSequence m_charge;       // lazily filled cache
    mutable bool m_charge_loaded{false};
};

}  // namespace WireCell::Arrow

#endif  // WIRE_CELL_ARROW_ARROWTRACE_H
