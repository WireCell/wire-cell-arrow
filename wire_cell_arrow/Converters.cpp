#include "wire_cell_arrow/Converters.hpp"

#include "WireCellUtil/Persist.h"

#include <charconv>
#include <stdexcept>
#include <string>
#include <vector>

namespace WireCell::Arrow {

namespace {

// Build the semantic schema metadata for `name`: the schema name + version
// (kSchemaNameKey / kSchemaVersionKey) plus any caller-supplied extra keys.
// Centralizing this guarantees every wc.* schema is versioned identically.
std::shared_ptr<arrow::KeyValueMetadata> semantic_metadata(
    const std::string& name,
    std::vector<std::string> extra_keys = {},
    std::vector<std::string> extra_vals = {})
{
    std::vector<std::string> keys{kSchemaNameKey, kSchemaVersionKey};
    std::vector<std::string> vals{name, std::to_string(kSchemaVersion)};
    keys.insert(keys.end(), std::make_move_iterator(extra_keys.begin()),
                std::make_move_iterator(extra_keys.end()));
    vals.insert(vals.end(), std::make_move_iterator(extra_vals.begin()),
                std::make_move_iterator(extra_vals.end()));
    return std::make_shared<arrow::KeyValueMetadata>(std::move(keys), std::move(vals));
}

}  // namespace

int schema_version(const std::shared_ptr<arrow::Schema>& schema)
{
    if (!schema) return 0;
    auto md = schema->metadata();
    if (!md) return 0;
    const int i = md->FindKey(kSchemaVersionKey);
    if (i < 0) return 0;
    try {
        return std::stoi(md->value(i));
    }
    catch (...) {
        return 0;
    }
}

void require_readable_schema(const std::shared_ptr<arrow::Schema>& schema,
                             const std::string& context)
{
    const int v = schema_version(schema);
    if (v > kSchemaVersion) {
        // Prefer the schema's own recorded name; fall back to the caller's hint.
        std::string name = context;
        if (schema) {
            if (auto md = schema->metadata()) {
                const int i = md->FindKey(kSchemaNameKey);
                if (i >= 0) name = md->value(i);
            }
        }
        throw std::runtime_error(
            "wire-cell-arrow: " + name + " schema version " + std::to_string(v) +
            " is newer than this build supports (version " + std::to_string(kSchemaVersion) +
            "); upgrade wire-cell-arrow to read this data.");
    }
}

std::shared_ptr<arrow::Schema> trace_schema()
{
    auto md = semantic_metadata("wc.trace");
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
    auto md = semantic_metadata("wc.depo");
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

namespace {
// Builders for the shared wc.depo / wc.deposet row schema (10 flat columns +
// the priors list<struct>).  append() adds one depo row (flat fields + its
// prior() chain as structs, most-recent-first); finish() yields the 11 column
// arrays in schema order.  Used by both the single-depo and deposet converters.
struct DepoRowBuilders {
    arrow::DoubleBuilder time, charge, energy, x, y, z, el, et;
    arrow::Int32Builder id, pdg;
    std::shared_ptr<arrow::StructBuilder> sb;
    std::shared_ptr<arrow::ListBuilder> priors;

    explicit DepoRowBuilders(arrow::MemoryPool* pool)
      : time(pool), charge(pool), energy(pool), x(pool), y(pool), z(pool),
        el(pool), et(pool), id(pool), pdg(pool)
    {
        std::vector<std::shared_ptr<arrow::ArrayBuilder>> sf = {
            std::make_shared<arrow::DoubleBuilder>(pool), std::make_shared<arrow::DoubleBuilder>(pool),
            std::make_shared<arrow::DoubleBuilder>(pool), std::make_shared<arrow::DoubleBuilder>(pool),
            std::make_shared<arrow::DoubleBuilder>(pool), std::make_shared<arrow::DoubleBuilder>(pool),
            std::make_shared<arrow::DoubleBuilder>(pool), std::make_shared<arrow::DoubleBuilder>(pool),
            std::make_shared<arrow::Int32Builder>(pool),  std::make_shared<arrow::Int32Builder>(pool),
        };
        sb = std::make_shared<arrow::StructBuilder>(depo_struct_type(), pool, sf);
        priors = std::make_shared<arrow::ListBuilder>(pool, sb);
    }

    arrow::Status append(const WireCell::IDepo::pointer& d)
    {
        ARROW_RETURN_NOT_OK(append_depo_fields(d, &time, &charge, &energy, &x, &y, &z,
                                               &el, &et, &id, &pdg));
        ARROW_RETURN_NOT_OK(priors->Append());   // open this row's priors list
        for (auto p = d->prior(); p; p = p->prior()) {
            ARROW_RETURN_NOT_OK(sb->Append());
            ARROW_RETURN_NOT_OK(append_depo_fields(p,
                static_cast<arrow::DoubleBuilder*>(sb->field_builder(0)),
                static_cast<arrow::DoubleBuilder*>(sb->field_builder(1)),
                static_cast<arrow::DoubleBuilder*>(sb->field_builder(2)),
                static_cast<arrow::DoubleBuilder*>(sb->field_builder(3)),
                static_cast<arrow::DoubleBuilder*>(sb->field_builder(4)),
                static_cast<arrow::DoubleBuilder*>(sb->field_builder(5)),
                static_cast<arrow::DoubleBuilder*>(sb->field_builder(6)),
                static_cast<arrow::DoubleBuilder*>(sb->field_builder(7)),
                static_cast<arrow::Int32Builder*>(sb->field_builder(8)),
                static_cast<arrow::Int32Builder*>(sb->field_builder(9))));
        }
        return arrow::Status::OK();
    }

    arrow::Status finish(std::vector<std::shared_ptr<arrow::Array>>& out)
    {
        out.resize(11);
        ARROW_RETURN_NOT_OK(time.Finish(&out[0]));
        ARROW_RETURN_NOT_OK(charge.Finish(&out[1]));
        ARROW_RETURN_NOT_OK(energy.Finish(&out[2]));
        ARROW_RETURN_NOT_OK(x.Finish(&out[3]));
        ARROW_RETURN_NOT_OK(y.Finish(&out[4]));
        ARROW_RETURN_NOT_OK(z.Finish(&out[5]));
        ARROW_RETURN_NOT_OK(el.Finish(&out[6]));
        ARROW_RETURN_NOT_OK(et.Finish(&out[7]));
        ARROW_RETURN_NOT_OK(id.Finish(&out[8]));
        ARROW_RETURN_NOT_OK(pdg.Finish(&out[9]));
        ARROW_RETURN_NOT_OK(priors->Finish(&out[10]));
        return arrow::Status::OK();
    }
};
}  // namespace

arrow::Result<std::shared_ptr<arrow::RecordBatch>>
to_arrow(const WireCell::IDepo::pointer& depo)
{
    DepoRowBuilders b(arrow::default_memory_pool());
    ARROW_RETURN_NOT_OK(b.append(depo));
    std::vector<std::shared_ptr<arrow::Array>> cols;
    ARROW_RETURN_NOT_OK(b.finish(cols));
    return arrow::RecordBatch::Make(depo_schema(), /*num_rows=*/1, cols);
}

std::shared_ptr<arrow::Schema> deposet_schema(int ident)
{
    // Same columns as wc.depo; differ only by schema metadata.
    auto base = depo_schema();
    auto md = semantic_metadata("wc.deposet", {"wc.deposet.ident"}, {std::to_string(ident)});
    return arrow::schema(base->fields(), md);
}

arrow::Result<std::shared_ptr<arrow::Table>>
to_arrow(const WireCell::IDepoSet::pointer& deposet)
{
    DepoRowBuilders b(arrow::default_memory_pool());
    auto depos = deposet->depos();
    const int64_t nrows = depos ? static_cast<int64_t>(depos->size()) : 0;
    if (depos) {
        for (const auto& d : *depos) {
            ARROW_RETURN_NOT_OK(b.append(d));
        }
    }
    std::vector<std::shared_ptr<arrow::Array>> cols;
    ARROW_RETURN_NOT_OK(b.finish(cols));
    return arrow::Table::Make(deposet_schema(deposet->ident()), cols, nrows);
}

// ---------------------------------------------------------------------------
// wc.tensor
// ---------------------------------------------------------------------------

std::shared_ptr<arrow::Schema> tensor_schema()
{
    auto md = semantic_metadata("wc.tensor");
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

namespace {
// Builders for the shared wc.tensor / wc.tensorset row schema.  Used by both
// the single-tensor and tensorset converters.
struct TensorRowBuilders {
    arrow::LargeBinaryBuilder data;
    arrow::StringBuilder dtype;
    std::shared_ptr<arrow::Int64Builder> shape_vb, order_vb;
    std::shared_ptr<arrow::ListBuilder> shape, order;
    arrow::StringBuilder meta;

    explicit TensorRowBuilders(arrow::MemoryPool* pool)
      : data(pool), dtype(pool)
      , shape_vb(std::make_shared<arrow::Int64Builder>(pool))
      , order_vb(std::make_shared<arrow::Int64Builder>(pool))
      , shape(std::make_shared<arrow::ListBuilder>(pool, shape_vb))
      , order(std::make_shared<arrow::ListBuilder>(pool, order_vb))
      , meta(pool)
    {}

    arrow::Status append(const WireCell::ITensor::pointer& t)
    {
        const auto nbytes = static_cast<int64_t>(t->size());
        if (nbytes > 0) {
            ARROW_RETURN_NOT_OK(data.Append(reinterpret_cast<const uint8_t*>(t->data()), nbytes));
        } else {
            ARROW_RETURN_NOT_OK(data.AppendEmptyValue());
        }
        ARROW_RETURN_NOT_OK(dtype.Append(t->dtype()));
        ARROW_RETURN_NOT_OK(shape->Append());
        for (auto s : t->shape()) ARROW_RETURN_NOT_OK(shape_vb->Append(static_cast<int64_t>(s)));
        ARROW_RETURN_NOT_OK(order->Append());
        for (auto o : t->order()) ARROW_RETURN_NOT_OK(order_vb->Append(static_cast<int64_t>(o)));
        auto cfg = t->metadata();
        if (cfg.isNull()) {
            ARROW_RETURN_NOT_OK(meta.AppendNull());
        } else {
            ARROW_RETURN_NOT_OK(meta.Append(WireCell::Persist::dumps(cfg)));
        }
        return arrow::Status::OK();
    }

    arrow::Status finish(std::vector<std::shared_ptr<arrow::Array>>& out)
    {
        out.resize(5);
        ARROW_RETURN_NOT_OK(data.Finish(&out[0]));
        ARROW_RETURN_NOT_OK(dtype.Finish(&out[1]));
        ARROW_RETURN_NOT_OK(shape->Finish(&out[2]));
        ARROW_RETURN_NOT_OK(order->Finish(&out[3]));
        ARROW_RETURN_NOT_OK(meta.Finish(&out[4]));
        return arrow::Status::OK();
    }
};
}  // namespace

arrow::Result<std::shared_ptr<arrow::RecordBatch>>
to_arrow(const WireCell::ITensor::pointer& tensor)
{
    TensorRowBuilders b(arrow::default_memory_pool());
    ARROW_RETURN_NOT_OK(b.append(tensor));
    std::vector<std::shared_ptr<arrow::Array>> cols;
    ARROW_RETURN_NOT_OK(b.finish(cols));
    return arrow::RecordBatch::Make(tensor_schema(), /*num_rows=*/1, cols);
}

std::shared_ptr<arrow::Schema> tensorset_schema(int ident, const std::string& metadata_json)
{
    auto base = tensor_schema();
    std::vector<std::string> keys = {"wc.tensorset.ident"};
    std::vector<std::string> vals = {std::to_string(ident)};
    if (!metadata_json.empty()) {
        keys.push_back("wc.tensorset.metadata");
        vals.push_back(metadata_json);
    }
    auto md = semantic_metadata("wc.tensorset", std::move(keys), std::move(vals));
    return arrow::schema(base->fields(), md);
}

arrow::Result<std::shared_ptr<arrow::Table>>
to_arrow(const WireCell::ITensorSet::pointer& tensorset)
{
    TensorRowBuilders b(arrow::default_memory_pool());
    auto tensors = tensorset->tensors();
    const int64_t nrows = tensors ? static_cast<int64_t>(tensors->size()) : 0;
    if (tensors) {
        for (const auto& t : *tensors) {
            ARROW_RETURN_NOT_OK(b.append(t));
        }
    }
    std::vector<std::shared_ptr<arrow::Array>> cols;
    ARROW_RETURN_NOT_OK(b.finish(cols));

    auto cfg = tensorset->metadata();
    std::string md_json = cfg.isNull() ? std::string() : WireCell::Persist::dumps(cfg);
    return arrow::Table::Make(tensorset_schema(tensorset->ident(), md_json), cols, nrows);
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

arrow::Result<std::shared_ptr<arrow::RecordBatch>>
table_to_batch(const std::shared_ptr<arrow::Table>& table)
{
    ARROW_ASSIGN_OR_RAISE(auto combined, table->CombineChunks());
    if (combined->num_rows() == 0) {
        return arrow::RecordBatch::MakeEmpty(combined->schema());
    }
    std::vector<std::shared_ptr<arrow::Array>> arrays;
    for (const auto& col : combined->columns()) {
        arrays.push_back(col->chunk(0));
    }
    return arrow::RecordBatch::Make(combined->schema(), combined->num_rows(), arrays);
}

namespace {

// Frame-scalar schema metadata: arrow.schema + ident/time/tick (time/tick as
// bit-exact hexfloat).
std::shared_ptr<arrow::KeyValueMetadata> frame_metadata(const std::string& schema_name,
                                                        const WireCell::IFrame::pointer& f)
{
    return semantic_metadata(
        schema_name,
        {"wc.frame.ident", "wc.frame.time", "wc.frame.tick"},
        {std::to_string(f->ident()), hexfloat(f->time()), hexfloat(f->tick())});
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
        semantic_metadata("wc.frame.frame_tags"));
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
        semantic_metadata("wc.frame.trace_tags"));
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
        semantic_metadata("wc.frame.cmm"));
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
