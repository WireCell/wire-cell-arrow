// Trivial Arrow + Wire-Cell Toolkit smoke test.
//
// Confirms the build system correctly finds AND links both Apache Arrow and
// WCT: it builds a trivial Arrow array and constructs a concrete WCT IFrame
// (Aux::SimpleFrame), exercising real symbols from libarrow and the WireCell
// libraries.  Exits 0 on success, non-zero on any failure.

#include <arrow/api.h>

#include "WireCellIface/IFrame.h"
#include "WireCellAux/SimpleFrame.h"

#include <iostream>
#include <memory>

int main()
{
    // --- Arrow: build a trivial Int64 array and check it. ---
    arrow::Int64Builder builder;
    if (!builder.Append(42).ok()) {
        std::cerr << "Arrow: Append failed\n";
        return 1;
    }
    std::shared_ptr<arrow::Array> array;
    if (!builder.Finish(&array).ok()) {
        std::cerr << "Arrow: Finish failed\n";
        return 1;
    }
    if (array->length() != 1) {
        std::cerr << "Arrow: unexpected array length " << array->length() << "\n";
        return 1;
    }

    // --- WCT: construct a concrete IFrame and read it back through the
    //     interface (links WireCellAux + WireCellIface + WireCellUtil). ---
    auto simple = std::make_shared<WireCell::Aux::SimpleFrame>(7, 1.0);
    WireCell::IFrame* frame = simple.get();
    if (frame->ident() != 7) {
        std::cerr << "WCT: unexpected frame ident " << frame->ident() << "\n";
        return 1;
    }

    std::cout << "Arrow array length=" << array->length()
              << ", WCT frame ident=" << frame->ident() << "\n";
    return 0;
}
