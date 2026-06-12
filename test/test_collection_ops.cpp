// Collection operator tests (ddm-8t5): decompose -> reassemble -> compare,
// plus a zero-copy (shared-buffer) check on frame_to_traces.
#include "test_helpers.h"

#include "wire_cell_arrow/Converters.h"
#include "wire_cell_arrow/Ops.h"
#include "wire_cell_arrow/ArrowFrame.h"
#include "wire_cell_arrow/ArrowDepoSet.h"
#include "wire_cell_arrow/ArrowTensorSet.h"
#include "WireCellUtil/Persist.h"

#include <iostream>

using namespace WireCell;
using namespace wcatest;
namespace A = WireCell::Arrow;

static int fails = 0;
static void check(bool ok, const char* n){ std::cout<<(ok?"ok   ":"FAIL ")<<n<<"\n"; if(!ok)++fails; }

int main()
{
    // ---- frame <-> traces ----
    {
        auto f = make_fake_frame(4, 8, true, true, /*sparse*/true);
        auto bundle = A::to_arrow_sparse(f).ValueOrDie();
        auto traces = A::frame_to_traces(bundle.traces).ValueOrDie();
        check(traces.size() == 4, "frame_to_traces count");

        // zero-copy: row slices share the same underlying channel buffer.
        check(traces[0]->column(0)->data()->buffers[1].get()
              == traces[1]->column(0)->data()->buffers[1].get(),
              "frame_to_traces zero-copy (shared buffer)");

        auto retbl = A::traces_to_frame(traces, {f->ident(), f->time(), f->tick()}).ValueOrDie();
        A::FrameTables rb{retbl, bundle.frame_tags, bundle.trace_tags, bundle.cmm};
        check(frame_equal(*f, A::ArrowFrame(rb)), "frame round-trip via operators");
    }

    // ---- deposet <-> depos ----
    {
        auto ds = make_fake_deposet(3, true);
        auto tbl = A::to_arrow(ds).ValueOrDie();
        auto depos = A::deposet_to_depos(tbl).ValueOrDie();
        check(depos.size() == 3, "deposet_to_depos count");
        auto re = A::depos_to_deposet(depos, ds->ident()).ValueOrDie();
        check(deposet_equal(*ds, A::ArrowDepoSet(re)), "deposet round-trip via operators");
    }

    // ---- tensorset <-> tensors ----
    {
        auto ts = make_fake_tensorset(3);
        auto tbl = A::to_arrow(ts).ValueOrDie();
        auto tens = A::tensorset_to_tensors(tbl).ValueOrDie();
        check(tens.size() == 3, "tensorset_to_tensors count");
        auto re = A::tensors_to_tensorset(tens, ts->ident(),
                                          Persist::dumps(ts->metadata())).ValueOrDie();
        check(tensorset_equal(*ts, A::ArrowTensorSet(re)), "tensorset round-trip via operators");
    }

    std::cout << (fails ? "OPERATOR FAILURES\n" : "all collection ops OK\n");
    return fails ? 1 : 0;
}
