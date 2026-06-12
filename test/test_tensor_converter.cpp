// Unit test for to_arrow(ITensor) — the wc.tensor converter (ddm-d5c).
//
// Checks schema/metadata, raw-byte round-trip of the data buffer, dtype,
// shape, order (empty = C), and JSON metadata (present + absent).

#include "wire_cell_arrow/Converters.h"

#include "WireCellAux/SimpleTensor.h"
#include "WireCellUtil/Persist.h"

#include <arrow/api.h>

#include <cstring>
#include <iostream>
#include <memory>
#include <vector>

using WireCell::ITensor;
using WireCell::Configuration;
using WireCell::Aux::SimpleTensor;

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
    auto batch = *result;

    if (auto st = batch->ValidateFull(); !st.ok()) return fail("ValidateFull: " + st.ToString());
    if (batch->num_rows() != 1 || batch->num_columns() != 5) return fail("shape of batch");
    auto sm = batch->schema()->metadata();
    if (!sm) return fail("no schema metadata");
    auto as = sm->Get("arrow.schema");
    if (!as.ok() || *as != "wc.tensor") return fail("arrow.schema != wc.tensor");

    // data: raw bytes round-trip.
    auto data = std::static_pointer_cast<arrow::LargeBinaryArray>(batch->column(0));
    auto view = data->GetView(0);
    if (static_cast<int64_t>(view.size()) != static_cast<int64_t>(vals.size() * sizeof(float)))
        return fail("data byte length");
    if (std::memcmp(view.data(), vals.data(), view.size()) != 0)
        return fail("data bytes mismatch");

    // dtype
    auto dtype = std::static_pointer_cast<arrow::StringArray>(batch->column(1));
    if (dtype->GetString(0) != t->dtype()) return fail("dtype mismatch: got " + dtype->GetString(0));

    // shape == [2,3]
    auto shp = std::static_pointer_cast<arrow::ListArray>(batch->column(2));
    auto shp_v = std::static_pointer_cast<arrow::Int64Array>(shp->values());
    if (shp->value_length(0) != 2) return fail("shape length");
    if (shp_v->Value(shp->value_offset(0) + 0) != 2 ||
        shp_v->Value(shp->value_offset(0) + 1) != 3) return fail("shape values");

    // order empty (C order)
    auto ord = std::static_pointer_cast<arrow::ListArray>(batch->column(3));
    if (ord->value_length(0) != 0) return fail("order should be empty (C order)");

    // metadata JSON present
    auto meta = std::static_pointer_cast<arrow::StringArray>(batch->column(4));
    if (meta->IsNull(0)) return fail("metadata should be present");
    Configuration back = WireCell::Persist::loads(meta->GetString(0));
    if (back["tag"].asString() != "noise") return fail("metadata tag");
    if (back["count"].asInt() != 7)        return fail("metadata count");

    // --- absent metadata -> null column ---
    // NB: pass an explicit empty (null) Configuration rather than using the
    // no-metadata SimpleTensor ctor: SimpleTensor::metadata() const dereferences
    // its unique_ptr unconditionally and would abort if no metadata was set.
    {
        ITensor::pointer t2 = std::make_shared<SimpleTensor>(shape, vals.data(), Configuration());
        auto r2 = WireCell::Arrow::to_arrow(t2);
        if (!r2.ok()) return fail("to_arrow(no-md) failed");
        auto m2 = std::static_pointer_cast<arrow::StringArray>((*r2)->column(4));
        if (!m2->IsNull(0)) return fail("absent metadata should be null");
    }

    std::cout << "tensor converter OK\n";
    return 0;
}
