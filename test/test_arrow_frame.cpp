// Round-trip test for the IFrame<->Arrow path (ddm-c2g + ddm-0ow/ddm-one).
//
// IFrame --to_arrow_sparse/dense--> bundle --ArrowFrame--> IFrame, checking
// ident/time/tick, tags, tagged_traces/summary, masks, and traces().

#include "wire_cell_arrow/Converters.h"
#include "wire_cell_arrow/ArrowFrame.h"

#include "WireCellAux/SimpleFrame.h"
#include "WireCellAux/SimpleTrace.h"
#include "WireCellUtil/Waveform.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <vector>

using WireCell::IFrame;
using WireCell::ITrace;
using WireCell::Aux::SimpleFrame;
using WireCell::Aux::SimpleTrace;
using WireCell::Arrow::ArrowFrame;

static int fail(const std::string& m) { std::cerr << "FAIL: " << m << "\n"; return 1; }
static bool has(const IFrame::tag_list_t& v, const std::string& s)
{ return std::find(v.begin(), v.end(), s) != v.end(); }

int main()
{
    // ---- sparse ----
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

    auto r = WireCell::Arrow::to_arrow_sparse(IFrame::pointer(sf));
    if (!r.ok()) return fail("to_arrow_sparse: " + r.status().ToString());
    ArrowFrame af(*r);

    if (af.ident() != 99) return fail("ident");
    if (af.time() != ftime) return fail("time round-trip");
    if (af.tick() != ftick) return fail("tick round-trip");

    if (!has(af.frame_tags(), "solid") || af.frame_tags().size() != 1) return fail("frame_tags");
    if (!has(af.trace_tags(), "loose") || !has(af.trace_tags(), "bare")) return fail("trace_tags");

    if (af.tagged_traces("loose") != IFrame::trace_list_t{0, 1}) return fail("tagged loose");
    if (af.tagged_traces("bare")  != IFrame::trace_list_t{1})    return fail("tagged bare");
    if (af.trace_summary("loose") != IFrame::trace_summary_t{1.5, 2.5}) return fail("summary loose");
    if (!af.trace_summary("bare").empty()) return fail("summary bare should be empty");
    if (!af.tagged_traces("nope").empty()) return fail("unknown tag should be empty");

    if (af.masks() != cmm) return fail("masks round-trip");

    auto tr = af.traces();
    if (!tr || tr->size() != 2) return fail("traces size");
    if ((*tr)[0]->channel() != 10 || (*tr)[0]->tbin() != 0) return fail("trace0 ch/tbin");
    if ((*tr)[0]->charge() != std::vector<float>{1, 2, 3}) return fail("trace0 charge");
    if ((*tr)[1]->channel() != 20 || (*tr)[1]->tbin() != 5) return fail("trace1 ch/tbin");
    if ((*tr)[1]->charge() != std::vector<float>{4, 5}) return fail("trace1 charge");
    if (af.traces() != tr) return fail("traces() should cache");

    // ---- dense (tbin comes from metadata) ----
    ITrace::vector dtraces{
        std::make_shared<SimpleTrace>(10, 4, std::vector<float>{1, 2, 3}),
        std::make_shared<SimpleTrace>(20, 4, std::vector<float>{4, 5, 6}),
    };
    auto df = std::make_shared<SimpleFrame>(7, 0.0, dtraces, 0.5);
    auto rd = WireCell::Arrow::to_arrow_dense(IFrame::pointer(df));
    if (!rd.ok()) return fail("to_arrow_dense: " + rd.status().ToString());
    ArrowFrame adf(*rd);
    if (adf.ident() != 7) return fail("dense ident");
    auto dtr = adf.traces();
    if (!dtr || dtr->size() != 2) return fail("dense traces size");
    if ((*dtr)[1]->channel() != 20) return fail("dense trace1 channel");
    if ((*dtr)[1]->tbin() != 4) return fail("dense trace1 tbin (from metadata)");
    if ((*dtr)[1]->charge() != std::vector<float>{4, 5, 6}) return fail("dense trace1 charge");

    std::cout << "ArrowFrame round-trip OK\n";
    return 0;
}
