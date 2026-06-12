// Unit test for to_arrow(ITrace) — the wc.trace converter (ddm-85v).
//
// Builds Aux::SimpleTrace inputs, converts to Arrow RecordBatches, and checks
// the wc.trace schema (field names/types, arrow.schema metadata) and that
// channel/tbin/charge round-trip — including the empty-charge edge case.

#include "wire_cell_arrow/Converters.h"

#include "WireCellAux/SimpleTrace.h"

#include <arrow/api.h>

#include <iostream>
#include <memory>
#include <vector>

using WireCell::ITrace;
using WireCell::Aux::SimpleTrace;

static int fail(const std::string& msg)
{
    std::cerr << "FAIL: " << msg << "\n";
    return 1;
}

// Validate the common wc.trace schema/shape of a one-row batch.
static int check_batch_shape(const std::shared_ptr<arrow::RecordBatch>& batch)
{
    if (!batch) return fail("null batch");
    if (batch->num_rows() != 1) return fail("expected 1 row");
    if (batch->num_columns() != 3) return fail("expected 3 columns");

    auto schema = batch->schema();
    if (schema->field(0)->name() != "wc.trace.channel") return fail("col0 name");
    if (schema->field(1)->name() != "wc.trace.tbin")    return fail("col1 name");
    if (schema->field(2)->name() != "wc.trace.charge")  return fail("col2 name");

    if (schema->field(0)->type()->id() != arrow::Type::INT32) return fail("channel type");
    if (schema->field(1)->type()->id() != arrow::Type::INT32) return fail("tbin type");
    if (schema->field(2)->type()->id() != arrow::Type::LIST)  return fail("charge type");

    auto md = schema->metadata();
    if (!md) return fail("missing schema metadata");
    auto got = md->Get("arrow.schema");
    if (!got.ok() || *got != "wc.trace") return fail("arrow.schema metadata != wc.trace");
    return 0;
}

int main()
{
    // --- Case 1: a normal trace. ---
    {
        std::vector<float> q{1.5f, -2.0f, 3.25f, 0.0f, 4.0f};
        ITrace::pointer trace = std::make_shared<SimpleTrace>(42, 10, q);

        auto result = WireCell::Arrow::to_arrow(trace);
        if (!result.ok()) return fail("to_arrow failed: " + result.status().ToString());
        auto batch = *result;

        if (int rc = check_batch_shape(batch)) return rc;

        auto channel = std::static_pointer_cast<arrow::Int32Array>(batch->column(0));
        auto tbin    = std::static_pointer_cast<arrow::Int32Array>(batch->column(1));
        auto charge  = std::static_pointer_cast<arrow::ListArray>(batch->column(2));

        if (channel->Value(0) != 42) return fail("channel value");
        if (tbin->Value(0) != 10)    return fail("tbin value");
        if (charge->value_length(0) != static_cast<int64_t>(q.size()))
            return fail("charge length");

        auto vals = std::static_pointer_cast<arrow::FloatArray>(charge->values());
        for (size_t i = 0; i < q.size(); ++i) {
            if (vals->Value(charge->value_offset(0) + i) != q[i])
                return fail("charge value mismatch at " + std::to_string(i));
        }
    }

    // --- Case 2: empty charge -> zero-length (non-null) list. ---
    {
        ITrace::pointer trace = std::make_shared<SimpleTrace>(7, 0, std::vector<float>{});
        auto result = WireCell::Arrow::to_arrow(trace);
        if (!result.ok()) return fail("to_arrow (empty) failed: " + result.status().ToString());
        auto batch = *result;
        if (int rc = check_batch_shape(batch)) return rc;

        auto charge = std::static_pointer_cast<arrow::ListArray>(batch->column(2));
        if (charge->IsNull(0))            return fail("empty charge should be non-null list");
        if (charge->value_length(0) != 0) return fail("empty charge length should be 0");
    }

    std::cout << "trace converter OK\n";
    return 0;
}
