// Unit test for is_dense() + to_arrow_dense(IFrame) (ddm-one).
//
// Dense frame -> wc.frame.dense: charge as fixed_size_list<float32>[nticks],
// one row per channel, tbin/nticks in schema metadata.  Non-dense frame is
// rejected with Status::Invalid.

#include "wire_cell_arrow/Converters.h"

#include "WireCellAux/SimpleFrame.h"
#include "WireCellAux/SimpleTrace.h"

#include <arrow/api.h>

#include <iostream>
#include <memory>
#include <vector>

using WireCell::IFrame;
using WireCell::ITrace;
using WireCell::Aux::SimpleFrame;
using WireCell::Aux::SimpleTrace;

static int fail(const std::string& msg) { std::cerr << "FAIL: " << msg << "\n"; return 1; }

int main()
{
    // Dense: two traces, same length (3) and tbin (4).
    ITrace::vector dense_traces{
        std::make_shared<SimpleTrace>(10, 4, std::vector<float>{1, 2, 3}),
        std::make_shared<SimpleTrace>(20, 4, std::vector<float>{4, 5, 6}),
    };
    auto dense = std::make_shared<SimpleFrame>(7, 0.0, dense_traces, 0.5);

    if (!WireCell::Arrow::is_dense(IFrame::pointer(dense))) return fail("dense frame not detected");

    auto result = WireCell::Arrow::to_arrow_dense(IFrame::pointer(dense));
    if (!result.ok()) return fail("to_arrow_dense: " + result.status().ToString());
    auto b = *result;
    if (auto st = b.traces->ValidateFull(); !st.ok()) return fail("ValidateFull: " + st.ToString());

    if (b.traces->num_rows() != 2) return fail("dense rows");
    auto md = b.traces->schema()->metadata();
    if (md->Get("arrow.schema").ValueOr("") != "wc.frame.dense") return fail("arrow.schema");
    if (md->Get("wc.frame.nticks").ValueOr("") != "3") return fail("nticks");
    if (md->Get("wc.frame.tbin").ValueOr("") != "4")   return fail("tbin");

    auto charge = std::static_pointer_cast<arrow::FixedSizeListArray>(
        b.traces->GetColumnByName("wc.trace.charge")->chunk(0));
    if (charge->value_length(0) != 3) return fail("fixed list size");
    auto cv = std::static_pointer_cast<arrow::FloatArray>(charge->values());
    // row 1 (channel 20) -> {4,5,6}
    const int64_t off1 = charge->value_offset(1);
    if (cv->Value(off1) != 4 || cv->Value(off1 + 1) != 5 || cv->Value(off1 + 2) != 6)
        return fail("dense charge row1");

    // Non-dense: differing sample counts.
    ITrace::vector ragged{
        std::make_shared<SimpleTrace>(10, 0, std::vector<float>{1, 2, 3}),
        std::make_shared<SimpleTrace>(20, 0, std::vector<float>{4, 5}),
    };
    auto ragged_frame = std::make_shared<SimpleFrame>(8, 0.0, ragged, 0.5);
    if (WireCell::Arrow::is_dense(IFrame::pointer(ragged_frame))) return fail("ragged should not be dense");
    if (WireCell::Arrow::to_arrow_dense(IFrame::pointer(ragged_frame)).ok())
        return fail("to_arrow_dense should reject non-dense frame");

    // Differing tbin is also non-dense.
    ITrace::vector difftbin{
        std::make_shared<SimpleTrace>(10, 0, std::vector<float>{1, 2, 3}),
        std::make_shared<SimpleTrace>(20, 1, std::vector<float>{4, 5, 6}),
    };
    auto difftbin_frame = std::make_shared<SimpleFrame>(9, 0.0, difftbin, 0.5);
    if (WireCell::Arrow::is_dense(IFrame::pointer(difftbin_frame))) return fail("diff tbin should not be dense");

    std::cout << "frame dense converter OK\n";
    return 0;
}
