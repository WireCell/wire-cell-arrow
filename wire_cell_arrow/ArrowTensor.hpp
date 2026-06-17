#ifndef WIRE_CELL_ARROW_ARROWTENSOR_H
#define WIRE_CELL_ARROW_ARROWTENSOR_H

#include "WireCellIface/ITensor.h"

#include <arrow/api.h>

#include <memory>
#include <string>

namespace WireCell::Arrow {

/// An ITensor backed by one row of a wc.tensor / wc.tensorset RecordBatch.
///
/// data() returns a pointer directly into the Arrow large_binary buffer
/// (zero-copy; the batch is retained for lifetime).  dtype/shape/order are read
/// from their columns; metadata() lazily parses the JSON column into a WCT
/// Configuration (null column => null Configuration).
///
/// NOTE on alignment: data() returns a std::byte* (alignment 1).  Whether that
/// pointer is suitably aligned to reinterpret_cast to the dtype depends on the
/// physical layout (see the ddm-3b2 alignment contract: single-tensor batches +
/// ensure_alignment=64 for safe in-place typed access).
class ArrowTensor : public WireCell::ITensor {
  public:
    /// Wrap row `row` of a wc.tensor / wc.tensorset batch.  Throws
    /// std::invalid_argument on missing/mistyped columns or out-of-range row.
    ArrowTensor(std::shared_ptr<arrow::RecordBatch> batch, int64_t row = 0);
    virtual ~ArrowTensor();

    virtual const std::type_info& element_type() const;
    virtual size_t element_size() const;
    virtual std::string dtype() const;
    virtual shape_t shape() const;
    virtual order_t order() const;
    virtual const std::byte* data() const;
    virtual size_t size() const;
    virtual Configuration metadata() const;

  private:
    std::shared_ptr<arrow::RecordBatch> m_batch;   // keepalive
    int64_t m_row;

    const std::byte* m_data{nullptr};
    size_t m_nbytes{0};
    std::string m_dtype;
    shape_t m_shape;
    order_t m_order;

    std::shared_ptr<arrow::StringArray> m_meta_col;  // for lazy metadata()
};

}  // namespace WireCell::Arrow

#endif  // WIRE_CELL_ARROW_ARROWTENSOR_H
