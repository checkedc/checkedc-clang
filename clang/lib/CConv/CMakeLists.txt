set( LLVM_LINK_COMPONENTS
        ${LLVM_TARGETS_TO_BUILD}
        Option
        Support
        )

add_clang_library(CConv
  ArrayBoundsInferenceConsumer.cpp
  ArrayBoundsInformation.cpp
  CConv.cpp
  CConvInteractiveData.cpp
  ConstraintBuilder.cpp
  ConstraintVariables.cpp
  Constraints.cpp
  GatherTool.cpp
  IterativeItypeHelper.cpp
  MappingVisitor.cpp
  PersistentSourceLoc.cpp
  ProgramInfo.cpp
  RewriteUtils.cpp
  Utils.cpp
  LINK_LIBS
  clangAST
  clangBasic
  clangDriver
  clangFrontend
  clangRewriteFrontend
  clangStaticAnalyzerFrontend
  clangTooling
)

add_subdirectory(unittests)
