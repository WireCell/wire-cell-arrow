#include "wire_cell_arrow/Converters.h"

namespace WireCell::Arrow {

std::shared_ptr<arrow::Schema> trace_schema()
{
    auto md = arrow::key_value_metadata({{"arrow.schema", "wc.trace"}});
    return arrow::schema(
        {
            arrow::field("wc.trace.channel", arrow::int32(), /*nullable=*/false),
            arrow::field("wc.trace.tbin",    arrow::int32(), /*nullable=*/false),
            // arrow::list(float32()) yields list<item: float32 (nullable)>,
            // matching the ListBuilder(FloatBuilder) output type below.
            arrow::field("wc.trace.charge",  arrow::list(arrow::float32()), /*nullable=*/false),
        },
        md);
}

arrow::Result<std::shared_ptr<arrow::RecordBatch>>
to_arrow(const WireCell::ITrace::pointer& trace)
{
    auto* pool = arrow::default_memory_pool();

    arrow::Int32Builder channel_b(pool);
    arrow::Int32Builder tbin_b(pool);
    auto charge_value_b = std::make_shared<arrow::FloatBuilder>(pool);
    arrow::ListBuilder charge_b(pool, charge_value_b);

    ARROW_RETURN_NOT_OK(channel_b.Append(trace->channel()));
    ARROW_RETURN_NOT_OK(tbin_b.Append(trace->tbin()));

    // Open one list slot (this row), then append the contiguous charge buffer.
    ARROW_RETURN_NOT_OK(charge_b.Append());
    const auto& charge = trace->charge();   // const std::vector<float>&
    if (!charge.empty()) {
        ARROW_RETURN_NOT_OK(
            charge_value_b->AppendValues(charge.data(),
                                         static_cast<int64_t>(charge.size())));
    }

    std::shared_ptr<arrow::Array> channel_arr, tbin_arr, charge_arr;
    ARROW_RETURN_NOT_OK(channel_b.Finish(&channel_arr));
    ARROW_RETURN_NOT_OK(tbin_b.Finish(&tbin_arr));
    ARROW_RETURN_NOT_OK(charge_b.Finish(&charge_arr));

    return arrow::RecordBatch::Make(trace_schema(), /*num_rows=*/1,
                                    {channel_arr, tbin_arr, charge_arr});
}

}  // namespace WireCell::Arrow
