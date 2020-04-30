set( LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  Option
  Support
  )

add_clang_executable(checked-c-convert
  ArrayBoundsInferenceConsumer.cpp
  ArrayBoundsInformation.cpp
  CheckedCConvert.cpp
  ConstraintBuilder.cpp
  ConstraintVariables.cpp
  Constraints.cpp
  IterativeItypeHelper.cpp
  MappingVisitor.cpp
  ProgramInfo.cpp
  PersistentSourceLoc.cpp
  RewriteUtils.cpp
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
