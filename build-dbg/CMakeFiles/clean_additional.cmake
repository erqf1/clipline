# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles/clipline_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/clipline_autogen.dir/ParseCache.txt"
  "CMakeFiles/rnnoise_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/rnnoise_autogen.dir/ParseCache.txt"
  "clipline_autogen"
  "rnnoise_autogen"
  )
endif()
