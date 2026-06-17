// Round-trip test for the ITrace<->Arrow path (ddm-ajf + ddm-85v).
//
// SimpleTrace --to_arrow--> RecordBatch --ArrowTrace--> ITrace, then check the
// facade reports the original channel/tbin/charge.

#include "wire_cell_arrow/Converters.hpp"
#include "wire_cell_arrow/ArrowTrace.hpp"

#include "WireCellAux/SimpleTrace.h"

#include <iostream>
#include <memory>
#include <vector>

using WireCell::ITrace;
using WireCell::Aux::SimpleTrace;
using WireCell::Arrow::ArrowTrace;

static int fail(const std::string& msg)
{
    std::cerr << "FAIL: " << msg << "\n";
    return 1;
}

static int roundtrip(int chid, int tbin, const std::vector<float>& q)
{
    ITrace::pointer in = std::make_shared<SimpleTrace>(chid, tbin, q);
    auto result = WireCell::Arrow::to_arrow(in);
    if (!result.ok()) return fail("to_arrow failed: " + result.status().ToString());

    ArrowTrace out(*result, 0);
    if (out.channel() != chid) return fail("channel mismatch");
    if (out.tbin() != tbin)    return fail("tbin mismatch");

    const auto& qc = out.charge();
    if (qc.size() != q.size()) return fail("charge size mismatch");
    for (size_t i = 0; i < q.size(); ++i) {
        if (qc[i] != q[i]) return fail("charge value mismatch at " + std::to_string(i));
    }
    // Second call must return the same cached vector (same data, no re-copy bug).
    if (&out.charge() != &qc) return fail("charge() should return the cached reference");
    return 0;
}

int main()
{
    if (int rc = roundtrip(42, 10, {1.5f, -2.0f, 3.25f, 0.0f, 4.0f})) return rc;
    if (int rc = roundtrip(7, 0, {})) return rc;            // empty charge
    if (int rc = roundtrip(-1, 100, {0.0f})) return rc;     // sentinel channel

    std::cout << "ArrowTrace round-trip OK\n";
    return 0;
}
