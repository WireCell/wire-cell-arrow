#ifndef WIRE_CELL_ARROW_ARROWTRACKSEGMENT_H
#define WIRE_CELL_ARROW_ARROWTRACKSEGMENT_H

#include "WireCellIface/ITrackSegment.h"

#include <arrow/api.h>

#include <memory>

namespace WireCell::Arrow {

/// An ITrackSegment backed by one row of a wc.tracksegment(set) RecordBatch.
/// Retains the batch (keeps the Arrow buffers alive); scalar accessors read
/// the typed arrays directly.  The start()/stop() Points are materialized at
/// construction (they are returned by const reference).
class ArrowTrackSegment : public WireCell::ITrackSegment {
  public:
    /// Wrap one row.  The batch schema must name wc.tracksegment or
    /// wc.tracksegmentset (they share the column set).
    ArrowTrackSegment(std::shared_ptr<arrow::RecordBatch> batch, int64_t row);
    virtual ~ArrowTrackSegment();

    virtual const WireCell::Point& start() const;
    virtual const WireCell::Point& stop() const;
    virtual double start_time() const;
    virtual double stop_time() const;
    virtual double energy() const;
    virtual double secondary() const;
    virtual double n_electrons() const;
    virtual double track_length() const;
    virtual int id() const;
    virtual int pdg() const;

  private:
    std::shared_ptr<arrow::RecordBatch> m_batch;
    int64_t m_row;
    WireCell::Point m_start, m_stop;
    std::shared_ptr<arrow::DoubleArray> m_start_t, m_stop_t;
    std::shared_ptr<arrow::DoubleArray> m_energy, m_secondary, m_n_electrons, m_track_length;
    std::shared_ptr<arrow::Int32Array> m_id, m_pdg;
};

}  // namespace WireCell::Arrow

#endif
