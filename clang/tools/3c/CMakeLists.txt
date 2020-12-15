set( LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  Option
  Support
  )
if (FIVE_C)
  add_compile_definitions(FIVE_C)
endif()
add_clang_executable(3c
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
target_include_directories(3c PUBLIC)

install(TARGETS 3c
  RUNTIME DESTINATION bin)
