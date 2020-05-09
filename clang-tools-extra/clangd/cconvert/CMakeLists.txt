set( LLVM_LINK_COMPONENTS
        ${LLVM_TARGETS_TO_BUILD}
        Option
        Support
        )

add_clang_library(CConvert
  ArrayBoundsInferenceConsumer.cpp
  ArrayBoundsInformation.cpp
  CConvInteractive.cpp
  CConvInteractiveData.cpp
  CheckedCConvert.cpp
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

add_clang_tool(CConvertStandalone
  ArrayBoundsInferenceConsumer.cpp
  ArrayBoundsInformation.cpp
  CConvInteractive.cpp
  CConvInteractiveData.cpp
  CheckedCConvert.cpp
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
)

target_link_libraries(CConvertStandalone
        PRIVATE
        clangAST
        clangBasic
        clangDriver
        clangFrontend
        clangRewriteFrontend
        clangStaticAnalyzerFrontend
        clangTooling)

target_compile_definitions(CConvertStandalone PRIVATE CCCONVSTANDALONE)
