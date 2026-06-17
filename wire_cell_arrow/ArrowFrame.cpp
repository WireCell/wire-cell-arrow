#include "wire_cell_arrow/ArrowFrame.h"
#include "wire_cell_arrow/ArrowTrace.h"

#include <stdexcept>
#include <string>

namespace WireCell::Arrow {

namespace {

std::string meta_get(const std::shared_ptr<arrow::RecordBatch>& b, const std::string& key)
{
    auto md = b->schema()->metadata();
    if (!md) return {};
    auto got = md->Get(key);
    return got.ok() ? *got : std::string{};
}

}  // namespace

ArrowFrame::ArrowFrame(FrameTables tables)
  : m_tables(std::move(tables))
{
    if (!m_tables.traces) {
        throw std::invalid_argument("ArrowFrame: null traces table");
    }
    auto br = table_to_batch(m_tables.traces);
    if (!br.ok()) throw std::runtime_error("ArrowFrame: " + br.status().ToString());
    m_trace_batch = *br;
    require_readable_schema(m_trace_batch->schema(), "wc.frame");

    // Frame scalars from schema metadata.
    const std::string sid = meta_get(m_trace_batch, "wc.frame.ident");
    if (!sid.empty()) m_ident = std::stoi(sid);
    m_time = parse_hexfloat(meta_get(m_trace_batch, "wc.frame.time"));
    m_tick = parse_hexfloat(meta_get(m_trace_batch, "wc.frame.tick"));

    // frame_tags
    if (m_tables.frame_tags) {
        auto col = m_tables.frame_tags->GetColumnByName("wc.frame.frame_tags.tag");
        if (col) {
            auto arr = std::static_pointer_cast<arrow::StringArray>(col->chunk(0));
            for (int64_t i = 0; i < arr->length(); ++i) m_frame_tags.push_back(arr->GetString(i));
        }
    }

    // trace_tags + tagged_traces + trace_summary
    if (m_tables.trace_tags) {
        auto tag_col = m_tables.trace_tags->GetColumnByName("wc.frame.trace_tags.tag");
        auto ti_col  = m_tables.trace_tags->GetColumnByName("wc.frame.trace_tags.trace_index");
        auto sm_col  = m_tables.trace_tags->GetColumnByName("wc.frame.trace_tags.summary");
        if (tag_col && ti_col && sm_col) {
            auto tags = std::static_pointer_cast<arrow::StringArray>(tag_col->chunk(0));
            auto ti   = std::static_pointer_cast<arrow::ListArray>(ti_col->chunk(0));
            auto tiv  = std::static_pointer_cast<arrow::Int64Array>(ti->values());
            auto sm   = std::static_pointer_cast<arrow::ListArray>(sm_col->chunk(0));
            auto smv  = std::static_pointer_cast<arrow::DoubleArray>(sm->values());
            for (int64_t r = 0; r < tags->length(); ++r) {
                const std::string tag = tags->GetString(r);
                m_trace_tags.push_back(tag);

                trace_list_t indices;
                const int64_t to = ti->value_offset(r), tl = ti->value_length(r);
                for (int64_t k = 0; k < tl; ++k) {
                    indices.push_back(static_cast<size_t>(tiv->Value(to + k)));
                }
                m_tagged[tag] = std::move(indices);

                if (!sm->IsNull(r)) {
                    trace_summary_t summ;
                    const int64_t so = sm->value_offset(r), sl = sm->value_length(r);
                    for (int64_t k = 0; k < sl; ++k) summ.push_back(smv->Value(so + k));
                    m_summary[tag] = std::move(summ);
                }
            }
        }
    }

    // CMM
    if (m_tables.cmm) {
        auto lab = m_tables.cmm->GetColumnByName("wc.frame.cmm.label");
        auto chn = m_tables.cmm->GetColumnByName("wc.frame.cmm.channel");
        auto lo  = m_tables.cmm->GetColumnByName("wc.frame.cmm.bin_start");
        auto hi  = m_tables.cmm->GetColumnByName("wc.frame.cmm.bin_end");
        if (lab && chn && lo && hi) {
            auto a_lab = std::static_pointer_cast<arrow::StringArray>(lab->chunk(0));
            auto a_chn = std::static_pointer_cast<arrow::Int32Array>(chn->chunk(0));
            auto a_lo  = std::static_pointer_cast<arrow::Int32Array>(lo->chunk(0));
            auto a_hi  = std::static_pointer_cast<arrow::Int32Array>(hi->chunk(0));
            for (int64_t r = 0; r < a_lab->length(); ++r) {
                m_masks[a_lab->GetString(r)][a_chn->Value(r)].push_back(
                    {a_lo->Value(r), a_hi->Value(r)});
            }
        }
    }
}

ArrowFrame::~ArrowFrame() {}

const IFrame::tag_list_t& ArrowFrame::frame_tags() const { return m_frame_tags; }
const IFrame::tag_list_t& ArrowFrame::trace_tags() const { return m_trace_tags; }

const IFrame::trace_list_t& ArrowFrame::tagged_traces(const tag_t& tag) const
{
    auto it = m_tagged.find(tag);
    return it == m_tagged.end() ? m_empty_tl : it->second;
}

const IFrame::trace_summary_t& ArrowFrame::trace_summary(const tag_t& tag) const
{
    auto it = m_summary.find(tag);
    return it == m_summary.end() ? m_empty_ts : it->second;
}

ITrace::shared_vector ArrowFrame::traces() const
{
    if (!m_traces_loaded) {
        auto vec = std::make_shared<ITrace::vector>();
        const int64_t n = m_trace_batch->num_rows();
        vec->reserve(n);
        for (int64_t r = 0; r < n; ++r) {
            vec->push_back(std::make_shared<ArrowTrace>(m_trace_batch, r));
        }
        m_traces = vec;
        m_traces_loaded = true;
    }
    return m_traces;
}

Waveform::ChannelMaskMap ArrowFrame::masks() const { return m_masks; }
int ArrowFrame::ident() const { return m_ident; }
double ArrowFrame::time() const { return m_time; }
double ArrowFrame::tick() const { return m_tick; }

}  // namespace WireCell::Arrow
