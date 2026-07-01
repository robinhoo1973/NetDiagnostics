# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Release")
  file(REMOVE_RECURSE
  "CMakeFiles/net_diagnostic_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/net_diagnostic_autogen.dir/ParseCache.txt"
  "net_diagnostic_autogen"
  "tests/CMakeFiles/test_engine_quick_autogen.dir/AutogenUsed.txt"
  "tests/CMakeFiles/test_engine_quick_autogen.dir/ParseCache.txt"
  "tests/test_engine_quick_autogen"
  )
endif()
