#include "wire_cell_arrow/ArrowDepoSet.h"
#include "wire_cell_arrow/ArrowDepo.h"
#include "wire_cell_arrow/Converters.h"

#include <stdexcept>

namespace WireCell::Arrow {

ArrowDepoSet::ArrowDepoSet(std::shared_ptr<arrow::Table> table)
{
    if (!table) throw std::invalid_argument("ArrowDepoSet: null table");
    auto br = table_to_batch(table);
    if (!br.ok()) throw std::runtime_error("ArrowDepoSet: " + br.status().ToString());
    m_batch = *br;
    if (auto md = m_batch->schema()->metadata()) {
        auto got = md->Get("wc.deposet.ident");
        if (got.ok()) m_ident = std::stoi(*got);
    }
}

ArrowDepoSet::~ArrowDepoSet() {}

int ArrowDepoSet::ident() const { return m_ident; }

WireCell::IDepo::shared_vector ArrowDepoSet::depos() const
{
    if (!m_loaded) {
        auto vec = std::make_shared<WireCell::IDepo::vector>();
        const int64_t n = m_batch->num_rows();
        vec->reserve(n);
        for (int64_t r = 0; r < n; ++r) {
            vec->push_back(std::make_shared<ArrowDepo>(m_batch, r));
        }
        m_depos = vec;
        m_loaded = true;
    }
    return m_depos;
}

}  // namespace WireCell::Arrow
