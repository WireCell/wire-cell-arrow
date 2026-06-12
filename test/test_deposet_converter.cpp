// Unit test for to_arrow(IDepoSet) — wc.deposet (ddm-uac).
#include "wire_cell_arrow/Converters.h"
#include "WireCellAux/SimpleDepoSet.h"
#include "WireCellAux/SimpleDepo.h"
#include "WireCellUtil/Point.h"
#include <arrow/api.h>
#include <iostream>
#include <memory>

using WireCell::IDepo; using WireCell::IDepoSet; using WireCell::Point;
using WireCell::Aux::SimpleDepo; using WireCell::Aux::SimpleDepoSet;
static int fail(const std::string& m){ std::cerr<<"FAIL: "<<m<<"\n"; return 1; }

int main()
{
    auto p0 = std::make_shared<SimpleDepo>(5.0, Point(51,52,53), 5.0, nullptr, 0,0, 70, 13, 5.5);
    auto d0 = std::make_shared<SimpleDepo>(1.0, Point(11,12,13), 1.0, p0, 0,0, 42, -11, 1.5);
    auto d1 = std::make_shared<SimpleDepo>(2.0, Point(21,22,23));   // no prior
    IDepo::vector depos{d0, d1};
    IDepoSet::pointer ds = std::make_shared<SimpleDepoSet>(55, depos);

    auto r = WireCell::Arrow::to_arrow(ds);
    if (!r.ok()) return fail("to_arrow: " + r.status().ToString());
    auto t = *r;
    if (auto st = t->ValidateFull(); !st.ok()) return fail("ValidateFull: " + st.ToString());
    if (t->num_rows() != 2) return fail("rows");
    auto md = t->schema()->metadata();
    if (md->Get("arrow.schema").ValueOr("") != "wc.deposet") return fail("arrow.schema");
    if (md->Get("wc.deposet.ident").ValueOr("") != "55") return fail("ident");

    auto time = std::static_pointer_cast<arrow::DoubleArray>(t->GetColumnByName("wc.depo.time")->chunk(0));
    if (time->Value(0) != 1.0 || time->Value(1) != 2.0) return fail("time col");
    auto priors = std::static_pointer_cast<arrow::ListArray>(t->GetColumnByName("wc.depo.priors")->chunk(0));
    if (priors->value_length(0) != 1) return fail("row0 priors len");
    if (priors->value_length(1) != 0) return fail("row1 priors len");

    std::cout << "deposet converter OK\n";
    return 0;
}
