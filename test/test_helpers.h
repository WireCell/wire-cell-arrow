// Shared test infrastructure for wire-cell-arrow (ddm-c0l).
//
// Synthetic IData generators + field-by-field comparison helpers used by the
// round-trip (ddm-5zj) and collection-operator tests.  Header-only.

#ifndef WIRE_CELL_ARROW_TEST_HELPERS_H
#define WIRE_CELL_ARROW_TEST_HELPERS_H

#include "WireCellAux/SimpleTrace.h"
#include "WireCellAux/SimpleFrame.h"
#include "WireCellAux/SimpleDepo.h"
#include "WireCellAux/SimpleDepoSet.h"
#include "WireCellAux/SimpleTensor.h"
#include "WireCellAux/SimpleTensorSet.h"
#include "WireCellIface/IFrame.h"
#include "WireCellIface/IDepoSet.h"
#include "WireCellIface/ITensorSet.h"
#include "WireCellUtil/Point.h"
#include "WireCellUtil/Waveform.h"

#include <arrow/api.h>

#include <cstring>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace wcatest {

using namespace WireCell;

// --------------------------------------------------------------------------
// Generators
// --------------------------------------------------------------------------

/// Build a synthetic IFrame.
///  - ntraces==0 yields an empty frame.
///  - sparse=true gives traces of varying length/tbin (not dense-convertible);
///    sparse=false gives uniform length nsamples and tbin 0 (dense-convertible).
///  - with_tags adds a frame tag plus "even"/"odd" trace tags (the "even" tag
///    carrying a summary).
///  - with_cmm adds a small ChannelMaskMap.
inline IFrame::pointer make_fake_frame(int ntraces, int nsamples,
                                       bool with_tags = true, bool with_cmm = true,
                                       bool sparse = true)
{
    ITrace::vector traces;
    for (int i = 0; i < ntraces; ++i) {
        const int len = sparse ? nsamples + (i % 3) : nsamples;
        const int tbin = sparse ? i : 0;
        std::vector<float> q(len);
        for (int j = 0; j < len; ++j) q[j] = static_cast<float>(i * 100 + j);
        traces.push_back(std::make_shared<Aux::SimpleTrace>(10 * (i + 1), tbin, q));
    }

    Waveform::ChannelMaskMap cmm;
    if (with_cmm && ntraces > 0) {
        cmm["bad"][10] = {{1, 4}, {6, 9}};
        cmm["noisy"][20] = {{0, 2}};
    }

    auto sf = std::make_shared<Aux::SimpleFrame>(1000 + ntraces, 1.25e-3, traces, 0.5, cmm);

    if (with_tags && ntraces > 0) {
        sf->tag_frame("solid");
        IFrame::trace_list_t even, odd;
        IFrame::trace_summary_t even_sum;
        for (int i = 0; i < ntraces; ++i) {
            if (i % 2 == 0) { even.push_back(i); even_sum.push_back(0.5 * i); }
            else            { odd.push_back(i); }
        }
        if (!even.empty()) sf->tag_traces("even", even, even_sum);
        if (!odd.empty())  sf->tag_traces("odd", odd);   // no summary
    }
    return sf;
}

/// Build a synthetic IDepoSet.  with_prior gives each depo a 2-deep prior chain.
inline IDepoSet::pointer make_fake_deposet(int ndepos, bool with_prior = true)
{
    IDepo::vector depos;
    for (int i = 0; i < ndepos; ++i) {
        IDepo::pointer prior = nullptr;
        if (with_prior) {
            auto p1 = std::make_shared<Aux::SimpleDepo>(
                90.0 + i, Point(91 + i, 92, 93), 900.0 + i, nullptr, 9.1, 9.2, 700 + i, 211, 9.5);
            prior = std::make_shared<Aux::SimpleDepo>(
                50.0 + i, Point(51 + i, 52, 53), 500.0 + i, p1, 5.1, 5.2, 500 + i, 13, 5.5);
        }
        depos.push_back(std::make_shared<Aux::SimpleDepo>(
            1.0 + i, Point(11 + i, 12, 13), 100.0 + i, prior, 1.1, 1.2, 42 + i, -11, 1.5 + i));
    }
    return std::make_shared<Aux::SimpleDepoSet>(7000 + ndepos, depos);
}

/// Build a synthetic ITensorSet with ntensors tensors of alternating
/// float32/int32 element type.  (Tensors are given an explicit empty
/// Configuration to avoid SimpleTensor::metadata()'s null-deref.)
inline ITensorSet::pointer make_fake_tensorset(int ntensors)
{
    auto tv = std::make_shared<ITensor::vector>();
    for (int i = 0; i < ntensors; ++i) {
        ITensor::shape_t shape{static_cast<size_t>(2 + i), 3};
        Configuration tmd; tmd["index"] = i;
        if (i % 2 == 0) {
            std::vector<float> d((2 + i) * 3);
            for (size_t k = 0; k < d.size(); ++k) d[k] = 1.0f * (i * 10 + k);
            tv->push_back(std::make_shared<Aux::SimpleTensor>(shape, d.data(), tmd));
        } else {
            std::vector<int32_t> d((2 + i) * 3);
            for (size_t k = 0; k < d.size(); ++k) d[k] = static_cast<int32_t>(i * 10 + k);
            tv->push_back(std::make_shared<Aux::SimpleTensor>(shape, d.data(), tmd));
        }
    }
    Configuration md; md["job"] = "synthetic";
    return std::make_shared<Aux::SimpleTensorSet>(8000 + ntensors, md, tv);
}

// --------------------------------------------------------------------------
// Comparison helpers (return true on match; print first mismatch otherwise)
// --------------------------------------------------------------------------

inline bool cmp(bool ok, const char* what)
{
    if (!ok) std::cerr << "MISMATCH: " << what << "\n";
    return ok;
}

// --------------------------------------------------------------------------
// Arrow structural validation
//
// RecordBatch::Make / Table::Make do NOT validate type equality of child
// fields (nested list/struct mismatches only bite at IPC — see beads memory
// arrow-nested-list-struct-builder), so every converter result must be passed
// through ValidateFull() before it is trusted.
// --------------------------------------------------------------------------

inline bool validate_full(const std::shared_ptr<arrow::RecordBatch>& b, const std::string& what)
{
    if (!b) return cmp(false, what.c_str());
    auto st = b->ValidateFull();
    if (!st.ok()) std::cerr << "VALIDATE " << what << ": " << st.ToString() << "\n";
    return st.ok();
}

inline bool validate_full(const std::shared_ptr<arrow::Table>& t, const std::string& what)
{
    if (!t) return cmp(false, what.c_str());
    auto st = t->ValidateFull();
    if (!st.ok()) std::cerr << "VALIDATE " << what << ": " << st.ToString() << "\n";
    return st.ok();
}

inline bool trace_equal(const ITrace& a, const ITrace& b)
{
    return cmp(a.channel() == b.channel(), "trace.channel")
        && cmp(a.tbin() == b.tbin(), "trace.tbin")
        && cmp(a.charge() == b.charge(), "trace.charge");
}

inline bool depo_equal(const IDepo& a, const IDepo& b)
{
    bool ok = cmp(a.time() == b.time(), "depo.time")
        && cmp(a.charge() == b.charge(), "depo.charge")
        && cmp(a.energy() == b.energy(), "depo.energy")
        && cmp(a.id() == b.id(), "depo.id")
        && cmp(a.pdg() == b.pdg(), "depo.pdg")
        && cmp(a.extent_long() == b.extent_long(), "depo.extent_long")
        && cmp(a.extent_tran() == b.extent_tran(), "depo.extent_tran")
        && cmp(a.pos().x() == b.pos().x() && a.pos().y() == b.pos().y()
               && a.pos().z() == b.pos().z(), "depo.pos");
    if (!ok) return false;
    // Recurse the prior chain.
    auto pa = a.prior(), pb = b.prior();
    if ((bool)pa != (bool)pb) return cmp(false, "depo.prior presence");
    if (pa && pb) return depo_equal(*pa, *pb);
    return true;
}

inline bool tensor_equal(const ITensor& a, const ITensor& b)
{
    bool ok = cmp(a.dtype() == b.dtype(), "tensor.dtype")
        && cmp(a.shape() == b.shape(), "tensor.shape")
        && cmp(a.order() == b.order(), "tensor.order")
        && cmp(a.size() == b.size(), "tensor.size");
    if (!ok) return false;
    if (a.size() && std::memcmp(a.data(), b.data(), a.size()) != 0)
        return cmp(false, "tensor.data");
    return true;
}

inline bool frame_equal(const IFrame& a, const IFrame& b)
{
    if (!cmp(a.ident() == b.ident(), "frame.ident")) return false;
    if (!cmp(a.time() == b.time(), "frame.time")) return false;
    if (!cmp(a.tick() == b.tick(), "frame.tick")) return false;

    auto ta = a.traces(); auto tb = b.traces();
    if (!cmp((ta ? ta->size() : 0) == (tb ? tb->size() : 0), "frame.ntraces")) return false;
    if (ta) for (size_t i = 0; i < ta->size(); ++i) if (!trace_equal(*ta->at(i), *tb->at(i))) return false;

    auto as_set = [](const IFrame::tag_list_t& v) { return std::set<std::string>(v.begin(), v.end()); };
    if (!cmp(as_set(a.frame_tags()) == as_set(b.frame_tags()), "frame.frame_tags")) return false;
    if (!cmp(as_set(a.trace_tags()) == as_set(b.trace_tags()), "frame.trace_tags")) return false;
    for (const auto& tag : a.trace_tags()) {
        if (!cmp(a.tagged_traces(tag) == b.tagged_traces(tag), "frame.tagged_traces")) return false;
        if (!cmp(a.trace_summary(tag) == b.trace_summary(tag), "frame.trace_summary")) return false;
    }
    if (!cmp(a.masks() == b.masks(), "frame.masks")) return false;
    return true;
}

inline bool deposet_equal(const IDepoSet& a, const IDepoSet& b)
{
    if (!cmp(a.ident() == b.ident(), "deposet.ident")) return false;
    auto da = a.depos(); auto db = b.depos();
    if (!cmp((da ? da->size() : 0) == (db ? db->size() : 0), "deposet.ndepos")) return false;
    if (da) for (size_t i = 0; i < da->size(); ++i) if (!depo_equal(*da->at(i), *db->at(i))) return false;
    return true;
}

inline bool tensorset_equal(const ITensorSet& a, const ITensorSet& b)
{
    if (!cmp(a.ident() == b.ident(), "tensorset.ident")) return false;
    auto ta = a.tensors(); auto tb = b.tensors();
    if (!cmp((ta ? ta->size() : 0) == (tb ? tb->size() : 0), "tensorset.ntensors")) return false;
    if (ta) for (size_t i = 0; i < ta->size(); ++i) if (!tensor_equal(*ta->at(i), *tb->at(i))) return false;
    return true;
}

}  // namespace wcatest

#endif  // WIRE_CELL_ARROW_TEST_HELPERS_H
