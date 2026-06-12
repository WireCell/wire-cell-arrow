#ifndef WIRE_CELL_ARROW_OPS_H
#define WIRE_CELL_ARROW_OPS_H

// Arrow collection operators (ddm-qof): free functions that decompose and
// reassemble the Arrow container types.  Decomposition is a per-row zero-copy
// RecordBatch::Slice; assembly concatenates the row batches and relabels the
// schema metadata.  These operate on the trace/depo/tensor *table* (the row
// table); frame tag/CMM companion tables are handled separately.

#include <arrow/api.h>

#include <memory>
#include <string>
#include <vector>

namespace WireCell::Arrow {

/// Frame-level scalars needed to relabel a trace table as a wc.frame.
struct FrameMeta {
    int ident{0};
    double time{0.0};
    double tick{0.0};
};

// wc.frame / wc.trace
arrow::Result<std::vector<std::shared_ptr<arrow::RecordBatch>>>
frame_to_traces(const std::shared_ptr<arrow::Table>& frame);

arrow::Result<std::shared_ptr<arrow::Table>>
traces_to_frame(const std::vector<std::shared_ptr<arrow::RecordBatch>>& traces, const FrameMeta& meta);

// wc.deposet / wc.depo
arrow::Result<std::vector<std::shared_ptr<arrow::RecordBatch>>>
deposet_to_depos(const std::shared_ptr<arrow::Table>& deposet);

arrow::Result<std::shared_ptr<arrow::Table>>
depos_to_deposet(const std::vector<std::shared_ptr<arrow::RecordBatch>>& depos, int ident);

// wc.tensorset / wc.tensor
arrow::Result<std::vector<std::shared_ptr<arrow::RecordBatch>>>
tensorset_to_tensors(const std::shared_ptr<arrow::Table>& tensorset);

arrow::Result<std::shared_ptr<arrow::Table>>
tensors_to_tensorset(const std::vector<std::shared_ptr<arrow::RecordBatch>>& tensors,
                     int ident, const std::string& metadata_json = {});

}  // namespace WireCell::Arrow

#endif  // WIRE_CELL_ARROW_OPS_H
