// Unit test for to_arrow(ITensorSet) — wc.tensorset (ddm-szy).
#include "wire_cell_arrow/Converters.h"
#include "WireCellAux/SimpleTensorSet.h"
#include "WireCellAux/SimpleTensor.h"
#include "WireCellUtil/Persist.h"
#include <arrow/api.h>
#include <iostream>
#include <memory>
#include <vector>

using WireCell::ITensor; using WireCell::ITensorSet; using WireCell::Configuration;
using WireCell::Aux::SimpleTensor; using WireCell::Aux::SimpleTensorSet;
static int fail(const std::string& m){ std::cerr<<"FAIL: "<<m<<"\n"; return 1; }

int main()
{
    std::vector<float> f{1,2,3,4,5,6};
    std::vector<int32_t> i{7,8,9};
    auto t0 = std::make_shared<SimpleTensor>(ITensor::shape_t{2,3}, f.data(), Configuration());
    auto t1 = std::make_shared<SimpleTensor>(ITensor::shape_t{3}, i.data(), Configuration());
    auto tv = std::make_shared<ITensor::vector>(ITensor::vector{t0, t1});

    Configuration setmd; setmd["job"] = "sigproc";
    ITensorSet::pointer ts = std::make_shared<SimpleTensorSet>(77, setmd, tv);

    auto r = WireCell::Arrow::to_arrow(ts);
    if (!r.ok()) return fail("to_arrow: " + r.status().ToString());
    auto t = *r;
    if (auto st = t->ValidateFull(); !st.ok()) return fail("ValidateFull: " + st.ToString());
    if (t->num_rows() != 2) return fail("rows");
    auto md = t->schema()->metadata();
    if (md->Get("arrow.schema").ValueOr("") != "wc.tensorset") return fail("arrow.schema");
    if (md->Get("wc.tensorset.ident").ValueOr("") != "77") return fail("ident");
    Configuration back = WireCell::Persist::loads(md->Get("wc.tensorset.metadata").ValueOr("null"));
    if (back["job"].asString() != "sigproc") return fail("set metadata");

    auto dt = std::static_pointer_cast<arrow::StringArray>(t->GetColumnByName("wc.tensor.dtype")->chunk(0));
    if (dt->GetString(0) != t0->dtype()) return fail("row0 dtype");
    if (dt->GetString(1) != t1->dtype()) return fail("row1 dtype");

    std::cout << "tensorset converter OK\n";
    return 0;
}
