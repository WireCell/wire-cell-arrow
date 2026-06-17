#include "wire_cell_arrow/ArrowTensor.h"
#include "wire_cell_arrow/Converters.h"

#include "WireCellUtil/Persist.h"

#include <complex>
#include <cstdint>
#include <stdexcept>

namespace WireCell::Arrow {

namespace {

template <typename ArrayType>
std::shared_ptr<ArrayType> as_typed(std::shared_ptr<arrow::Array> col,
                                    const std::string& name, arrow::Type::type expected)
{
    if (!col) throw std::invalid_argument("ArrowTensor: missing column '" + name + "'");
    if (col->type_id() != expected)
        throw std::invalid_argument("ArrowTensor: column '" + name + "' wrong type "
                                    + col->type()->ToString());
    return std::static_pointer_cast<ArrayType>(col);
}

// Map a WCT dtype string (numpy-style typestring) to a C++ type_info.
const std::type_info& dtype_typeinfo(const std::string& dt)
{
    if (dt == "i1") return typeid(std::int8_t);
    if (dt == "i2") return typeid(std::int16_t);
    if (dt == "i4") return typeid(std::int32_t);
    if (dt == "i8") return typeid(std::int64_t);
    if (dt == "u1") return typeid(std::uint8_t);
    if (dt == "u2") return typeid(std::uint16_t);
    if (dt == "u4") return typeid(std::uint32_t);
    if (dt == "u8") return typeid(std::uint64_t);
    if (dt == "f4") return typeid(float);
    if (dt == "f8") return typeid(double);
    if (dt == "c8") return typeid(std::complex<float>);
    if (dt == "c16") return typeid(std::complex<double>);
    if (dt == "b1") return typeid(bool);
    return typeid(void);
}

}  // namespace

ArrowTensor::ArrowTensor(std::shared_ptr<arrow::RecordBatch> batch, int64_t row)
  : m_batch(std::move(batch))
  , m_row(row)
{
    if (!m_batch) throw std::invalid_argument("ArrowTensor: null batch");
    if (m_row < 0 || m_row >= m_batch->num_rows())
        throw std::invalid_argument("ArrowTensor: row " + std::to_string(m_row) + " out of range");
    require_readable_schema(m_batch->schema(), "wc.tensor");

    auto byname = [&](const std::string& n) { return m_batch->GetColumnByName(n); };

    auto data  = as_typed<arrow::LargeBinaryArray>(byname("wc.tensor.data"),  "wc.tensor.data",  arrow::Type::LARGE_BINARY);
    auto dtype = as_typed<arrow::StringArray>(byname("wc.tensor.dtype"),      "wc.tensor.dtype", arrow::Type::STRING);
    auto shape = as_typed<arrow::ListArray>(byname("wc.tensor.shape"),        "wc.tensor.shape", arrow::Type::LIST);
    auto order = as_typed<arrow::ListArray>(byname("wc.tensor.order"),        "wc.tensor.order", arrow::Type::LIST);
    m_meta_col = as_typed<arrow::StringArray>(byname("wc.tensor.metadata"),   "wc.tensor.metadata", arrow::Type::STRING);

    auto view = data->GetView(m_row);
    m_data = reinterpret_cast<const std::byte*>(view.data());
    m_nbytes = view.size();

    m_dtype = dtype->GetString(m_row);

    auto shp_v = std::static_pointer_cast<arrow::Int64Array>(shape->values());
    const int64_t soff = shape->value_offset(m_row);
    const int64_t slen = shape->value_length(m_row);
    m_shape.reserve(slen);
    for (int64_t i = 0; i < slen; ++i) m_shape.push_back(static_cast<size_t>(shp_v->Value(soff + i)));

    auto ord_v = std::static_pointer_cast<arrow::Int64Array>(order->values());
    const int64_t ooff = order->value_offset(m_row);
    const int64_t olen = order->value_length(m_row);
    m_order.reserve(olen);
    for (int64_t i = 0; i < olen; ++i) m_order.push_back(static_cast<size_t>(ord_v->Value(ooff + i)));
}

ArrowTensor::~ArrowTensor() {}

const std::type_info& ArrowTensor::element_type() const { return dtype_typeinfo(m_dtype); }

size_t ArrowTensor::element_size() const
{
    // Prefer size/num_elements; fall back to parsing the dtype's trailing int.
    size_t nelem = 1;
    for (auto s : m_shape) nelem *= s;
    if (nelem > 0) return m_nbytes / nelem;
    auto pos = m_dtype.find_first_of("0123456789");
    if (pos != std::string::npos) return static_cast<size_t>(std::stoul(m_dtype.substr(pos)));
    return 0;
}

std::string ArrowTensor::dtype() const { return m_dtype; }
ITensor::shape_t ArrowTensor::shape() const { return m_shape; }
ITensor::order_t ArrowTensor::order() const { return m_order; }
const std::byte* ArrowTensor::data() const { return m_data; }
size_t ArrowTensor::size() const { return m_nbytes; }

WireCell::Configuration ArrowTensor::metadata() const
{
    if (m_meta_col->IsNull(m_row)) {
        return WireCell::Configuration();   // null
    }
    return WireCell::Persist::loads(m_meta_col->GetString(m_row));
}

}  // namespace WireCell::Arrow
