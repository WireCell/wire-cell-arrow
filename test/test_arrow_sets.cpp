// Round-trip tests for the set facades (ddm-5vm ArrowDepoSet, ddm-2c4 ArrowTensorSet).
#include "wire_cell_arrow/Converters.h"
#include "wire_cell_arrow/ArrowDepoSet.h"
#include "wire_cell_arrow/ArrowTensorSet.h"
#include "WireCellAux/SimpleDepoSet.h"
#include "WireCellAux/SimpleDepo.h"
#include "WireCellAux/SimpleTensorSet.h"
#include "WireCellAux/SimpleTensor.h"
#include "WireCellUtil/Point.h"
#include <iostream>
#include <memory>
#include <vector>

using namespace WireCell;
using WireCell::Aux::SimpleDepo; using WireCell::Aux::SimpleDepoSet;
using WireCell::Aux::SimpleTensor; using WireCell::Aux::SimpleTensorSet;
static int fail(const std::string& m){ std::cerr<<"FAIL: "<<m<<"\n"; return 1; }

int main()
{
    // ---- deposet ----
    auto p0 = std::make_shared<SimpleDepo>(5.0, Point(51,52,53), 5.0, nullptr, 0,0, 70, 13, 5.5);
    auto d0 = std::make_shared<SimpleDepo>(1.0, Point(11,12,13), 1.0, p0, 0,0, 42, -11, 1.5);
    auto d1 = std::make_shared<SimpleDepo>(2.0, Point(21,22,23));
    IDepoSet::pointer ds = std::make_shared<SimpleDepoSet>(55, IDepo::vector{d0, d1});
    auto rds = WireCell::Arrow::to_arrow(ds);
    if (!rds.ok()) return fail("to_arrow(deposet)");
    WireCell::Arrow::ArrowDepoSet ads(*rds);
    if (ads.ident() != 55) return fail("deposet ident");
    auto depos = ads.depos();
    if (!depos || depos->size() != 2) return fail("depos size");
    if ((*depos)[0]->time() != 1.0) return fail("depo0 time");
    if (!(*depos)[0]->prior()) return fail("depo0 prior missing");
    if ((*depos)[0]->prior()->id() != 70) return fail("depo0 prior id");
    if ((*depos)[1]->prior()) return fail("depo1 should have no prior");

    // ---- tensorset ----
    std::vector<float> f{1,2,3,4,5,6};
    std::vector<int32_t> i{7,8,9};
    auto t0 = std::make_shared<SimpleTensor>(ITensor::shape_t{2,3}, f.data(), Configuration());
    auto t1 = std::make_shared<SimpleTensor>(ITensor::shape_t{3}, i.data(), Configuration());
    auto tv = std::make_shared<ITensor::vector>(ITensor::vector{t0, t1});
    Configuration setmd; setmd["job"] = "sigproc";
    ITensorSet::pointer ts = std::make_shared<SimpleTensorSet>(77, setmd, tv);
    auto rts = WireCell::Arrow::to_arrow(ts);
    if (!rts.ok()) return fail("to_arrow(tensorset)");
    WireCell::Arrow::ArrowTensorSet ats(*rts);
    if (ats.ident() != 77) return fail("tensorset ident");
    if (ats.metadata()["job"].asString() != "sigproc") return fail("tensorset metadata");
    auto tensors = ats.tensors();
    if (!tensors || tensors->size() != 2) return fail("tensors size");
    if ((*tensors)[0]->dtype() != t0->dtype()) return fail("tensor0 dtype");
    if ((*tensors)[0]->shape() != ITensor::shape_t{2,3}) return fail("tensor0 shape");
    if ((*tensors)[1]->shape() != ITensor::shape_t{3}) return fail("tensor1 shape");

    std::cout << "Arrow set facades round-trip OK\n";
    return 0;
}
