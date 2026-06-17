// Semantic schema versioning tests (ddm-c3s.9).
//
// Asserts: (1) every wc.* schema produced by the converters carries the build's
// kSchemaVersion under kSchemaVersionKey; (2) a versioned product round-trips
// through its facade; (3) a reader DETECTS and REJECTS a deliberately-bumped
// (newer) version; (4) an unversioned (legacy) schema is accepted best-effort.
#include "test_helpers.hpp"

#include "wire_cell_arrow/ArrowDepoSet.hpp"
#include "wire_cell_arrow/ArrowFrame.hpp"
#include "wire_cell_arrow/ArrowTensorSet.hpp"
#include "wire_cell_arrow/ArrowTrace.hpp"
#include "wire_cell_arrow/Converters.hpp"

#include <iostream>
#include <memory>
#include <stdexcept>

using namespace WireCell;
using namespace wcatest;
using WireCell::Arrow::kSchemaVersion;
using WireCell::Arrow::kSchemaVersionKey;
using WireCell::Arrow::schema_version;

static int fails = 0;
static void check(bool ok, const std::string& name)
{
    std::cout << (ok ? "ok   " : "FAIL ") << name << "\n";
    if (!ok) ++fails;
}

// Return a copy of `schema` with its version metadata bumped to v.
static std::shared_ptr<arrow::Schema> bump_version(const std::shared_ptr<arrow::Schema>& schema,
                                                   int v)
{
    auto md = schema->metadata()->Copy();
    auto st = md->Set(kSchemaVersionKey, std::to_string(v));
    if (!st.ok()) throw std::runtime_error("bump_version: " + st.ToString());
    return schema->WithMetadata(md);
}

template <typename Facade, typename Make>
static void expect_reject_newer(const std::string& name,
                                const std::shared_ptr<arrow::Table>& table, Make make)
{
    auto bumped = table->ReplaceSchemaMetadata(
      bump_version(table->schema(), kSchemaVersion + 1)->metadata());
    bool threw = false;
    try {
        make(bumped);
    }
    catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw, name + ": reader rejects newer version");
}

int main()
{
    // (1) Every schema is versioned with the build version.
    check(schema_version(WireCell::Arrow::trace_schema()) == kSchemaVersion, "trace schema versioned");
    check(schema_version(WireCell::Arrow::depo_schema()) == kSchemaVersion, "depo schema versioned");
    check(schema_version(WireCell::Arrow::deposet_schema(1)) == kSchemaVersion, "deposet schema versioned");
    check(schema_version(WireCell::Arrow::tensor_schema()) == kSchemaVersion, "tensor schema versioned");
    check(schema_version(WireCell::Arrow::tensorset_schema(1)) == kSchemaVersion, "tensorset schema versioned");

    // wc.frame version rides in the traces-table schema metadata.
    {
        auto f = make_fake_frame(3, 8);
        auto r = WireCell::Arrow::to_arrow_sparse(f);
        check(r.ok() && schema_version(r->traces->schema()) == kSchemaVersion, "frame schema versioned");
    }

    // (2) Versioned products round-trip through their facades (version OK).
    {
        auto ds = make_fake_deposet(2);
        auto r = WireCell::Arrow::to_arrow(ds);
        bool ok = r.ok();
        try { WireCell::Arrow::ArrowDepoSet probe(*r); }
        catch (...) { ok = false; }
        check(ok, "current-version deposet accepted");
    }

    // (3) Readers detect + reject a deliberately-bumped (newer) version.
    {
        auto ds = make_fake_deposet(2);
        auto t = *WireCell::Arrow::to_arrow(ds);
        expect_reject_newer<WireCell::Arrow::ArrowDepoSet>(
          "deposet", t, [](const std::shared_ptr<arrow::Table>& tb) {
              WireCell::Arrow::ArrowDepoSet x(tb);
          });
    }
    {
        auto ts = make_fake_tensorset(2);
        auto t = *WireCell::Arrow::to_arrow(ts);
        expect_reject_newer<WireCell::Arrow::ArrowTensorSet>(
          "tensorset", t, [](const std::shared_ptr<arrow::Table>& tb) {
              WireCell::Arrow::ArrowTensorSet x(tb);
          });
    }
    {
        // Frame: bump the traces table; ArrowFrame must reject.
        auto f = make_fake_frame(3, 8);
        auto b = *WireCell::Arrow::to_arrow_sparse(f);
        auto bumped = b;
        bumped.traces = b.traces->ReplaceSchemaMetadata(
          bump_version(b.traces->schema(), kSchemaVersion + 1)->metadata());
        bool threw = false;
        try { WireCell::Arrow::ArrowFrame x(bumped); }
        catch (const std::runtime_error&) { threw = true; }
        check(threw, "frame: reader rejects newer version");
    }

    // (4) Unversioned (legacy) schema: schema_version == 0, accepted best-effort.
    {
        auto ds = make_fake_deposet(1);
        auto t = *WireCell::Arrow::to_arrow(ds);
        // Strip all schema metadata -> unversioned.
        auto bare = t->ReplaceSchemaMetadata(nullptr);
        check(schema_version(bare->schema()) == 0, "stripped schema reports version 0");
        bool ok = true;
        try { WireCell::Arrow::ArrowDepoSet x(bare); }
        catch (...) { ok = false; }
        check(ok, "unversioned (legacy) schema accepted best-effort");
    }

    std::cout << (fails ? "SCHEMA VERSION FAILURES\n" : "all schema-version checks OK\n");
    return fails ? 1 : 0;
}
