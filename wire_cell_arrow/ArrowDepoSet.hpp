#ifndef WIRE_CELL_ARROW_ARROWDEPOSET_H
#define WIRE_CELL_ARROW_ARROWDEPOSET_H

#include "WireCellIface/IDepoSet.h"

#include <arrow/api.h>

#include <memory>

namespace WireCell::Arrow {

/// An IDepoSet backed by a wc.deposet Table.  ident() reads the
/// wc.deposet.ident schema metadata; depos() lazily builds one ArrowDepo per
/// row (cached).
class ArrowDepoSet : public WireCell::IDepoSet {
  public:
    explicit ArrowDepoSet(std::shared_ptr<arrow::Table> table);
    virtual ~ArrowDepoSet();

    virtual int ident() const;
    virtual WireCell::IDepo::shared_vector depos() const;

  private:
    std::shared_ptr<arrow::RecordBatch> m_batch;
    int m_ident{0};
    mutable WireCell::IDepo::shared_vector m_depos;
    mutable bool m_loaded{false};
};

}  // namespace WireCell::Arrow

#endif
