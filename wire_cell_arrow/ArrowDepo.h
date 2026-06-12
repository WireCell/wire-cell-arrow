#ifndef WIRE_CELL_ARROW_ARROWDEPO_H
#define WIRE_CELL_ARROW_ARROWDEPO_H

#include "WireCellIface/IDepo.h"
#include "WireCellUtil/Point.h"

#include <arrow/api.h>

#include <memory>

namespace WireCell::Arrow {

/// An IDepo backed by one row of a wc.depo / wc.deposet RecordBatch.
///
/// The depo's own fields are read from the flat per-depo columns.  prior()
/// walks the nested wc.depo.priors list<struct> chain (per the ddm-yb7
/// decision): prior() returns the struct element at chain index 0, whose
/// prior() returns element 1, etc., until the chain is exhausted (nullptr).
///
/// Scalar accessors read Arrow values directly.  pos() returns a const Point&
/// (which cannot alias Arrow memory), so it is lazily materialised and cached;
/// prior() is likewise cached.  The backing batch is retained for lifetime.
class ArrowDepo : public WireCell::IDepo {
  public:
    /// Wrap row `row` of a wc.depo / wc.deposet batch.  Throws
    /// std::invalid_argument on missing/mistyped columns or out-of-range row.
    ArrowDepo(std::shared_ptr<arrow::RecordBatch> batch, int64_t row = 0);
    virtual ~ArrowDepo();

    virtual const WireCell::Point& pos() const;
    virtual double time() const;
    virtual double charge() const;
    virtual double energy() const;
    virtual int id() const;
    virtual int pdg() const;
    virtual WireCell::IDepo::pointer prior() const;
    virtual double extent_long() const;
    virtual double extent_tran() const;

  private:
    // The 10 per-depo scalar columns, resolved from either the flat batch
    // columns (head) or the priors struct fields (chain elements).
    struct Cols {
        std::shared_ptr<arrow::DoubleArray> time, charge, energy, x, y, z, el, et;
        std::shared_ptr<arrow::Int32Array> id, pdg;
    };

    // Private ctor for a prior-chain element (fields read from the priors
    // struct, indexed by an absolute position in [chain_begin, chain_end)).
    ArrowDepo(std::shared_ptr<arrow::RecordBatch> keepalive, Cols cols, int64_t row,
              std::shared_ptr<arrow::StructArray> chain, Cols chain_cols,
              int64_t chain_begin, int64_t chain_end, int64_t chain_pos);

    std::shared_ptr<arrow::RecordBatch> m_batch;   // keepalive for all buffers
    Cols m_cols;                                   // this depo's own columns
    int64_t m_row;

    std::shared_ptr<arrow::StructArray> m_chain;   // priors values (shared in a chain)
    Cols m_chain_cols;                             // columns over m_chain
    int64_t m_chain_begin{0}, m_chain_end{0}, m_chain_pos{-1};  // pos<0 => head

    mutable WireCell::Point m_pos;
    mutable bool m_pos_loaded{false};
    mutable WireCell::IDepo::pointer m_prior;
    mutable bool m_prior_loaded{false};
};

}  // namespace WireCell::Arrow

#endif  // WIRE_CELL_ARROW_ARROWDEPO_H
