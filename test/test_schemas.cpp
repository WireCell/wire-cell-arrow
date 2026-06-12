// Schema-definition validation (ddm-ppo): pure schema factories, no conversion.
#include "wire_cell_arrow/Converters.h"
#include <arrow/api.h>
#include <iostream>

namespace A = WireCell::Arrow;
static int fails = 0;
static void check(bool ok, const std::string& n){ std::cout<<(ok?"ok   ":"FAIL ")<<n<<"\n"; if(!ok)++fails; }

static void field_is(const std::shared_ptr<arrow::Schema>& s, int i,
                     const std::string& name, arrow::Type::type tid)
{
    check(i < s->num_fields() && s->field(i)->name()==name && s->field(i)->type()->id()==tid,
          "field["+std::to_string(i)+"]="+name);
}
static void meta_is(const std::shared_ptr<arrow::Schema>& s, const std::string& k, const std::string& v)
{
    auto md = s->metadata();
    check(md && md->Get(k).ok() && md->Get(k).ValueOr("")==v, "meta "+k+"="+v);
}

int main()
{
    auto tr = A::trace_schema();
    field_is(tr,0,"wc.trace.channel",arrow::Type::INT32);
    field_is(tr,1,"wc.trace.tbin",arrow::Type::INT32);
    field_is(tr,2,"wc.trace.charge",arrow::Type::LIST);
    meta_is(tr,"arrow.schema","wc.trace");

    auto dp = A::depo_schema();
    check(dp->num_fields()==11, "depo 11 fields");
    field_is(dp,0,"wc.depo.time",arrow::Type::DOUBLE);
    field_is(dp,8,"wc.depo.id",arrow::Type::INT32);
    field_is(dp,10,"wc.depo.priors",arrow::Type::LIST);
    meta_is(dp,"arrow.schema","wc.depo");
    check(A::depo_struct_type()->id()==arrow::Type::STRUCT
          && A::depo_struct_type()->num_fields()==10, "depo_struct_type");

    auto dps = A::deposet_schema(42);
    meta_is(dps,"arrow.schema","wc.deposet");
    meta_is(dps,"wc.deposet.ident","42");
    check(dps->num_fields()==dp->num_fields(), "deposet shares depo columns");

    auto te = A::tensor_schema();
    check(te->num_fields()==5, "tensor 5 fields");
    field_is(te,0,"wc.tensor.data",arrow::Type::LARGE_BINARY);
    field_is(te,1,"wc.tensor.dtype",arrow::Type::STRING);
    field_is(te,2,"wc.tensor.shape",arrow::Type::LIST);
    field_is(te,4,"wc.tensor.metadata",arrow::Type::STRING);
    check(te->field(4)->nullable(), "tensor.metadata nullable");
    meta_is(te,"arrow.schema","wc.tensor");

    auto tes = A::tensorset_schema(9, "{}");
    meta_is(tes,"arrow.schema","wc.tensorset");
    meta_is(tes,"wc.tensorset.ident","9");
    check(tes->metadata()->Get("wc.tensorset.metadata").ok(), "tensorset.metadata present");

    std::cout << (fails ? "SCHEMA FAILURES\n" : "all schemas OK\n");
    return fails ? 1 : 0;
}
