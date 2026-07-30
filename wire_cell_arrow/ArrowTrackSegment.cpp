#include "wire_cell_arrow/ArrowTrackSegment.hpp"
#include "wire_cell_arrow/Converters.hpp"

#include <stdexcept>
#include <string>

namespace WireCell::Arrow {

namespace {

template <typename ArrayType>
std::shared_ptr<ArrayType> as_typed(std::shared_ptr<arrow::Array> col,
                                    const std::string& name, arrow::Type::type expected)
{
    if (!col) throw std::invalid_argument("ArrowTrackSegment: missing column '" + name + "'");
    if (col->type_id() != expected)
        throw std::invalid_argument("ArrowTrackSegment: column '" + name + "' wrong type "
                                    + col->type()->ToString());
    return std::static_pointer_cast<ArrayType>(col);
}

}  // namespace

ArrowTrackSegment::ArrowTrackSegment(std::shared_ptr<arrow::RecordBatch> batch, int64_t row)
  : m_batch(std::move(batch))
  , m_row(row)
{
    if (!m_batch) throw std::invalid_argument("ArrowTrackSegment: null batch");
    if (m_row < 0 || m_row >= m_batch->num_rows())
        throw std::invalid_argument("ArrowTrackSegment: row " + std::to_string(m_row) +
                                    " out of range");

    // A row facade serves both the standalone and the set-flavored schema
    // (identical columns, different arrow.schema name); the column resolution
    // below is the effective structural check.
    require_readable_schema(m_batch->schema(), "wc.tracksegment");

    auto byname = [&](const std::string& n) { return m_batch->GetColumnByName(n); };
    auto dbl = [&](const std::string& n) {
        return as_typed<arrow::DoubleArray>(byname(n), n, arrow::Type::DOUBLE);
    };

    m_start = WireCell::Point(dbl("wc.tracksegment.start_x")->Value(m_row),
                              dbl("wc.tracksegment.start_y")->Value(m_row),
                              dbl("wc.tracksegment.start_z")->Value(m_row));
    m_stop = WireCell::Point(dbl("wc.tracksegment.stop_x")->Value(m_row),
                             dbl("wc.tracksegment.stop_y")->Value(m_row),
                             dbl("wc.tracksegment.stop_z")->Value(m_row));
    m_start_t = dbl("wc.tracksegment.start_t");
    m_stop_t = dbl("wc.tracksegment.stop_t");
    m_energy = dbl("wc.tracksegment.energy");
    m_secondary = dbl("wc.tracksegment.secondary");
    m_n_electrons = dbl("wc.tracksegment.n_electrons");
    m_track_length = dbl("wc.tracksegment.track_length");
    m_id = as_typed<arrow::Int32Array>(byname("wc.tracksegment.id"), "wc.tracksegment.id",
                                       arrow::Type::INT32);
    m_pdg = as_typed<arrow::Int32Array>(byname("wc.tracksegment.pdg"), "wc.tracksegment.pdg",
                                        arrow::Type::INT32);
}

ArrowTrackSegment::~ArrowTrackSegment() {}

const WireCell::Point& ArrowTrackSegment::start() const { return m_start; }
const WireCell::Point& ArrowTrackSegment::stop() const { return m_stop; }
double ArrowTrackSegment::start_time() const { return m_start_t->Value(m_row); }
double ArrowTrackSegment::stop_time() const { return m_stop_t->Value(m_row); }
double ArrowTrackSegment::energy() const { return m_energy->Value(m_row); }
double ArrowTrackSegment::secondary() const { return m_secondary->Value(m_row); }
double ArrowTrackSegment::n_electrons() const { return m_n_electrons->Value(m_row); }
double ArrowTrackSegment::track_length() const { return m_track_length->Value(m_row); }
int ArrowTrackSegment::id() const { return m_id->Value(m_row); }
int ArrowTrackSegment::pdg() const { return m_pdg->Value(m_row); }

}  // namespace WireCell::Arrow
