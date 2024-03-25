set(LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  Option
  Support
  )

# We expect users to build either 5C (with all 5C-specific files present) or 3C
# (with none present), so we can check existence of an arbitrary 5C-specific
# file to tell whether this is 3C or 5C.
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/DeclRewriter_5C.cpp)
  set(FIVE_C ON)
endif()

if (FIVE_C)
  add_compile_definitions(FIVE_C)
  set(five_c_source
    DeclRewriter_5C.cpp
  )
endif()

if (MSVC)
  set_source_files_properties(ArrayBoundsInferenceConsumer.cpp PROPERTIES COMPILE_FLAGS /bigobj)
  set_source_files_properties(ConstraintBuilder.cpp PROPERTIES COMPILE_FLAGS /bigobj)
  set_source_files_properties(DeclRewriter.cpp PROPERTIES COMPILE_FLAGS /bigobj)
  set_source_files_properties(RewriteUtils.cpp PROPERTIES COMPILE_FLAGS /bigobj)
endif()

add_clang_library(clang3C
  ABounds.cpp
  AVarBoundsInfo.cpp
  AVarBoundsConflictResolver.cpp
  AVarGraph.cpp
  ArrayBoundsInferenceConsumer.cpp
  CastPlacement.cpp
  3C.cpp
  3CInteractiveData.cpp
  3CStats.cpp
  CheckedRegions.cpp
  ConstraintBuilder.cpp
  ConstraintResolver.cpp
  Constraints.cpp
  ConstraintsGraph.cpp
  ConstraintVariables.cpp
  CtxSensAVarBounds.cpp
  DeclRewriter.cpp
  IntermediateToolHook.cpp
  LowerBoundAssignment.cpp
  MappingVisitor.cpp
  MultiDecls.cpp
  PersistentSourceLoc.cpp
  ProgramInfo.cpp
  ProgramVar.cpp
  RewriteUtils.cpp
  StructInit.cpp
  TypeVariableAnalysis.cpp
  Utils.cpp
  ${five_c_source}
  LINK_LIBS
  clangAST
  clangAnalysis
  clangBasic
  clangDriver
  clangFrontend
  clangRewriteFrontend
  clangStaticAnalyzerFrontend
  clangTooling
  clangTransformer
)
