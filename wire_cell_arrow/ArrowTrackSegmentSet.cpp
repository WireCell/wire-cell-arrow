#include "wire_cell_arrow/ArrowTrackSegmentSet.hpp"
#include "wire_cell_arrow/ArrowTrackSegment.hpp"
#include "wire_cell_arrow/Converters.hpp"

#include <stdexcept>

namespace WireCell::Arrow {

ArrowTrackSegmentSet::ArrowTrackSegmentSet(std::shared_ptr<arrow::Table> table)
{
    if (!table) throw std::invalid_argument("ArrowTrackSegmentSet: null table");
    auto br = table_to_batch(table);
    if (!br.ok()) throw std::runtime_error("ArrowTrackSegmentSet: " + br.status().ToString());
    m_batch = *br;
    require_readable_schema(m_batch->schema(), "wc.tracksegmentset");
    if (auto md = m_batch->schema()->metadata()) {
        auto got = md->Get("wc.tracksegmentset.ident");
        if (got.ok()) m_ident = std::stoi(*got);
    }
}

ArrowTrackSegmentSet::~ArrowTrackSegmentSet() {}

int ArrowTrackSegmentSet::ident() const { return m_ident; }

WireCell::ITrackSegment::shared_vector ArrowTrackSegmentSet::segments() const
{
    if (!m_loaded) {
        auto vec = std::make_shared<WireCell::ITrackSegment::vector>();
        const int64_t n = m_batch->num_rows();
        vec->reserve(n);
        for (int64_t r = 0; r < n; ++r) {
            vec->push_back(std::make_shared<ArrowTrackSegment>(m_batch, r));
        }
        m_segments = vec;
        m_loaded = true;
    }
    return m_segments;
}

}  // namespace WireCell::Arrow
