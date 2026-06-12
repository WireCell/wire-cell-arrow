// Consolidated IData<->Arrow round-trip tests (ddm-5zj), using test_helpers.
#include "test_helpers.h"

#include "wire_cell_arrow/Converters.h"
#include "wire_cell_arrow/ArrowTrace.h"
#include "wire_cell_arrow/ArrowFrame.h"
#include "wire_cell_arrow/ArrowDepoSet.h"
#include "wire_cell_arrow/ArrowTensorSet.h"

#include <iostream>

using namespace WireCell;
using namespace wcatest;

static int fails = 0;
static void check(bool ok, const char* name)
{
    std::cout << (ok ? "ok   " : "FAIL ") << name << "\n";
    if (!ok) ++fails;
}

int main()
{
    // ITrace
    {
        ITrace::pointer t = std::make_shared<Aux::SimpleTrace>(42, 7, std::vector<float>{1,2,3});
        auto r = WireCell::Arrow::to_arrow(t);
        check(r.ok() && trace_equal(*t, WireCell::Arrow::ArrowTrace(*r, 0)), "trace");
    }

    // IFrame sparse: with tags + cmm, empty, single
    for (auto [nt, name] : std::vector<std::pair<int,const char*>>{{5,"frame.sparse"},{0,"frame.empty"},{1,"frame.single"}}) {
        auto f = make_fake_frame(nt, 8, /*tags*/true, /*cmm*/true, /*sparse*/true);
        auto r = WireCell::Arrow::to_arrow_sparse(f);
        check(r.ok() && frame_equal(*f, WireCell::Arrow::ArrowFrame(*r)), name);
    }

    // IFrame dense
    {
        auto f = make_fake_frame(4, 8, true, true, /*sparse*/false);
        auto r = WireCell::Arrow::to_arrow_dense(f);
        check(r.ok() && frame_equal(*f, WireCell::Arrow::ArrowFrame(*r)), "frame.dense");
    }

    // IDepoSet: with prior, without prior, empty
    for (auto [nd, wp, name] : std::vector<std::tuple<int,bool,const char*>>{
            {3,true,"deposet.priors"},{2,false,"deposet.noprior"},{0,true,"deposet.empty"}}) {
        auto ds = make_fake_deposet(nd, wp);
        auto r = WireCell::Arrow::to_arrow(ds);
        check(r.ok() && deposet_equal(*ds, WireCell::Arrow::ArrowDepoSet(*r)), name);
    }

    // ITensorSet: several, empty
    for (auto [nt, name] : std::vector<std::pair<int,const char*>>{{3,"tensorset"},{0,"tensorset.empty"}}) {
        auto ts = make_fake_tensorset(nt);
        auto r = WireCell::Arrow::to_arrow(ts);
        check(r.ok() && tensorset_equal(*ts, WireCell::Arrow::ArrowTensorSet(*r)), name);
    }

    std::cout << (fails ? "ROUNDTRIP FAILURES\n" : "all round-trips OK\n");
    return fails ? 1 : 0;
}
