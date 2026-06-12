# wire-cell-arrow/test/CMakeLists.txt

# Build-and-link smoke test: confirms the wire_cell_arrow SHARED library links
# and resolves a symbol.  The Spack GCC runtime libstdc++ is linked explicitly
# (WIRECELL_RUNTIME_LIBS, set in the top-level CMakeLists.txt) to resolve
# GLIBCXX versioned symbols pulled in transitively by WireCell/Arrow.
add_executable(test_version test_version.cpp)
target_link_libraries(test_version PRIVATE wire_cell_arrow
  ${WIRECELL_RUNTIME_LIBS})
add_test(NAME version COMMAND test_version)

# Trivial Arrow + WCT smoke test: builds a trivial Arrow array and constructs a
# concrete WCT IFrame, confirming both Arrow and the WireCell libraries are
# found and linked.  Links Arrow explicitly (the lib uses Arrow only in PRIVATE
# implementation files later, so the facade is not guaranteed to re-export it).
add_executable(test_trivial test_trivial.cpp)
target_link_libraries(test_trivial PRIVATE wire_cell_arrow
  Arrow::arrow_shared
  WireCell::Aux
  ${WIRECELL_RUNTIME_LIBS})
add_test(NAME trivial COMMAND test_trivial)

# wc.trace converter unit test (ddm-85v): to_arrow(ITrace).
add_executable(test_trace_converter test_trace_converter.cpp)
target_link_libraries(test_trace_converter PRIVATE wire_cell_arrow
  Arrow::arrow_shared
  WireCell::Aux
  ${WIRECELL_RUNTIME_LIBS})
add_test(NAME trace_converter COMMAND test_trace_converter)

# ArrowTrace facade round-trip (ddm-ajf): ITrace -> Arrow -> ITrace.
add_executable(test_arrow_trace test_arrow_trace.cpp)
target_link_libraries(test_arrow_trace PRIVATE wire_cell_arrow
  Arrow::arrow_shared
  WireCell::Aux
  ${WIRECELL_RUNTIME_LIBS})
add_test(NAME arrow_trace COMMAND test_arrow_trace)

# wc.depo converter unit test (ddm-0w7): to_arrow(IDepo) incl. prior chain.
add_executable(test_depo_converter test_depo_converter.cpp)
target_link_libraries(test_depo_converter PRIVATE wire_cell_arrow
  Arrow::arrow_shared
  WireCell::Aux
  ${WIRECELL_RUNTIME_LIBS})
add_test(NAME depo_converter COMMAND test_depo_converter)

# Further Arrow+WCT round-trip tests are added by the converter/facade test
# issues.
