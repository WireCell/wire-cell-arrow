#include "wire_cell_arrow/ArrowTrace.h"

#include <stdexcept>
#include <string>

namespace WireCell::Arrow {

namespace {
// Resolve a column by name and downcast, or throw with a clear message.
template <typename ArrayType>
std::shared_ptr<ArrayType> typed_column(const std::shared_ptr<arrow::RecordBatch>& batch,
                                        const std::string& name,
                                        arrow::Type::type expected)
{
    auto col = batch->GetColumnByName(name);
    if (!col) {
        throw std::invalid_argument("ArrowTrace: missing column '" + name + "'");
    }
    if (col->type_id() != expected) {
        throw std::invalid_argument("ArrowTrace: column '" + name + "' has unexpected type "
                                    + col->type()->ToString());
    }
    return std::static_pointer_cast<ArrayType>(col);
}
}  // namespace

ArrowTrace::ArrowTrace(std::shared_ptr<arrow::RecordBatch> batch, int64_t row)
  : m_batch(std::move(batch))
  , m_row(row)
{
    if (!m_batch) {
        throw std::invalid_argument("ArrowTrace: null batch");
    }
    if (m_row < 0 || m_row >= m_batch->num_rows()) {
        throw std::invalid_argument("ArrowTrace: row " + std::to_string(m_row)
                                    + " out of range");
    }
    m_channel     = typed_column<arrow::Int32Array>(m_batch, "wc.trace.channel", arrow::Type::INT32);
    m_tbin        = typed_column<arrow::Int32Array>(m_batch, "wc.trace.tbin",    arrow::Type::INT32);
    m_charge_list = typed_column<arrow::ListArray>(m_batch,  "wc.trace.charge",  arrow::Type::LIST);
}

ArrowTrace::~ArrowTrace() {}

int ArrowTrace::channel() const
{
    return m_channel->Value(m_row);
}

int ArrowTrace::tbin() const
{
    return m_tbin->Value(m_row);
}

const ITrace::ChargeSequence& ArrowTrace::charge() const
{
    if (!m_charge_loaded) {
        // value_slice returns the child values for this list element as a slice
        // whose raw_values() already accounts for the slice offset.
        auto slice = std::static_pointer_cast<arrow::FloatArray>(m_charge_list->value_slice(m_row));
        const float* p = slice->raw_values();
        m_charge.assign(p, p + slice->length());
        m_charge_loaded = true;
    }
    return m_charge;
}

}  // namespace WireCell::Arrow
