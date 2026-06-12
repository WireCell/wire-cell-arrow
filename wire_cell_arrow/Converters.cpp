#include "wire_cell_arrow/Converters.h"

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

}  // namespace WireCell::Arrow
