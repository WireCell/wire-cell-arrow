#include "wire_cell_arrow/Ops.h"
#include "wire_cell_arrow/Converters.h"   // table_to_batch, *_schema, hexfloat

namespace WireCell::Arrow {

namespace {

// Decompose a table into one zero-copy single-row batch per row.
arrow::Result<std::vector<std::shared_ptr<arrow::RecordBatch>>>
rows_to_batches(const std::shared_ptr<arrow::Table>& table)
{
    if (!table) return arrow::Status::Invalid("null table");
    ARROW_ASSIGN_OR_RAISE(auto batch, table_to_batch(table));
    std::vector<std::shared_ptr<arrow::RecordBatch>> out;
    out.reserve(batch->num_rows());
    for (int64_t i = 0; i < batch->num_rows(); ++i) {
        out.push_back(batch->Slice(i, 1));   // zero-copy
    }
    return out;
}

// Assemble row batches into a Table with the given schema (column-wise; avoids
// schema-metadata equality issues in FromRecordBatches).
arrow::Result<std::shared_ptr<arrow::Table>>
batches_to_table(const std::vector<std::shared_ptr<arrow::RecordBatch>>& batches,
                 const std::shared_ptr<arrow::Schema>& schema)
{
    const int ncol = schema->num_fields();
    std::vector<std::shared_ptr<arrow::ChunkedArray>> columns(ncol);
    for (int c = 0; c < ncol; ++c) {
        std::vector<std::shared_ptr<arrow::Array>> chunks;
        for (const auto& b : batches) {
            if (b->num_columns() != ncol) {
                return arrow::Status::Invalid("batches_to_table: column count mismatch");
            }
            chunks.push_back(b->column(c));
        }
        ARROW_ASSIGN_OR_RAISE(columns[c], arrow::ChunkedArray::Make(chunks, schema->field(c)->type()));
    }
    return arrow::Table::Make(schema, columns);
}

// Fields to use for assembly: from the first batch if any, else the fallback.
arrow::FieldVector assembly_fields(const std::vector<std::shared_ptr<arrow::RecordBatch>>& batches,
                                   const std::shared_ptr<arrow::Schema>& fallback)
{
    return batches.empty() ? fallback->fields() : batches.front()->schema()->fields();
}

}  // namespace

arrow::Result<std::vector<std::shared_ptr<arrow::RecordBatch>>>
frame_to_traces(const std::shared_ptr<arrow::Table>& frame) { return rows_to_batches(frame); }

arrow::Result<std::shared_ptr<arrow::Table>>
traces_to_frame(const std::vector<std::shared_ptr<arrow::RecordBatch>>& traces, const FrameMeta& meta)
{
    auto fields = assembly_fields(traces, trace_schema());
    auto md = arrow::key_value_metadata({
        {"arrow.schema",   "wc.frame"},
        {"wc.frame.ident", std::to_string(meta.ident)},
        {"wc.frame.time",  hexfloat(meta.time)},
        {"wc.frame.tick",  hexfloat(meta.tick)},
    });
    return batches_to_table(traces, arrow::schema(fields, md));
}

arrow::Result<std::vector<std::shared_ptr<arrow::RecordBatch>>>
deposet_to_depos(const std::shared_ptr<arrow::Table>& deposet) { return rows_to_batches(deposet); }

arrow::Result<std::shared_ptr<arrow::Table>>
depos_to_deposet(const std::vector<std::shared_ptr<arrow::RecordBatch>>& depos, int ident)
{
    // deposet_schema(ident) carries the depo columns + wc.deposet metadata.
    return batches_to_table(depos, deposet_schema(ident));
}

arrow::Result<std::vector<std::shared_ptr<arrow::RecordBatch>>>
tensorset_to_tensors(const std::shared_ptr<arrow::Table>& tensorset) { return rows_to_batches(tensorset); }

arrow::Result<std::shared_ptr<arrow::Table>>
tensors_to_tensorset(const std::vector<std::shared_ptr<arrow::RecordBatch>>& tensors,
                     int ident, const std::string& metadata_json)
{
    return batches_to_table(tensors, tensorset_schema(ident, metadata_json));
}

}  // namespace WireCell::Arrow
