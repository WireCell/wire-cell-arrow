#include "wire_cell_arrow/ArrowTensorSet.hpp"
#include "wire_cell_arrow/ArrowTensor.hpp"
#include "wire_cell_arrow/Converters.hpp"

#include "WireCellUtil/Persist.h"

#include <stdexcept>

namespace WireCell::Arrow {

ArrowTensorSet::ArrowTensorSet(std::shared_ptr<arrow::Table> table)
{
    if (!table) throw std::invalid_argument("ArrowTensorSet: null table");
    auto br = table_to_batch(table);
    if (!br.ok()) throw std::runtime_error("ArrowTensorSet: " + br.status().ToString());
    m_batch = *br;
    require_readable_schema(m_batch->schema(), "wc.tensorset");
    if (auto md = m_batch->schema()->metadata()) {
        auto id = md->Get("wc.tensorset.ident");
        if (id.ok()) m_ident = std::stoi(*id);
        auto mj = md->Get("wc.tensorset.metadata");
        if (mj.ok()) m_md = WireCell::Persist::loads(*mj);
    }
}

ArrowTensorSet::~ArrowTensorSet() {}

int ArrowTensorSet::ident() const { return m_ident; }
WireCell::Configuration ArrowTensorSet::metadata() const { return m_md; }

WireCell::ITensor::shared_vector ArrowTensorSet::tensors() const
{
    if (!m_loaded) {
        auto vec = std::make_shared<WireCell::ITensor::vector>();
        const int64_t n = m_batch->num_rows();
        vec->reserve(n);
        for (int64_t r = 0; r < n; ++r) {
            vec->push_back(std::make_shared<ArrowTensor>(m_batch, r));
        }
        m_tensors = vec;
        m_loaded = true;
    }
    return m_tensors;
}

}  // namespace WireCell::Arrow
