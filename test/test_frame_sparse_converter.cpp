// Unit test for to_arrow_sparse(IFrame) — the wc.frame bundle (ddm-0ow).
//
// Builds a SimpleFrame with traces, a frame tag, two trace tags (one with a
// summary, one without) and a CMM, then verifies the four-table bundle:
// traces (+ ident/time/tick metadata via hexfloat), frame_tags, trace_tags,
// and the flattened cmm.

#include "wire_cell_arrow/Converters.hpp"

#include "WireCellAux/SimpleFrame.h"
#include "WireCellAux/SimpleTrace.h"
#include "WireCellUtil/Waveform.h"

#include <arrow/api.h>

#include <iostream>
#include <memory>
#include <string>
#include <vector>

using WireCell::IFrame;
using WireCell::ITrace;
using WireCell::Aux::SimpleFrame;
using WireCell::Aux::SimpleTrace;

static int fail(const std::string& msg) { std::cerr << "FAIL: " << msg << "\n"; return 1; }

// Find the row in a single-chunk utf8 column equal to `want`; -1 if absent.
static int64_t find_tag(const std::shared_ptr<arrow::Table>& t, const std::string& col,
                        const std::string& want)
{
    auto arr = std::static_pointer_cast<arrow::StringArray>(t->GetColumnByName(col)->chunk(0));
    for (int64_t i = 0; i < arr->length(); ++i) if (arr->GetString(i) == want) return i;
    return -1;
}

int main()
{
    ITrace::vector traces{
        std::make_shared<SimpleTrace>(10, 0, std::vector<float>{1, 2, 3}),
        std::make_shared<SimpleTrace>(20, 5, std::vector<float>{4, 5}),
    };

    WireCell::Waveform::ChannelMaskMap cmm;
    cmm["bad"][5] = {{10, 20}, {30, 40}};
    cmm["bad"][7] = {{0, 5}};

    const double ftime = 1.25e-3, ftick = 0.5;
    auto sf = std::make_shared<SimpleFrame>(99, ftime, traces, ftick, cmm);
    sf->tag_frame("solid");
    sf->tag_traces("loose", IFrame::trace_list_t{0, 1}, IFrame::trace_summary_t{1.5, 2.5});
    sf->tag_traces("bare", IFrame::trace_list_t{1});

    auto result = WireCell::Arrow::to_arrow_sparse(IFrame::pointer(sf));
    if (!result.ok()) return fail("to_arrow_sparse: " + result.status().ToString());
    auto b = *result;

    for (auto* tbl : {&b.traces, &b.frame_tags, &b.trace_tags, &b.cmm}) {
        if (auto st = (*tbl)->ValidateFull(); !st.ok()) return fail("ValidateFull: " + st.ToString());
    }

    // traces table + frame scalar metadata.
    if (b.traces->num_rows() != 2) return fail("traces rows");
    auto md = b.traces->schema()->metadata();
    if (!md) return fail("no frame metadata");
    if (md->Get("arrow.schema").ValueOr("") != "wc.frame") return fail("arrow.schema");
    if (md->Get("wc.frame.ident").ValueOr("") != "99") return fail("ident");
    if (WireCell::Arrow::parse_hexfloat(md->Get("wc.frame.time").ValueOr("")) != ftime) return fail("time round-trip");
    if (WireCell::Arrow::parse_hexfloat(md->Get("wc.frame.tick").ValueOr("")) != ftick) return fail("tick round-trip");
    {
        auto ch = std::static_pointer_cast<arrow::Int32Array>(b.traces->GetColumnByName("wc.trace.channel")->chunk(0));
        if (ch->Value(0) != 10 || ch->Value(1) != 20) return fail("trace channels");
    }

    // frame_tags
    if (b.frame_tags->num_rows() != 1) return fail("frame_tags rows");
    if (find_tag(b.frame_tags, "wc.frame.frame_tags.tag", "solid") < 0) return fail("frame tag 'solid'");

    // trace_tags: loose (summary) + bare (no summary)
    if (b.trace_tags->num_rows() != 2) return fail("trace_tags rows");
    int64_t r_loose = find_tag(b.trace_tags, "wc.frame.trace_tags.tag", "loose");
    int64_t r_bare  = find_tag(b.trace_tags, "wc.frame.trace_tags.tag", "bare");
    if (r_loose < 0 || r_bare < 0) return fail("trace tag rows");

    auto ti  = std::static_pointer_cast<arrow::ListArray>(b.trace_tags->GetColumnByName("wc.frame.trace_tags.trace_index")->chunk(0));
    auto tiv = std::static_pointer_cast<arrow::Int64Array>(ti->values());
    if (ti->value_length(r_loose) != 2) return fail("loose trace_index len");
    if (tiv->Value(ti->value_offset(r_loose) + 0) != 0 ||
        tiv->Value(ti->value_offset(r_loose) + 1) != 1) return fail("loose trace_index vals");

    auto sm  = std::static_pointer_cast<arrow::ListArray>(b.trace_tags->GetColumnByName("wc.frame.trace_tags.summary")->chunk(0));
    if (sm->IsNull(r_loose)) return fail("loose summary should be present");
    auto smv = std::static_pointer_cast<arrow::DoubleArray>(sm->values());
    if (smv->Value(sm->value_offset(r_loose) + 0) != 1.5 ||
        smv->Value(sm->value_offset(r_loose) + 1) != 2.5) return fail("loose summary vals");
    if (!sm->IsNull(r_bare)) return fail("bare summary should be null");

    // cmm: 3 flattened rows
    if (b.cmm->num_rows() != 3) return fail("cmm rows");
    {
        auto lo = std::static_pointer_cast<arrow::Int32Array>(b.cmm->GetColumnByName("wc.frame.cmm.bin_start")->chunk(0));
        auto hi = std::static_pointer_cast<arrow::Int32Array>(b.cmm->GetColumnByName("wc.frame.cmm.bin_end")->chunk(0));
        auto cc = std::static_pointer_cast<arrow::Int32Array>(b.cmm->GetColumnByName("wc.frame.cmm.channel")->chunk(0));
        // map order: (bad,5,10-20),(bad,5,30-40),(bad,7,0-5)
        if (cc->Value(0) != 5 || lo->Value(0) != 10 || hi->Value(0) != 20) return fail("cmm row0");
        if (cc->Value(1) != 5 || lo->Value(1) != 30 || hi->Value(1) != 40) return fail("cmm row1");
        if (cc->Value(2) != 7 || lo->Value(2) != 0  || hi->Value(2) != 5)  return fail("cmm row2");
    }

    std::cout << "frame sparse converter OK\n";
    return 0;
}
