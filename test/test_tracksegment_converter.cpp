// Round-trip test for the ITrackSegmentSet<->Arrow path (xerosere ddm-69y.11).
//
// SimpleTrackSegmentSet --to_arrow--> wc.tracksegmentset Table
// --ArrowTrackSegmentSet--> ITrackSegmentSet, then check the facade reports
// the original fields.  Also checks the version gate and that extra columns
// (as a re-stamped edep.segments table would carry) are tolerated.

#include "wire_cell_arrow/Converters.hpp"
#include "wire_cell_arrow/ArrowTrackSegmentSet.hpp"

#include "WireCellAux/SimpleTrackSegment.h"
#include "WireCellAux/SimpleTrackSegmentSet.h"
#include "WireCellUtil/Units.h"

#include <iostream>
#include <memory>
#include <string>

using namespace WireCell;
using WireCell::Aux::SimpleTrackSegment;
using WireCell::Aux::SimpleTrackSegmentSet;

static int fail(const std::string& msg)
{
    std::cerr << "FAIL: " << msg << "\n";
    return 1;
}

static int same_fields(const ITrackSegment& got, const ITrackSegment& want, const std::string& tag)
{
    if (got.start() != want.start())               return fail(tag + ": start");
    if (got.stop() != want.stop())                 return fail(tag + ": stop");
    if (got.start_time() != want.start_time())     return fail(tag + ": start_time");
    if (got.stop_time() != want.stop_time())       return fail(tag + ": stop_time");
    if (got.energy() != want.energy())             return fail(tag + ": energy");
    if (got.secondary() != want.secondary())       return fail(tag + ": secondary");
    if (got.n_electrons() != want.n_electrons())   return fail(tag + ": n_electrons");
    if (got.track_length() != want.track_length()) return fail(tag + ": track_length");
    if (got.id() != want.id())                     return fail(tag + ": id");
    if (got.pdg() != want.pdg())                   return fail(tag + ": pdg");
    return 0;
}

int main()
{
    ITrackSegment::vector segs;
    for (int ind = 0; ind < 3; ++ind) {
        const Point start(ind * units::mm, 2 * units::mm, 3 * units::mm);
        const Point stop(ind * units::mm, 2 * units::mm, (4 + ind) * units::mm);
        segs.push_back(std::make_shared<SimpleTrackSegment>(
            start, stop, ind * units::ns, (ind + 1) * units::ns,
            0.21 * (ind + 1) * units::MeV, 0.07 * units::MeV,
            6.0e3 * (ind + 1), (1.5 + ind) * units::mm, 10 + ind, ind % 2 ? 13 : 11));
    }
    auto orig = std::make_shared<SimpleTrackSegmentSet>(42, segs);

    // Convert to Arrow.
    auto tr = WireCell::Arrow::to_arrow(ITrackSegmentSet::pointer(orig));
    if (!tr.ok()) return fail("to_arrow: " + tr.status().ToString());
    auto table = *tr;
    if (!table->ValidateFull().ok()) return fail("table does not validate");
    if (table->num_rows() != 3) return fail("row count");

    // Read back through the facade.
    WireCell::Arrow::ArrowTrackSegmentSet back(table);
    if (back.ident() != 42) return fail("ident");
    auto got = back.segments();
    if (!got || got->size() != 3) return fail("segments size");
    for (size_t ind = 0; ind < 3; ++ind) {
        if (same_fields(*got->at(ind), *segs[ind], "row " + std::to_string(ind))) return 1;
    }

    // Empty set round-trips.
    auto empty_tr = WireCell::Arrow::to_arrow(
        ITrackSegmentSet::pointer(std::make_shared<SimpleTrackSegmentSet>(7, ITrackSegment::vector{})));
    if (!empty_tr.ok()) return fail("empty to_arrow");
    WireCell::Arrow::ArrowTrackSegmentSet empty(*empty_tr);
    if (empty.ident() != 7) return fail("empty ident");
    if (empty.segments()->size() != 0) return fail("empty segments");

    // Extra columns are tolerated (a projected/renamed/re-stamped foreign
    // table, e.g. from edep.segments, may carry more than the facade needs).
    {
        arrow::DoubleBuilder extra_b;
        for (int ind = 0; ind < 3; ++ind) {
            if (!extra_b.Append(ind).ok()) return fail("extra append");
        }
        std::shared_ptr<arrow::Array> extra;
        if (!extra_b.Finish(&extra).ok()) return fail("extra finish");
        auto res = table->AddColumn(table->num_columns(),
                                    arrow::field("extra", arrow::float64(), false),
                                    std::make_shared<arrow::ChunkedArray>(extra));
        if (!res.ok()) return fail("AddColumn");
        WireCell::Arrow::ArrowTrackSegmentSet extended(*res);
        if (extended.segments()->size() != 3) return fail("extended size");
        if (same_fields(*extended.segments()->at(1), *segs[1], "extended row 1")) return 1;
    }

    // The version gate rejects a future schema.
    {
        auto md = std::make_shared<arrow::KeyValueMetadata>(
            std::vector<std::string>{"arrow.schema", "arrow.schema.version"},
            std::vector<std::string>{"wc.tracksegmentset", "999"});
        try {
            WireCell::Arrow::ArrowTrackSegmentSet bad(table->ReplaceSchemaMetadata(md));
            return fail("future-version schema not rejected");
        }
        catch (const std::runtime_error&) {
        }
    }

    std::cout << "PASS: tracksegment round-trip\n";
    return 0;
}
