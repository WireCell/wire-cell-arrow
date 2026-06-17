#include "wire_cell_arrow/ArrowTrace.h"
#include "wire_cell_arrow/Converters.h"

#include <stdexcept>
#include <string>

namespace WireCell::Arrow {

ArrowTrace::ArrowTrace(std::shared_ptr<arrow::RecordBatch> batch, int64_t row)
  : m_batch(std::move(batch))
  , m_row(row)
{
    if (!m_batch) {
        throw std::invalid_argument("ArrowTrace: null batch");
    }
    if (m_row < 0 || m_row >= m_batch->num_rows()) {
        throw std::invalid_argument("ArrowTrace: row " + std::to_string(m_row) + " out of range");
    }
    require_readable_schema(m_batch->schema(), "wc.trace");

    auto chan = m_batch->GetColumnByName("wc.trace.channel");
    if (!chan || chan->type_id() != arrow::Type::INT32) {
        throw std::invalid_argument("ArrowTrace: missing/bad wc.trace.channel column");
    }
    m_channel = std::static_pointer_cast<arrow::Int32Array>(chan);

    // tbin: per-row column (sparse) or, if absent, the constant from the dense
    // frame's schema metadata (wc.frame.tbin).
    auto tcol = m_batch->GetColumnByName("wc.trace.tbin");
    if (tcol) {
        if (tcol->type_id() != arrow::Type::INT32) {
            throw std::invalid_argument("ArrowTrace: wc.trace.tbin has wrong type");
        }
        m_tbin_col = std::static_pointer_cast<arrow::Int32Array>(tcol);
    } else {
        auto md = m_batch->schema()->metadata();
        if (md) {
            auto got = md->Get("wc.frame.tbin");
            if (got.ok()) m_tbin_const = std::stoi(*got);
        }
    }

    auto chg = m_batch->GetColumnByName("wc.trace.charge");
    if (!chg) {
        throw std::invalid_argument("ArrowTrace: missing wc.trace.charge column");
    }
    if (chg->type_id() == arrow::Type::LIST) {
        m_charge_fixed = false;
    } else if (chg->type_id() == arrow::Type::FIXED_SIZE_LIST) {
        m_charge_fixed = true;
    } else {
        throw std::invalid_argument("ArrowTrace: wc.trace.charge has wrong type "
                                    + chg->type()->ToString());
    }
    m_charge = chg;
}

ArrowTrace::~ArrowTrace() {}

int ArrowTrace::channel() const
{
    return m_channel->Value(m_row);
}

int ArrowTrace::tbin() const
{
    return m_tbin_col ? m_tbin_col->Value(m_row) : m_tbin_const;
}

const ITrace::ChargeSequence& ArrowTrace::charge() const
{
    if (!m_charge_loaded) {
        std::shared_ptr<arrow::Array> slice;
        if (m_charge_fixed) {
            slice = std::static_pointer_cast<arrow::FixedSizeListArray>(m_charge)->value_slice(m_row);
        } else {
            slice = std::static_pointer_cast<arrow::ListArray>(m_charge)->value_slice(m_row);
        }
        auto fa = std::static_pointer_cast<arrow::FloatArray>(slice);
        const float* p = fa->raw_values();
        m_charge_cache.assign(p, p + fa->length());
        m_charge_loaded = true;
    }
    return m_charge_cache;
}

}  // namespace WireCell::Arrow
