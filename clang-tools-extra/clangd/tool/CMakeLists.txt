include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/..)

# Same idea as add_clangDaemon_target in ../CMakeLists.txt.
function(add_clangd_target_part1 target_name)
add_clang_tool(${target_name}
  ClangdMain.cpp
  $<TARGET_OBJECTS:obj.clangDaemonTweaks>
  )
endfunction()

if(CLANGD_INCLUDE_VARIANT_clangd)
add_clangd_target_part1(clangd)
endif()

# clangd3c is unmaintained and believed not to work, but as of 2020-11-22, it
# builds successfully and we continue to include it in some bulk code edits. We
# may revive it in the future.
if(CLANGD_INCLUDE_VARIANT_clangd3c)
add_clangd_target_part1(clangd3c)
endif()

set(LLVM_LINK_COMPONENTS
  support
  )

set(CLANGD_XPC_LIBS "")
if(CLANGD_BUILD_XPC)
  list(APPEND CLANGD_XPC_LIBS "clangdXpcJsonConversions" "clangdXpcTransport")
endif()

function(add_clangd_target_part2 target_name clangDaemon_target_name)
cmake_parse_arguments(ARG "" "" "EXTRA_LIBS" ${ARGN})

clang_target_link_libraries(${target_name}
  PRIVATE
  clangAST
  clangBasic
  clangFormat
  clangFrontend
  clangLex
  clangSema
  clangTooling
  clangToolingCore
  clangToolingRefactoring
  clangToolingSyntax
  ${ARG_EXTRA_LIBS}
  )
target_link_libraries(${target_name}
  PRIVATE
  clangTidy
  ${clangDaemon_target_name}
  clangdSupport
  ${CLANGD_XPC_LIBS}
  )

endfunction()

if(CLANGD_INCLUDE_VARIANT_clangd)
add_clangd_target_part2(clangd clangDaemon)
endif()

if(CLANGD_INCLUDE_VARIANT_clangd3c)
add_clangd_target_part2(clangd3c 3cClangDaemon EXTRA_LIBS clang3C)
target_compile_definitions(clangd3c PUBLIC INTERACTIVE3C=1)
endif()
