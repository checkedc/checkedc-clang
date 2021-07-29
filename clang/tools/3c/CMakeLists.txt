set(LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  Option
  Support
  )

# Copy the FIVE_C variable from clang/lib/3C. This relies on
# clang/lib/3C/CMakeLists.txt running before clang/tools/3c/CMakeLists.txt,
# which it does: clang/CMakeLists.txt does add_subdirectory(lib) before
# add_subdirectory(tools).
get_directory_property(FIVE_C DIRECTORY ../../lib/3C DEFINITION FIVE_C)

if (FIVE_C)
  add_compile_definitions(FIVE_C)
endif()

add_clang_tool(3c
  3CStandalone.cpp
  )

target_link_libraries(3c
  PRIVATE
  clang3C
  clangAST
  clangBasic
  clangDriver
  clangFrontend
  clangRewriteFrontend
  clangStaticAnalyzerFrontend
  clangTooling
  )

install(TARGETS 3c
  RUNTIME DESTINATION bin)
