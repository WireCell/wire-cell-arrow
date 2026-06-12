// Round-trip test for the ITensor<->Arrow path (ddm-4zr + ddm-d5c).
//
// SimpleTensor --to_arrow--> wc.tensor batch --ArrowTensor--> ITensor, then
// check shape/dtype/element_size/size/data bytes and JSON metadata.

#include "wire_cell_arrow/Converters.h"
#include "wire_cell_arrow/ArrowTensor.h"

#include "WireCellAux/SimpleTensor.h"

#include <cstring>
#include <iostream>
#include <memory>
#include <vector>

using WireCell::ITensor;
using WireCell::Configuration;
using WireCell::Aux::SimpleTensor;
using WireCell::Arrow::ArrowTensor;

static int fail(const std::string& msg)
{
    std::cerr << "FAIL: " << msg << "\n";
    return 1;
}

int main()
{
    std::vector<float> vals{1, 2, 3, 4, 5, 6};
    ITensor::shape_t shape{2, 3};
    Configuration md;
    md["tag"] = "noise";
    md["count"] = 7;

    ITensor::pointer t = std::make_shared<SimpleTensor>(shape, vals.data(), md);

    auto result = WireCell::Arrow::to_arrow(t);
    if (!result.ok()) return fail("to_arrow failed: " + result.status().ToString());

    ArrowTensor at(*result, 0);

    if (at.dtype() != t->dtype())                 return fail("dtype");
    if (at.element_size() != sizeof(float))       return fail("element_size");
    if (at.shape() != shape)                      return fail("shape");
    if (!at.order().empty())                      return fail("order should be empty (C)");
    if (at.size() != vals.size() * sizeof(float)) return fail("size bytes");
    if (at.element_type() != typeid(float))       return fail("element_type");

    if (std::memcmp(at.data(), vals.data(), at.size()) != 0) return fail("data bytes");

    auto back = at.metadata();
    if (back["tag"].asString() != "noise") return fail("metadata tag");
    if (back["count"].asInt() != 7)        return fail("metadata count");

    std::cout << "ArrowTensor round-trip OK\n";
    return 0;
}
