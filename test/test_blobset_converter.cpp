// Unit test for to_arrow(IBlobSet::vector) — wc.blobs (xerosere ddm-3bu.1).
//
// Builds a genuine RayGrid::Blob (via symmetric_raypairs + tiling) so its
// strips()/corners() are real, wraps it in SimpleBlob/SimpleBlobSet, converts
// the vector to a wc.blobs Table, then checks ValidateFull, num_rows, the flat
// scalar columns and the two nested list<struct> columns (strips, corners)
// against the source blob.  Also checks the empty-vector case.

#include "wire_cell_arrow/Converters.hpp"

#include "WireCellAux/SimpleBlob.h"
#include "WireCellAux/SimpleSlice.h"

#include "WireCellUtil/RayHelpers.h"
#include "WireCellUtil/RayTiling.h"
#include "WireCellUtil/RayGrid.h"
#include "WireCellUtil/Units.h"
#include "WireCellUtil/Point.h"

#include <arrow/api.h>

#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace WireCell;
using WireCell::Aux::SimpleBlob;
using WireCell::Aux::SimpleBlobSet;
using WireCell::Aux::SimpleSlice;

static int fail(const std::string& msg)
{
    std::cerr << "FAIL: " << msg << "\n";
    return 1;
}

// Build one real, area-having blob near the detector center.
static RayGrid::Blob make_test_blob()
{
    auto raypairs = RayGrid::symmetric_raypairs(100 * units::cm, 100 * units::cm, 3 * units::mm);
    RayGrid::Coordinates coords(raypairs);

    std::vector<Point> pts;
    for (double y = 48; y <= 52; y += 1.0) {
        for (double z = 48; z <= 52; z += 1.0) {
            pts.emplace_back(0.0, y * units::cm, z * units::cm);
        }
    }
    auto measures = RayGrid::make_measures(coords, pts);
    auto activities = RayGrid::make_activities(coords, measures);
    auto blobs = RayGrid::make_blobs(coords, activities);
    if (blobs.empty()) {
        throw std::runtime_error("test setup: no blobs tiled");
    }
    // Pick the first blob that actually has corners (an area).
    for (auto& b : blobs) {
        if (!b.corners().empty()) return b;
    }
    return blobs.front();
}

int main()
{
    const RayGrid::Blob shape = make_test_blob();
    if (shape.strips().empty()) return fail("test blob has no strips");
    if (shape.corners().empty()) return fail("test blob has no corners");

    // set 0: has a slice; one blob (face null).
    auto slice0 = std::make_shared<SimpleSlice>(nullptr, /*ident=*/5, /*start=*/100.0 * units::us, /*span=*/1.0);
    auto blob0 = std::make_shared<SimpleBlob>(/*ident=*/10, /*value=*/1.5f, /*unc=*/0.5f,
                                              shape, slice0, /*face=*/nullptr);
    auto set0 = std::make_shared<SimpleBlobSet>(/*ident=*/100, ISlice::pointer(slice0),
                                                IBlob::vector{blob0});

    // set 1: null slice; one blob (face null).
    auto blob1 = std::make_shared<SimpleBlob>(/*ident=*/11, /*value=*/2.5f, /*unc=*/0.25f,
                                              shape, ISlice::pointer(nullptr), /*face=*/nullptr);
    auto set1 = std::make_shared<SimpleBlobSet>(/*ident=*/200, ISlice::pointer(nullptr),
                                                IBlob::vector{blob1});

    IBlobSet::vector blobsets{IBlobSet::pointer(set0), IBlobSet::pointer(set1)};

    auto res = WireCell::Arrow::to_arrow(blobsets);
    if (!res.ok()) return fail("to_arrow: " + res.status().ToString());
    auto table = *res;
    if (!table->ValidateFull().ok()) return fail("table does not validate");
    if (table->num_rows() != 2) return fail("row count: got " + std::to_string(table->num_rows()));

    // Schema name check.
    {
        auto md = table->schema()->metadata();
        if (!md) return fail("no schema metadata");
        const int i = md->FindKey("arrow.schema");
        if (i < 0 || md->value(i) != "wc.blobs") return fail("schema name != wc.blobs");
    }

    auto col = [&](const std::string& name) { return table->GetColumnByName(name)->chunk(0); };

    auto blobset_index = std::static_pointer_cast<arrow::Int32Array>(col("wc.blobs.blobset_index"));
    auto slice_ident   = std::static_pointer_cast<arrow::Int32Array>(col("wc.blobs.slice_ident"));
    auto slice_start   = std::static_pointer_cast<arrow::DoubleArray>(col("wc.blobs.slice_start"));
    auto blobset_ident = std::static_pointer_cast<arrow::Int32Array>(col("wc.blobs.blobset_ident"));
    auto blob_ident    = std::static_pointer_cast<arrow::Int32Array>(col("wc.blob.ident"));
    auto value         = std::static_pointer_cast<arrow::FloatArray>(col("wc.blob.value"));
    auto uncertainty   = std::static_pointer_cast<arrow::FloatArray>(col("wc.blob.uncertainty"));
    auto face_ident    = std::static_pointer_cast<arrow::Int32Array>(col("wc.blob.face_ident"));

    // Row 0 (from set0).
    if (blobset_index->Value(0) != 0)         return fail("row0 blobset_index");
    if (slice_ident->Value(0) != 5)           return fail("row0 slice_ident");
    if (slice_start->Value(0) != 100.0 * units::us) return fail("row0 slice_start");
    if (blobset_ident->Value(0) != 100)       return fail("row0 blobset_ident");
    if (blob_ident->Value(0) != 10)           return fail("row0 blob ident");
    if (value->Value(0) != 1.5f)              return fail("row0 value");
    if (uncertainty->Value(0) != 0.5f)        return fail("row0 uncertainty");
    if (face_ident->Value(0) != -1)           return fail("row0 face_ident (null face guard)");

    // Row 1 (from set1, null slice sentinels).
    if (blobset_index->Value(1) != 1)         return fail("row1 blobset_index");
    if (slice_ident->Value(1) != -1)          return fail("row1 slice_ident (null slice guard)");
    if (slice_start->Value(1) != 0.0)         return fail("row1 slice_start (null slice guard)");
    if (blobset_ident->Value(1) != 200)       return fail("row1 blobset_ident");
    if (value->Value(1) != 2.5f)              return fail("row1 value");

    // wc.blob.strips : list<struct<layer, lo, hi>>, checked against the shape.
    {
        auto strips = std::static_pointer_cast<arrow::ListArray>(col("wc.blob.strips"));
        auto st = std::static_pointer_cast<arrow::StructArray>(strips->values());
        auto layer = std::static_pointer_cast<arrow::Int32Array>(st->field(0));
        auto lo    = std::static_pointer_cast<arrow::Int32Array>(st->field(1));
        auto hi    = std::static_pointer_cast<arrow::Int32Array>(st->field(2));

        const auto& src = shape.strips();
        const int64_t off = strips->value_offset(0);
        const int64_t len = strips->value_length(0);
        if (len != static_cast<int64_t>(src.size())) return fail("row0 strips length");
        for (int64_t i = 0; i < len; ++i) {
            if (layer->Value(off + i) != src[i].layer)        return fail("strip layer @" + std::to_string(i));
            if (lo->Value(off + i) != src[i].bounds.first)    return fail("strip lo @" + std::to_string(i));
            if (hi->Value(off + i) != src[i].bounds.second)   return fail("strip hi @" + std::to_string(i));
        }
    }

    // wc.blob.corners : list<struct<layer1, ray1, layer2, ray2>>.
    {
        auto corners = std::static_pointer_cast<arrow::ListArray>(col("wc.blob.corners"));
        auto st = std::static_pointer_cast<arrow::StructArray>(corners->values());
        auto l1 = std::static_pointer_cast<arrow::Int32Array>(st->field(0));
        auto r1 = std::static_pointer_cast<arrow::Int32Array>(st->field(1));
        auto l2 = std::static_pointer_cast<arrow::Int32Array>(st->field(2));
        auto r2 = std::static_pointer_cast<arrow::Int32Array>(st->field(3));

        const auto& src = shape.corners();
        const int64_t off = corners->value_offset(0);
        const int64_t len = corners->value_length(0);
        if (len != static_cast<int64_t>(src.size())) return fail("row0 corners length");
        for (int64_t i = 0; i < len; ++i) {
            if (l1->Value(off + i) != src[i].first.layer)  return fail("corner layer1 @" + std::to_string(i));
            if (r1->Value(off + i) != src[i].first.grid)   return fail("corner ray1 @" + std::to_string(i));
            if (l2->Value(off + i) != src[i].second.layer) return fail("corner layer2 @" + std::to_string(i));
            if (r2->Value(off + i) != src[i].second.grid)  return fail("corner ray2 @" + std::to_string(i));
        }
    }

    // Empty vector: 0 rows, still a valid wc.blobs table.
    {
        auto empty = WireCell::Arrow::to_arrow(IBlobSet::vector{});
        if (!empty.ok()) return fail("empty to_arrow: " + empty.status().ToString());
        if (!(*empty)->ValidateFull().ok()) return fail("empty table does not validate");
        if ((*empty)->num_rows() != 0) return fail("empty row count");
        if (!(*empty)->schema()->Equals(*WireCell::Arrow::blobs_schema())) return fail("empty schema mismatch");
    }

    std::cout << "PASS: wc.blobs converter ("
              << shape.strips().size() << " strips, "
              << shape.corners().size() << " corners)\n";
    return 0;
}
