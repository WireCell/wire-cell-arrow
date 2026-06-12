#include "wire_cell_arrow/Converters.h"

#include "WireCellUtil/Persist.h"

#include <charconv>

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

// ---------------------------------------------------------------------------
// wc.depo
// ---------------------------------------------------------------------------

std::shared_ptr<arrow::DataType> depo_struct_type()
{
    return arrow::struct_({
        arrow::field("time",        arrow::float64(), /*nullable=*/false),
        arrow::field("charge",      arrow::float64(), false),
        arrow::field("energy",      arrow::float64(), false),
        arrow::field("x",           arrow::float64(), false),
        arrow::field("y",           arrow::float64(), false),
        arrow::field("z",           arrow::float64(), false),
        arrow::field("extent_long", arrow::float64(), false),
        arrow::field("extent_tran", arrow::float64(), false),
        arrow::field("id",          arrow::int32(),   false),
        arrow::field("pdg",         arrow::int32(),   false),
    });
}

std::shared_ptr<arrow::Schema> depo_schema()
{
    auto md = arrow::key_value_metadata({{"arrow.schema", "wc.depo"}});
    return arrow::schema(
        {
            arrow::field("wc.depo.time",        arrow::float64(), /*nullable=*/false),
            arrow::field("wc.depo.charge",      arrow::float64(), false),
            arrow::field("wc.depo.energy",      arrow::float64(), false),
            arrow::field("wc.depo.x",           arrow::float64(), false),
            arrow::field("wc.depo.y",           arrow::float64(), false),
            arrow::field("wc.depo.z",           arrow::float64(), false),
            arrow::field("wc.depo.extent_long", arrow::float64(), false),
            arrow::field("wc.depo.extent_tran", arrow::float64(), false),
            arrow::field("wc.depo.id",          arrow::int32(),   false),
            arrow::field("wc.depo.pdg",         arrow::int32(),   false),
            // arrow::list(struct) yields list<item: struct (nullable)>, matching
            // the ListBuilder(StructBuilder) output type built below.
            arrow::field("wc.depo.priors",      arrow::list(depo_struct_type()), false),
        },
        md);
}

namespace {
// Append one depo's 10 scalar fields to the given column builders (used both
// for the flat top-level columns and for the priors struct fields).
arrow::Status append_depo_fields(const WireCell::IDepo::pointer& d,
                                 arrow::DoubleBuilder* t, arrow::DoubleBuilder* q,
                                 arrow::DoubleBuilder* e, arrow::DoubleBuilder* x,
                                 arrow::DoubleBuilder* y, arrow::DoubleBuilder* z,
                                 arrow::DoubleBuilder* el, arrow::DoubleBuilder* et,
                                 arrow::Int32Builder* id, arrow::Int32Builder* pdg)
{
    const auto& p = d->pos();
    ARROW_RETURN_NOT_OK(t->Append(d->time()));
    ARROW_RETURN_NOT_OK(q->Append(d->charge()));
    ARROW_RETURN_NOT_OK(e->Append(d->energy()));
    ARROW_RETURN_NOT_OK(x->Append(p.x()));
    ARROW_RETURN_NOT_OK(y->Append(p.y()));
    ARROW_RETURN_NOT_OK(z->Append(p.z()));
    ARROW_RETURN_NOT_OK(el->Append(d->extent_long()));
    ARROW_RETURN_NOT_OK(et->Append(d->extent_tran()));
    ARROW_RETURN_NOT_OK(id->Append(d->id()));
    ARROW_RETURN_NOT_OK(pdg->Append(d->pdg()));
    return arrow::Status::OK();
}
}  // namespace

arrow::Result<std::shared_ptr<arrow::RecordBatch>>
to_arrow(const WireCell::IDepo::pointer& depo)
{
    auto* pool = arrow::default_memory_pool();

    // Flat top-level column builders (this depo's own fields).
    arrow::DoubleBuilder time_b(pool), charge_b(pool), energy_b(pool),
        x_b(pool), y_b(pool), z_b(pool), el_b(pool), et_b(pool);
    arrow::Int32Builder id_b(pool), pdg_b(pool);

    // priors: list<struct<...>>.  Build the struct field builders in schema
    // order, then a StructBuilder, then wrap in a ListBuilder.
    std::vector<std::shared_ptr<arrow::ArrayBuilder>> sfields = {
        std::make_shared<arrow::DoubleBuilder>(pool),  // time
        std::make_shared<arrow::DoubleBuilder>(pool),  // charge
        std::make_shared<arrow::DoubleBuilder>(pool),  // energy
        std::make_shared<arrow::DoubleBuilder>(pool),  // x
        std::make_shared<arrow::DoubleBuilder>(pool),  // y
        std::make_shared<arrow::DoubleBuilder>(pool),  // z
        std::make_shared<arrow::DoubleBuilder>(pool),  // extent_long
        std::make_shared<arrow::DoubleBuilder>(pool),  // extent_tran
        std::make_shared<arrow::Int32Builder>(pool),   // id
        std::make_shared<arrow::Int32Builder>(pool),   // pdg
    };
    auto struct_b = std::make_shared<arrow::StructBuilder>(depo_struct_type(), pool, sfields);
    arrow::ListBuilder priors_b(pool, struct_b);

    auto* s_t  = static_cast<arrow::DoubleBuilder*>(sfields[0].get());
    auto* s_q  = static_cast<arrow::DoubleBuilder*>(sfields[1].get());
    auto* s_e  = static_cast<arrow::DoubleBuilder*>(sfields[2].get());
    auto* s_x  = static_cast<arrow::DoubleBuilder*>(sfields[3].get());
    auto* s_y  = static_cast<arrow::DoubleBuilder*>(sfields[4].get());
    auto* s_z  = static_cast<arrow::DoubleBuilder*>(sfields[5].get());
    auto* s_el = static_cast<arrow::DoubleBuilder*>(sfields[6].get());
    auto* s_et = static_cast<arrow::DoubleBuilder*>(sfields[7].get());
    auto* s_id = static_cast<arrow::Int32Builder*>(sfields[8].get());
    auto* s_pg = static_cast<arrow::Int32Builder*>(sfields[9].get());

    // This depo's own fields -> flat columns.
    ARROW_RETURN_NOT_OK(append_depo_fields(depo, &time_b, &charge_b, &energy_b,
                                           &x_b, &y_b, &z_b, &el_b, &et_b, &id_b, &pdg_b));

    // prior() chain -> one list element (this row), structs most-recent-first.
    ARROW_RETURN_NOT_OK(priors_b.Append());
    for (auto p = depo->prior(); p; p = p->prior()) {
        ARROW_RETURN_NOT_OK(struct_b->Append());
        ARROW_RETURN_NOT_OK(append_depo_fields(p, s_t, s_q, s_e, s_x, s_y, s_z,
                                               s_el, s_et, s_id, s_pg));
    }

    std::shared_ptr<arrow::Array> a_time, a_charge, a_energy, a_x, a_y, a_z,
        a_el, a_et, a_id, a_pdg, a_priors;
    ARROW_RETURN_NOT_OK(time_b.Finish(&a_time));
    ARROW_RETURN_NOT_OK(charge_b.Finish(&a_charge));
    ARROW_RETURN_NOT_OK(energy_b.Finish(&a_energy));
    ARROW_RETURN_NOT_OK(x_b.Finish(&a_x));
    ARROW_RETURN_NOT_OK(y_b.Finish(&a_y));
    ARROW_RETURN_NOT_OK(z_b.Finish(&a_z));
    ARROW_RETURN_NOT_OK(el_b.Finish(&a_el));
    ARROW_RETURN_NOT_OK(et_b.Finish(&a_et));
    ARROW_RETURN_NOT_OK(id_b.Finish(&a_id));
    ARROW_RETURN_NOT_OK(pdg_b.Finish(&a_pdg));
    ARROW_RETURN_NOT_OK(priors_b.Finish(&a_priors));

    return arrow::RecordBatch::Make(depo_schema(), /*num_rows=*/1,
        {a_time, a_charge, a_energy, a_x, a_y, a_z, a_el, a_et, a_id, a_pdg, a_priors});
}

// ---------------------------------------------------------------------------
// wc.tensor
// ---------------------------------------------------------------------------

std::shared_ptr<arrow::Schema> tensor_schema()
{
    auto md = arrow::key_value_metadata({{"arrow.schema", "wc.tensor"}});
    return arrow::schema(
        {
            arrow::field("wc.tensor.data",     arrow::large_binary(),       /*nullable=*/false),
            arrow::field("wc.tensor.dtype",    arrow::utf8(),               false),
            arrow::field("wc.tensor.shape",    arrow::list(arrow::int64()), false),
            arrow::field("wc.tensor.order",    arrow::list(arrow::int64()), false),
            arrow::field("wc.tensor.metadata", arrow::utf8(),               /*nullable=*/true),
        },
        md);
}

arrow::Result<std::shared_ptr<arrow::RecordBatch>>
to_arrow(const WireCell::ITensor::pointer& tensor)
{
    auto* pool = arrow::default_memory_pool();

    arrow::LargeBinaryBuilder data_b(pool);
    arrow::StringBuilder dtype_b(pool);
    auto shape_vb = std::make_shared<arrow::Int64Builder>(pool);
    arrow::ListBuilder shape_b(pool, shape_vb);
    auto order_vb = std::make_shared<arrow::Int64Builder>(pool);
    arrow::ListBuilder order_b(pool, order_vb);
    arrow::StringBuilder meta_b(pool);

    // data: raw bytes copied verbatim.
    const auto nbytes = static_cast<int64_t>(tensor->size());
    if (nbytes > 0) {
        ARROW_RETURN_NOT_OK(data_b.Append(reinterpret_cast<const uint8_t*>(tensor->data()), nbytes));
    } else {
        ARROW_RETURN_NOT_OK(data_b.AppendEmptyValue());  // non-null, zero-length
    }

    ARROW_RETURN_NOT_OK(dtype_b.Append(tensor->dtype()));

    ARROW_RETURN_NOT_OK(shape_b.Append());
    for (auto s : tensor->shape()) {
        ARROW_RETURN_NOT_OK(shape_vb->Append(static_cast<int64_t>(s)));
    }

    ARROW_RETURN_NOT_OK(order_b.Append());
    for (auto o : tensor->order()) {     // empty => C order
        ARROW_RETURN_NOT_OK(order_vb->Append(static_cast<int64_t>(o)));
    }

    auto cfg = tensor->metadata();
    if (cfg.isNull()) {
        ARROW_RETURN_NOT_OK(meta_b.AppendNull());
    } else {
        ARROW_RETURN_NOT_OK(meta_b.Append(WireCell::Persist::dumps(cfg)));
    }

    std::shared_ptr<arrow::Array> a_data, a_dtype, a_shape, a_order, a_meta;
    ARROW_RETURN_NOT_OK(data_b.Finish(&a_data));
    ARROW_RETURN_NOT_OK(dtype_b.Finish(&a_dtype));
    ARROW_RETURN_NOT_OK(shape_b.Finish(&a_shape));
    ARROW_RETURN_NOT_OK(order_b.Finish(&a_order));
    ARROW_RETURN_NOT_OK(meta_b.Finish(&a_meta));

    return arrow::RecordBatch::Make(tensor_schema(), /*num_rows=*/1,
                                    {a_data, a_dtype, a_shape, a_order, a_meta});
}

// ---------------------------------------------------------------------------
// wc.frame
// ---------------------------------------------------------------------------

std::string hexfloat(double v)
{
    char buf[64];
    auto [p, ec] = std::to_chars(buf, buf + sizeof(buf), v, std::chars_format::hex);
    return std::string(buf, p);
}

double parse_hexfloat(const std::string& s)
{
    double v = 0.0;
    std::from_chars(s.data(), s.data() + s.size(), v, std::chars_format::hex);
    return v;
}

namespace {

// Frame-scalar schema metadata: arrow.schema + ident/time/tick (time/tick as
// bit-exact hexfloat).
std::shared_ptr<arrow::KeyValueMetadata> frame_metadata(const std::string& schema_name,
                                                        const WireCell::IFrame::pointer& f)
{
    return arrow::key_value_metadata({
        {"arrow.schema",   schema_name},
        {"wc.frame.ident", std::to_string(f->ident())},
        {"wc.frame.time",  hexfloat(f->time())},
        {"wc.frame.tick",  hexfloat(f->tick())},
    });
}

// wc.frame.frame_tags : one row per frame tag.
arrow::Result<std::shared_ptr<arrow::Table>> build_frame_tags(const WireCell::IFrame::pointer& f)
{
    arrow::StringBuilder tag_b;
    for (const auto& t : f->frame_tags()) {
        ARROW_RETURN_NOT_OK(tag_b.Append(t));
    }
    std::shared_ptr<arrow::Array> a;
    ARROW_RETURN_NOT_OK(tag_b.Finish(&a));
    auto schema = arrow::schema(
        {arrow::field("wc.frame.frame_tags.tag", arrow::utf8(), false)},
        arrow::key_value_metadata({{"arrow.schema", "wc.frame.frame_tags"}}));
    return arrow::Table::Make(schema, {a});
}

// wc.frame.trace_tags : one row per trace tag; trace_index list<int64>,
// summary list<float64> (null when the tag has no summary).
arrow::Result<std::shared_ptr<arrow::Table>> build_trace_tags(const WireCell::IFrame::pointer& f)
{
    auto* pool = arrow::default_memory_pool();
    arrow::StringBuilder tag_b(pool);
    auto ti_vb = std::make_shared<arrow::Int64Builder>(pool);
    arrow::ListBuilder ti_b(pool, ti_vb);
    auto sm_vb = std::make_shared<arrow::DoubleBuilder>(pool);
    arrow::ListBuilder sm_b(pool, sm_vb);

    for (const auto& tag : f->trace_tags()) {
        ARROW_RETURN_NOT_OK(tag_b.Append(tag));

        ARROW_RETURN_NOT_OK(ti_b.Append());
        for (auto idx : f->tagged_traces(tag)) {
            ARROW_RETURN_NOT_OK(ti_vb->Append(static_cast<int64_t>(idx)));
        }

        const auto& summary = f->trace_summary(tag);
        if (summary.empty()) {
            ARROW_RETURN_NOT_OK(sm_b.AppendNull());
        } else {
            ARROW_RETURN_NOT_OK(sm_b.Append());
            ARROW_RETURN_NOT_OK(sm_vb->AppendValues(summary.data(),
                                                    static_cast<int64_t>(summary.size())));
        }
    }

    std::shared_ptr<arrow::Array> a_tag, a_ti, a_sm;
    ARROW_RETURN_NOT_OK(tag_b.Finish(&a_tag));
    ARROW_RETURN_NOT_OK(ti_b.Finish(&a_ti));
    ARROW_RETURN_NOT_OK(sm_b.Finish(&a_sm));
    auto schema = arrow::schema(
        {
            arrow::field("wc.frame.trace_tags.tag",         arrow::utf8(),                 false),
            arrow::field("wc.frame.trace_tags.trace_index", arrow::list(arrow::int64()),   false),
            arrow::field("wc.frame.trace_tags.summary",     arrow::list(arrow::float64()), /*nullable=*/true),
        },
        arrow::key_value_metadata({{"arrow.schema", "wc.frame.trace_tags"}}));
    return arrow::Table::Make(schema, {a_tag, a_ti, a_sm});
}

// wc.frame.cmm : flattened ChannelMaskMap, one row per (label, channel, range);
// half-open [bin_start, bin_end).  0 rows when the CMM is empty.
arrow::Result<std::shared_ptr<arrow::Table>> build_cmm(const WireCell::IFrame::pointer& f)
{
    arrow::StringBuilder label_b;
    arrow::Int32Builder chan_b, lo_b, hi_b;

    for (const auto& [label, chmasks] : f->masks()) {
        for (const auto& [chan, ranges] : chmasks) {
            for (const auto& range : ranges) {
                ARROW_RETURN_NOT_OK(label_b.Append(label));
                ARROW_RETURN_NOT_OK(chan_b.Append(chan));
                ARROW_RETURN_NOT_OK(lo_b.Append(range.first));
                ARROW_RETURN_NOT_OK(hi_b.Append(range.second));
            }
        }
    }

    std::shared_ptr<arrow::Array> a_label, a_chan, a_lo, a_hi;
    ARROW_RETURN_NOT_OK(label_b.Finish(&a_label));
    ARROW_RETURN_NOT_OK(chan_b.Finish(&a_chan));
    ARROW_RETURN_NOT_OK(lo_b.Finish(&a_lo));
    ARROW_RETURN_NOT_OK(hi_b.Finish(&a_hi));
    auto schema = arrow::schema(
        {
            arrow::field("wc.frame.cmm.label",     arrow::utf8(),  false),
            arrow::field("wc.frame.cmm.channel",   arrow::int32(), false),
            arrow::field("wc.frame.cmm.bin_start", arrow::int32(), false),
            arrow::field("wc.frame.cmm.bin_end",   arrow::int32(), false),
        },
        arrow::key_value_metadata({{"arrow.schema", "wc.frame.cmm"}}));
    return arrow::Table::Make(schema, {a_label, a_chan, a_lo, a_hi});
}

}  // namespace

arrow::Result<FrameTables>
to_arrow_sparse(const WireCell::IFrame::pointer& frame)
{
    auto* pool = arrow::default_memory_pool();

    arrow::Int32Builder channel_b(pool), tbin_b(pool);
    auto charge_vb = std::make_shared<arrow::FloatBuilder>(pool);
    arrow::ListBuilder charge_b(pool, charge_vb);

    auto traces = frame->traces();
    const int64_t nrows = traces ? static_cast<int64_t>(traces->size()) : 0;
    if (traces) {
        for (const auto& t : *traces) {
            ARROW_RETURN_NOT_OK(channel_b.Append(t->channel()));
            ARROW_RETURN_NOT_OK(tbin_b.Append(t->tbin()));
            ARROW_RETURN_NOT_OK(charge_b.Append());
            const auto& q = t->charge();
            if (!q.empty()) {
                ARROW_RETURN_NOT_OK(charge_vb->AppendValues(q.data(), static_cast<int64_t>(q.size())));
            }
        }
    }

    std::shared_ptr<arrow::Array> a_channel, a_tbin, a_charge;
    ARROW_RETURN_NOT_OK(channel_b.Finish(&a_channel));
    ARROW_RETURN_NOT_OK(tbin_b.Finish(&a_tbin));
    ARROW_RETURN_NOT_OK(charge_b.Finish(&a_charge));

    auto schema = arrow::schema(
        {
            arrow::field("wc.trace.channel", arrow::int32(), false),
            arrow::field("wc.trace.tbin",    arrow::int32(), false),
            arrow::field("wc.trace.charge",  arrow::list(arrow::float32()), false),
        },
        frame_metadata("wc.frame", frame));

    FrameTables out;
    out.traces = arrow::Table::Make(schema, {a_channel, a_tbin, a_charge}, nrows);
    ARROW_ASSIGN_OR_RAISE(out.frame_tags, build_frame_tags(frame));
    ARROW_ASSIGN_OR_RAISE(out.trace_tags, build_trace_tags(frame));
    ARROW_ASSIGN_OR_RAISE(out.cmm,        build_cmm(frame));
    return out;
}

bool is_dense(const WireCell::IFrame::pointer& frame)
{
    auto traces = frame->traces();
    if (!traces || traces->empty()) return true;   // vacuously dense
    const size_t T = (*traces)[0]->charge().size();
    const int tbin = (*traces)[0]->tbin();
    for (const auto& t : *traces) {
        if (t->charge().size() != T) return false;
        if (t->tbin() != tbin) return false;
    }
    return true;
}

arrow::Result<FrameTables>
to_arrow_dense(const WireCell::IFrame::pointer& frame)
{
    if (!is_dense(frame)) {
        return arrow::Status::Invalid("to_arrow_dense: frame is not dense "
                                      "(traces differ in sample count or tbin)");
    }
    auto* pool = arrow::default_memory_pool();

    auto traces = frame->traces();
    const int64_t nrows = traces ? static_cast<int64_t>(traces->size()) : 0;
    int32_t nticks = 0, tbin = 0;
    if (traces && !traces->empty()) {
        nticks = static_cast<int32_t>((*traces)[0]->charge().size());
        tbin   = (*traces)[0]->tbin();
    }

    arrow::Int32Builder channel_b(pool);
    auto charge_vb = std::make_shared<arrow::FloatBuilder>(pool);
    arrow::FixedSizeListBuilder charge_b(pool, charge_vb, nticks);

    if (traces) {
        for (const auto& t : *traces) {
            ARROW_RETURN_NOT_OK(channel_b.Append(t->channel()));
            ARROW_RETURN_NOT_OK(charge_b.Append());   // expects exactly nticks values
            const auto& q = t->charge();
            ARROW_RETURN_NOT_OK(charge_vb->AppendValues(q.data(), static_cast<int64_t>(q.size())));
        }
    }

    std::shared_ptr<arrow::Array> a_channel, a_charge;
    ARROW_RETURN_NOT_OK(channel_b.Finish(&a_channel));
    ARROW_RETURN_NOT_OK(charge_b.Finish(&a_charge));

    auto md = frame_metadata("wc.frame.dense", frame);
    md = md->Merge(*arrow::key_value_metadata({
        {"wc.frame.tbin",   std::to_string(tbin)},
        {"wc.frame.nticks", std::to_string(nticks)},
    }));
    auto schema = arrow::schema(
        {
            arrow::field("wc.trace.channel", arrow::int32(), false),
            arrow::field("wc.trace.charge",  arrow::fixed_size_list(arrow::float32(), nticks), false),
        },
        md);

    FrameTables out;
    out.traces = arrow::Table::Make(schema, {a_channel, a_charge}, nrows);
    ARROW_ASSIGN_OR_RAISE(out.frame_tags, build_frame_tags(frame));
    ARROW_ASSIGN_OR_RAISE(out.trace_tags, build_trace_tags(frame));
    ARROW_ASSIGN_OR_RAISE(out.cmm,        build_cmm(frame));
    return out;
}

}  // namespace WireCell::Arrow
