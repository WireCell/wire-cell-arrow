#ifndef WIRE_CELL_ARROW_VERSION_H
#define WIRE_CELL_ARROW_VERSION_H

namespace WireCell::Arrow {

/// Return the wire-cell-arrow library version string (matches the CMake
/// project version).  Serves as a trivial linkable symbol for the SHARED
/// library scaffold until the IData<->Arrow converters land.
const char* version();

}  // namespace WireCell::Arrow

#endif  // WIRE_CELL_ARROW_VERSION_H
