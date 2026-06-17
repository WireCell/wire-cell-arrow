// Unit test for to_arrow(IDepo) — the wc.depo converter (ddm-0w7).
//
// Checks the wc.depo schema/metadata, the flat per-depo columns, and the
// nested wc.depo.priors list<struct> (prior() chain, most-recent-first),
// plus the no-prior (empty list) case.  Calls ValidateFull() to catch any
// nested type mismatch.

#include "wire_cell_arrow/Converters.hpp"

#include "WireCellAux/SimpleDepo.h"
#include "WireCellUtil/Point.h"

#include <arrow/api.h>

#include <iostream>
#include <memory>

using WireCell::IDepo;
using WireCell::Point;
using WireCell::Aux::SimpleDepo;

static int fail(const std::string& msg)
{
    std::cerr << "FAIL: " << msg << "\n";
    return 1;
}

int main()
{
    // Chain: D (head) -> P0 -> P1.  ctor args:
    // (t, pos, charge, prior, extent_long, extent_tran, id, pdg, energy)
    IDepo::pointer p1 = std::make_shared<SimpleDepo>(
        9.0, Point(91, 92, 93), 901.0, nullptr, 9.1, 9.2, 71, 211, 9.5);
    IDepo::pointer p0 = std::make_shared<SimpleDepo>(
        5.0, Point(51, 52, 53), 501.0, p1, 5.1, 5.2, 70, 13, 5.5);
    IDepo::pointer d = std::make_shared<SimpleDepo>(
        1.0, Point(11, 12, 13), 101.0, p0, 1.1, 1.2, 42, -11, 1.5);

    auto result = WireCell::Arrow::to_arrow(d);
    if (!result.ok()) return fail("to_arrow failed: " + result.status().ToString());
    auto batch = *result;

    if (auto st = batch->ValidateFull(); !st.ok()) return fail("ValidateFull: " + st.ToString());
    if (batch->num_rows() != 1)     return fail("expected 1 row");
    if (batch->num_columns() != 11) return fail("expected 11 columns");

    auto md = batch->schema()->metadata();
    if (!md) return fail("missing metadata");
    auto got = md->Get("arrow.schema");
    if (!got.ok() || *got != "wc.depo") return fail("arrow.schema != wc.depo");

    // Flat columns reflect D.
    auto time = std::static_pointer_cast<arrow::DoubleArray>(batch->column(0));
    auto x    = std::static_pointer_cast<arrow::DoubleArray>(batch->column(3));
    auto ener = std::static_pointer_cast<arrow::DoubleArray>(batch->column(2));
    auto id   = std::static_pointer_cast<arrow::Int32Array>(batch->column(8));
    auto pdg  = std::static_pointer_cast<arrow::Int32Array>(batch->column(9));
    if (time->Value(0) != 1.0)   return fail("D.time");
    if (x->Value(0) != 11.0)     return fail("D.x");
    if (ener->Value(0) != 1.5)   return fail("D.energy");
    if (id->Value(0) != 42)      return fail("D.id");
    if (pdg->Value(0) != -11)    return fail("D.pdg");

    // priors list = [P0, P1] (most-recent-first).
    auto priors  = std::static_pointer_cast<arrow::ListArray>(batch->column(10));
    if (priors->value_length(0) != 2) return fail("expected 2 priors");
    const int64_t off = priors->value_offset(0);
    auto structs = std::static_pointer_cast<arrow::StructArray>(priors->values());
    auto p_time  = std::static_pointer_cast<arrow::DoubleArray>(structs->GetFieldByName("time"));
    auto p_id    = std::static_pointer_cast<arrow::Int32Array>(structs->GetFieldByName("id"));
    auto p_x     = std::static_pointer_cast<arrow::DoubleArray>(structs->GetFieldByName("x"));
    if (p_time->Value(off + 0) != 5.0) return fail("priors[0]=P0 time");
    if (p_id->Value(off + 0)   != 70)  return fail("priors[0]=P0 id");
    if (p_x->Value(off + 0)    != 51.0)return fail("priors[0]=P0 x");
    if (p_time->Value(off + 1) != 9.0) return fail("priors[1]=P1 time");
    if (p_id->Value(off + 1)   != 71)  return fail("priors[1]=P1 id");

    // No-prior depo -> empty priors list.
    {
        IDepo::pointer lone = std::make_shared<SimpleDepo>(2.0, Point(1, 2, 3));
        auto r2 = WireCell::Arrow::to_arrow(lone);
        if (!r2.ok()) return fail("to_arrow(lone) failed");
        auto b2 = *r2;
        auto pr = std::static_pointer_cast<arrow::ListArray>(b2->column(10));
        if (pr->IsNull(0))            return fail("priors should be non-null empty list");
        if (pr->value_length(0) != 0) return fail("lone priors length should be 0");
    }

    std::cout << "depo converter OK\n";
    return 0;
}
