#ifndef WIRE_CELL_ARROW_ARROWTRACKSEGMENTSET_H
#define WIRE_CELL_ARROW_ARROWTRACKSEGMENTSET_H

#include "WireCellIface/ITrackSegmentSet.h"

#include <arrow/api.h>

#include <memory>

namespace WireCell::Arrow {

/// An ITrackSegmentSet backed by a wc.tracksegmentset Table.  ident() reads
/// the wc.tracksegmentset.ident schema metadata; segments() lazily builds one
/// ArrowTrackSegment per row (cached).
class ArrowTrackSegmentSet : public WireCell::ITrackSegmentSet {
  public:
    explicit ArrowTrackSegmentSet(std::shared_ptr<arrow::Table> table);
    virtual ~ArrowTrackSegmentSet();

    virtual int ident() const;
    virtual WireCell::ITrackSegment::shared_vector segments() const;

  private:
    std::shared_ptr<arrow::RecordBatch> m_batch;
    int m_ident{0};
    mutable WireCell::ITrackSegment::shared_vector m_segments;
    mutable bool m_loaded{false};
};

}  // namespace WireCell::Arrow

#endif
