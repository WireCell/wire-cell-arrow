// Trivial build-and-link smoke test for the wire_cell_arrow scaffold.
//
// Confirms the SHARED library links and a symbol resolves.  The substantive
// Arrow+WCT round-trip smoke test is added separately (see issue ddm-l4x).

#include "wire_cell_arrow/Version.hpp"

#include <cstring>
#include <iostream>

int main()
{
    const char* v = WireCell::Arrow::version();
    if (v == nullptr || std::strlen(v) == 0) {
        std::cerr << "wire_cell_arrow::version() returned empty\n";
        return 1;
    }
    std::cout << "wire_cell_arrow " << v << "\n";
    return 0;
}
