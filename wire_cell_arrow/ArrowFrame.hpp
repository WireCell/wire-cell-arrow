#ifndef WIRE_CELL_ARROW_ARROWFRAME_H
#define WIRE_CELL_ARROW_ARROWFRAME_H

#include "wire_cell_arrow/Converters.hpp"   // FrameTables

#include "WireCellIface/IFrame.h"
#include "WireCellUtil/Waveform.h"

#include <arrow/api.h>

#include <map>
#include <memory>
#include <string>

namespace WireCell::Arrow {

/// An IFrame backed by a wc.frame bundle (FrameTables: traces table + the
/// frame_tags / trace_tags / cmm companion tables).  The traces table may be
/// sparse (wc.frame) or dense (wc.frame.dense) — traces() wraps each row in an
/// ArrowTrace, which handles both.
///
/// ident/time/tick are read from the traces-table schema metadata (time/tick
/// via hexfloat).  The tagging system and CMM are reconstructed from the
/// companion tables in the constructor; traces() is built lazily and cached.
///
/// (The issue's single-Table constructor predates the ddm-li8 bundle decision;
/// the companion tables are required to reconstruct tags/CMM.  Companion tables
/// may be null and are then treated as empty.)
class ArrowFrame : public WireCell::IFrame {
  public:
    explicit ArrowFrame(FrameTables tables);
    virtual ~ArrowFrame();

    virtual const tag_list_t& frame_tags() const;
    virtual const tag_list_t& trace_tags() const;
    virtual const trace_list_t& tagged_traces(const tag_t& tag) const;
    virtual const trace_summary_t& trace_summary(const tag_t& tag) const;
    virtual ITrace::shared_vector traces() const;
    virtual Waveform::ChannelMaskMap masks() const;
    virtual int ident() const;
    virtual double time() const;
    virtual double tick() const;

  private:
    FrameTables m_tables;                               // keepalive
    std::shared_ptr<arrow::RecordBatch> m_trace_batch;  // traces table as one batch

    int m_ident{0};
    double m_time{0.0}, m_tick{0.0};

    tag_list_t m_frame_tags, m_trace_tags;
    std::map<tag_t, trace_list_t> m_tagged;
    std::map<tag_t, trace_summary_t> m_summary;
    Waveform::ChannelMaskMap m_masks;

    const trace_list_t m_empty_tl;
    const trace_summary_t m_empty_ts;

    mutable ITrace::shared_vector m_traces;
    mutable bool m_traces_loaded{false};
};

}  // namespace WireCell::Arrow

#endif  // WIRE_CELL_ARROW_ARROWFRAME_H
