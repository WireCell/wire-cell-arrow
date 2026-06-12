// Round-trip test for the IDepo<->Arrow path (ddm-fod + ddm-0w7).
//
// SimpleDepo chain --to_arrow--> wc.depo batch --ArrowDepo--> IDepo, then check
// the facade reports the original fields and walks the prior() chain.

#include "wire_cell_arrow/Converters.h"
#include "wire_cell_arrow/ArrowDepo.h"

#include "WireCellAux/SimpleDepo.h"
#include "WireCellUtil/Point.h"

#include <cmath>
#include <iostream>
#include <memory>

using WireCell::IDepo;
using WireCell::Point;
using WireCell::Aux::SimpleDepo;
using WireCell::Arrow::ArrowDepo;

static int fail(const std::string& msg)
{
    std::cerr << "FAIL: " << msg << "\n";
    return 1;
}

// Compare an IDepo facade against an expected SimpleDepo (fields only).
static int same_fields(const IDepo& got, const IDepo& want, const std::string& tag)
{
    if (got.time() != want.time())               return fail(tag + ": time");
    if (got.charge() != want.charge())            return fail(tag + ": charge");
    if (got.energy() != want.energy())            return fail(tag + ": energy");
    if (got.id() != want.id())                    return fail(tag + ": id");
    if (got.pdg() != want.pdg())                  return fail(tag + ": pdg");
    if (got.extent_long() != want.extent_long())  return fail(tag + ": extent_long");
    if (got.extent_tran() != want.extent_tran())  return fail(tag + ": extent_tran");
    if (got.pos().x() != want.pos().x())          return fail(tag + ": pos.x");
    if (got.pos().y() != want.pos().y())          return fail(tag + ": pos.y");
    if (got.pos().z() != want.pos().z())          return fail(tag + ": pos.z");
    return 0;
}

int main()
{
    auto p1 = std::make_shared<SimpleDepo>(9.0, Point(91, 92, 93), 901.0, nullptr, 9.1, 9.2, 71, 211, 9.5);
    auto p0 = std::make_shared<SimpleDepo>(5.0, Point(51, 52, 53), 501.0, p1, 5.1, 5.2, 70, 13, 5.5);
    auto d  = std::make_shared<SimpleDepo>(1.0, Point(11, 12, 13), 101.0, p0, 1.1, 1.2, 42, -11, 1.5);

    auto result = WireCell::Arrow::to_arrow(IDepo::pointer(d));
    if (!result.ok()) return fail("to_arrow failed: " + result.status().ToString());

    ArrowDepo head(*result, 0);
    if (int rc = same_fields(head, *d, "head")) return rc;
    if (&head.pos() != &head.pos()) return fail("pos() should cache");

    // Walk the prior chain: head.prior() == P0, P0.prior() == P1, P1.prior() == null.
    auto a0 = head.prior();
    if (!a0) return fail("head.prior() is null");
    if (int rc = same_fields(*a0, *p0, "prior0")) return rc;

    auto a1 = a0->prior();
    if (!a1) return fail("prior0.prior() is null");
    if (int rc = same_fields(*a1, *p1, "prior1")) return rc;

    if (a1->prior()) return fail("end of chain should be null");

    // prior() should be cached (same pointer on repeat).
    if (head.prior() != a0) return fail("prior() should cache");

    std::cout << "ArrowDepo round-trip OK\n";
    return 0;
}
