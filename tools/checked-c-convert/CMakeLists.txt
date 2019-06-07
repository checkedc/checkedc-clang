set( LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  Option
  Support
  )

add_clang_executable(checked-c-convert
  CheckedCConvert.cpp
  ProgramInfo.cpp
  MappingVisitor.cpp
  ConstraintBuilder.cpp
  PersistentSourceLoc.cpp
  Constraints.cpp
  ConstraintVariables.cpp
  Utils.cpp
  )

target_link_libraries(checked-c-convert
  PRIVATE
  clangAST
  clangBasic
  clangDriver
  clangFrontend
  clangRewriteFrontend
  clangStaticAnalyzerFrontend
  clangTooling
  )

add_subdirectory(unittests)

install(TARGETS checked-c-convert
  RUNTIME DESTINATION bin)
