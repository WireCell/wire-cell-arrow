// Consolidated IData<->Arrow round-trip fidelity harness (ddm-5zj, ddm-c3s.7).
//
// For every WCT type converted by wire-cell-arrow this asserts the two pillars
// of the narrow-waist correctness burden:
//   (1) the produced Arrow passes RecordBatch/Table::ValidateFull()  — structure
//   (2) from_arrow(to_arrow(x)) deep-equals x across all accessors    — fidelity
// Per-type fidelity notes (what is preserved / normalized / dropped) are in
// wire-cell-arrow/docs/fidelity.md.
#include "test_helpers.h"

#include "wire_cell_arrow/ArrowDepo.h"
#include "wire_cell_arrow/ArrowDepoSet.h"
#include "wire_cell_arrow/ArrowFrame.h"
#include "wire_cell_arrow/ArrowTensor.h"
#include "wire_cell_arrow/ArrowTensorSet.h"
#include "wire_cell_arrow/ArrowTrace.h"
#include "wire_cell_arrow/Converters.h"

#include <iostream>

using namespace WireCell;
using namespace wcatest;

static int fails = 0;
static void check(bool ok, const char* name)
{
    std::cout << (ok ? "ok   " : "FAIL ") << name << "\n";
    if (!ok) ++fails;
}

// Validate the four tables of a wc.frame bundle.
static bool validate_frame_tables(const WireCell::Arrow::FrameTables& b, const std::string& tag)
{
    return validate_full(b.traces, tag + ".traces")
        && validate_full(b.frame_tags, tag + ".frame_tags")
        && validate_full(b.trace_tags, tag + ".trace_tags")
        && validate_full(b.cmm, tag + ".cmm");
}

int main()
{
    // ITrace
    {
        ITrace::pointer t = std::make_shared<Aux::SimpleTrace>(42, 7, std::vector<float>{1, 2, 3});
        auto r = WireCell::Arrow::to_arrow(t);
        bool ok = r.ok() && validate_full(*r, "trace") && trace_equal(*t, WireCell::Arrow::ArrowTrace(*r, 0));
        check(ok, "trace");
    }

    // IDepo (standalone, incl. 2-deep prior chain)
    {
        auto ds = make_fake_deposet(1, /*with_prior*/ true);
        IDepo::pointer d = ds->depos()->at(0);
        auto r = WireCell::Arrow::to_arrow(d);
        bool ok = r.ok() && validate_full(*r, "depo") && depo_equal(*d, WireCell::Arrow::ArrowDepo(*r, 0));
        check(ok, "depo");
    }

    // ITensor (standalone; explicit empty-ish metadata avoids the SimpleTensor
    // const-metadata() null-deref footgun — see test_helpers make_fake_tensorset)
    {
        auto ts = make_fake_tensorset(1);
        ITensor::pointer t = ts->tensors()->at(0);
        auto r = WireCell::Arrow::to_arrow(t);
        bool ok = r.ok() && validate_full(*r, "tensor") && tensor_equal(*t, WireCell::Arrow::ArrowTensor(*r, 0));
        check(ok, "tensor");
    }

    // IFrame sparse: with tags + cmm, empty, single
    for (auto [nt, name] : std::vector<std::pair<int, const char*>>{
           {5, "frame.sparse"}, {0, "frame.empty"}, {1, "frame.single"}}) {
        auto f = make_fake_frame(nt, 8, /*tags*/ true, /*cmm*/ true, /*sparse*/ true);
        auto r = WireCell::Arrow::to_arrow_sparse(f);
        bool ok = r.ok() && validate_frame_tables(*r, name) &&
                  frame_equal(*f, WireCell::Arrow::ArrowFrame(*r));
        check(ok, name);
    }

    // IFrame dense
    {
        auto f = make_fake_frame(4, 8, true, true, /*sparse*/ false);
        auto r = WireCell::Arrow::to_arrow_dense(f);
        bool ok = r.ok() && validate_frame_tables(*r, "frame.dense") &&
                  frame_equal(*f, WireCell::Arrow::ArrowFrame(*r));
        check(ok, "frame.dense");
    }

    // IDepoSet: with prior, without prior, empty
    for (auto [nd, wp, name] : std::vector<std::tuple<int, bool, const char*>>{
           {3, true, "deposet.priors"}, {2, false, "deposet.noprior"}, {0, true, "deposet.empty"}}) {
        auto ds = make_fake_deposet(nd, wp);
        auto r = WireCell::Arrow::to_arrow(ds);
        bool ok = r.ok() && validate_full(*r, name) &&
                  deposet_equal(*ds, WireCell::Arrow::ArrowDepoSet(*r));
        check(ok, name);
    }

    // ITensorSet: several, empty
    for (auto [nt, name] : std::vector<std::pair<int, const char*>>{
           {3, "tensorset"}, {0, "tensorset.empty"}}) {
        auto ts = make_fake_tensorset(nt);
        auto r = WireCell::Arrow::to_arrow(ts);
        bool ok = r.ok() && validate_full(*r, name) &&
                  tensorset_equal(*ts, WireCell::Arrow::ArrowTensorSet(*r));
        check(ok, name);
    }

    std::cout << (fails ? "ROUNDTRIP FAILURES\n" : "all round-trips OK\n");
    return fails ? 1 : 0;
}
