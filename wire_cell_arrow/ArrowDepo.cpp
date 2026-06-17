#include "wire_cell_arrow/ArrowDepo.h"
#include "wire_cell_arrow/Converters.h"

#include <stdexcept>
#include <string>

namespace WireCell::Arrow {

namespace {

template <typename ArrayType>
std::shared_ptr<ArrayType> as_typed(std::shared_ptr<arrow::Array> col,
                                    const std::string& name, arrow::Type::type expected)
{
    if (!col) throw std::invalid_argument("ArrowDepo: missing column '" + name + "'");
    if (col->type_id() != expected)
        throw std::invalid_argument("ArrowDepo: column '" + name + "' wrong type "
                                    + col->type()->ToString());
    return std::static_pointer_cast<ArrayType>(col);
}

}  // namespace

ArrowDepo::ArrowDepo(std::shared_ptr<arrow::RecordBatch> batch, int64_t row)
  : m_batch(std::move(batch))
  , m_row(row)
{
    if (!m_batch) throw std::invalid_argument("ArrowDepo: null batch");
    if (m_row < 0 || m_row >= m_batch->num_rows())
        throw std::invalid_argument("ArrowDepo: row " + std::to_string(m_row) + " out of range");
    require_readable_schema(m_batch->schema(), "wc.depo");

    auto byname = [&](const std::string& n) { return m_batch->GetColumnByName(n); };

    m_cols.time   = as_typed<arrow::DoubleArray>(byname("wc.depo.time"),        "wc.depo.time",   arrow::Type::DOUBLE);
    m_cols.charge = as_typed<arrow::DoubleArray>(byname("wc.depo.charge"),      "wc.depo.charge", arrow::Type::DOUBLE);
    m_cols.energy = as_typed<arrow::DoubleArray>(byname("wc.depo.energy"),      "wc.depo.energy", arrow::Type::DOUBLE);
    m_cols.x      = as_typed<arrow::DoubleArray>(byname("wc.depo.x"),           "wc.depo.x",      arrow::Type::DOUBLE);
    m_cols.y      = as_typed<arrow::DoubleArray>(byname("wc.depo.y"),           "wc.depo.y",      arrow::Type::DOUBLE);
    m_cols.z      = as_typed<arrow::DoubleArray>(byname("wc.depo.z"),           "wc.depo.z",      arrow::Type::DOUBLE);
    m_cols.el     = as_typed<arrow::DoubleArray>(byname("wc.depo.extent_long"), "wc.depo.extent_long", arrow::Type::DOUBLE);
    m_cols.et     = as_typed<arrow::DoubleArray>(byname("wc.depo.extent_tran"), "wc.depo.extent_tran", arrow::Type::DOUBLE);
    m_cols.id     = as_typed<arrow::Int32Array>(byname("wc.depo.id"),           "wc.depo.id",     arrow::Type::INT32);
    m_cols.pdg    = as_typed<arrow::Int32Array>(byname("wc.depo.pdg"),          "wc.depo.pdg",    arrow::Type::INT32);

    auto priors = as_typed<arrow::ListArray>(byname("wc.depo.priors"), "wc.depo.priors", arrow::Type::LIST);
    m_chain = std::static_pointer_cast<arrow::StructArray>(priors->values());
    m_chain_begin = priors->value_offset(m_row);
    m_chain_end   = m_chain_begin + priors->value_length(m_row);
    m_chain_pos   = -1;  // head

    // Resolve the struct field arrays once (bare names) for the chain.
    auto sfield = [&](const std::string& n) { return m_chain->GetFieldByName(n); };
    m_chain_cols.time   = as_typed<arrow::DoubleArray>(sfield("time"),        "priors.time",   arrow::Type::DOUBLE);
    m_chain_cols.charge = as_typed<arrow::DoubleArray>(sfield("charge"),      "priors.charge", arrow::Type::DOUBLE);
    m_chain_cols.energy = as_typed<arrow::DoubleArray>(sfield("energy"),      "priors.energy", arrow::Type::DOUBLE);
    m_chain_cols.x      = as_typed<arrow::DoubleArray>(sfield("x"),           "priors.x",      arrow::Type::DOUBLE);
    m_chain_cols.y      = as_typed<arrow::DoubleArray>(sfield("y"),           "priors.y",      arrow::Type::DOUBLE);
    m_chain_cols.z      = as_typed<arrow::DoubleArray>(sfield("z"),           "priors.z",      arrow::Type::DOUBLE);
    m_chain_cols.el     = as_typed<arrow::DoubleArray>(sfield("extent_long"), "priors.extent_long", arrow::Type::DOUBLE);
    m_chain_cols.et     = as_typed<arrow::DoubleArray>(sfield("extent_tran"), "priors.extent_tran", arrow::Type::DOUBLE);
    m_chain_cols.id     = as_typed<arrow::Int32Array>(sfield("id"),           "priors.id",     arrow::Type::INT32);
    m_chain_cols.pdg    = as_typed<arrow::Int32Array>(sfield("pdg"),          "priors.pdg",    arrow::Type::INT32);
}

ArrowDepo::ArrowDepo(std::shared_ptr<arrow::RecordBatch> keepalive, Cols cols, int64_t row,
                     std::shared_ptr<arrow::StructArray> chain, Cols chain_cols,
                     int64_t chain_begin, int64_t chain_end, int64_t chain_pos)
  : m_batch(std::move(keepalive))
  , m_cols(std::move(cols))
  , m_row(row)
  , m_chain(std::move(chain))
  , m_chain_cols(std::move(chain_cols))
  , m_chain_begin(chain_begin)
  , m_chain_end(chain_end)
  , m_chain_pos(chain_pos)
{
}

ArrowDepo::~ArrowDepo() {}

const WireCell::Point& ArrowDepo::pos() const
{
    if (!m_pos_loaded) {
        m_pos = WireCell::Point(m_cols.x->Value(m_row), m_cols.y->Value(m_row), m_cols.z->Value(m_row));
        m_pos_loaded = true;
    }
    return m_pos;
}

double ArrowDepo::time() const        { return m_cols.time->Value(m_row); }
double ArrowDepo::charge() const      { return m_cols.charge->Value(m_row); }
double ArrowDepo::energy() const      { return m_cols.energy->Value(m_row); }
int    ArrowDepo::id() const          { return m_cols.id->Value(m_row); }
int    ArrowDepo::pdg() const         { return m_cols.pdg->Value(m_row); }
double ArrowDepo::extent_long() const { return m_cols.el->Value(m_row); }
double ArrowDepo::extent_tran() const { return m_cols.et->Value(m_row); }

WireCell::IDepo::pointer ArrowDepo::prior() const
{
    if (!m_prior_loaded) {
        const int64_t next = (m_chain_pos < 0) ? m_chain_begin : (m_chain_pos + 1);
        if (m_chain && next < m_chain_end) {
            // Chain elements read their fields from the priors struct columns.
            m_prior = std::shared_ptr<const ArrowDepo>(
                new ArrowDepo(m_batch, m_chain_cols, next,
                              m_chain, m_chain_cols, m_chain_begin, m_chain_end, next));
        }
        // else: leave m_prior null (end of chain).
        m_prior_loaded = true;
    }
    return m_prior;
}

}  // namespace WireCell::Arrow
