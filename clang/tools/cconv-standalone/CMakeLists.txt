set( LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  Option
  Support
  )
if (FIVE_C)
  add_compile_definitions(FIVE_C)
endif()
add_clang_executable(cconv-standalone
  CConvStandalone.cpp
  )

target_link_libraries(cconv-standalone
  PRIVATE
  CConv
  clangAST
  clangBasic
  clangDriver
  clangFrontend
  clangRewriteFrontend
  clangStaticAnalyzerFrontend
  clangTooling
  )
target_include_directories(cconv-standalone PUBLIC)

install(TARGETS cconv-standalone
  RUNTIME DESTINATION bin)