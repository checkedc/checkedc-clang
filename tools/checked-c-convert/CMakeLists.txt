set( LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  Option
  Support
  )

add_clang_executable(checked-c-convert
  CheckedCConvert.cpp
  NewTyp.cpp
  ProgramInfo.cpp
  MappingVisitor.cpp
  ConstraintBuilder.cpp
  PersistentSourceLoc.cpp
  Constraints.cpp
  )

target_link_libraries(checked-c-convert
  Constraints
  clangAST
  clangBasic
  clangDriver
  clangFrontend
  clangRewriteFrontend
  clangStaticAnalyzerFrontend
  clangTooling
  )

install(TARGETS checked-c-convert
  RUNTIME DESTINATION bin)
