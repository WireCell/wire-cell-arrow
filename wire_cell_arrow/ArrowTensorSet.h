#ifndef WIRE_CELL_ARROW_ARROWTENSORSET_H
#define WIRE_CELL_ARROW_ARROWTENSORSET_H

#include "WireCellIface/ITensorSet.h"

#include <arrow/api.h>

#include <memory>

namespace WireCell::Arrow {

/// An ITensorSet backed by a wc.tensorset Table.  ident()/metadata() read the
/// wc.tensorset.ident / wc.tensorset.metadata schema metadata; tensors()
/// lazily builds one ArrowTensor per row (cached).
class ArrowTensorSet : public WireCell::ITensorSet {
  public:
    explicit ArrowTensorSet(std::shared_ptr<arrow::Table> table);
    virtual ~ArrowTensorSet();

    virtual int ident() const;
    virtual WireCell::Configuration metadata() const;
    virtual WireCell::ITensor::shared_vector tensors() const;

  private:
    std::shared_ptr<arrow::RecordBatch> m_batch;
    int m_ident{0};
    WireCell::Configuration m_md;
    mutable WireCell::ITensor::shared_vector m_tensors;
    mutable bool m_loaded{false};
};

}  // namespace WireCell::Arrow

#endif
