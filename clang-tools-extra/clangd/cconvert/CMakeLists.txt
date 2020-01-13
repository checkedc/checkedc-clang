set( LLVM_LINK_COMPONENTS
        ${LLVM_TARGETS_TO_BUILD}
        Option
        Support
        )

add_clang_library(CConvert
        CheckedCConvert.cpp
        ProgramInfo.cpp
        MappingVisitor.cpp
        ConstraintBuilder.cpp
        PersistentSourceLoc.cpp
        Constraints.cpp
        ConstraintVariables.cpp
        ArrayBoundsInferenceConsumer.cpp
        RewriteUtils.cpp
        IterativeItypeHelper.cpp
        ArrayBoundsInformation.cpp
        CConvInteractive.cpp
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
        CheckedCConvert.cpp
        ProgramInfo.cpp
        MappingVisitor.cpp
        ConstraintBuilder.cpp
        PersistentSourceLoc.cpp
        Constraints.cpp
        ConstraintVariables.cpp
        ArrayBoundsInferenceConsumer.cpp
        RewriteUtils.cpp
        IterativeItypeHelper.cpp
        ArrayBoundsInformation.cpp
        CConvInteractive.cpp
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