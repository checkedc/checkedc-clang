set( LLVM_LINK_COMPONENTS
        ${LLVM_TARGETS_TO_BUILD}
        Option
        Support
        )
if (LLVM_BOOST_FOUND)
    if(NOT LLVM_ENABLE_RTTI)
        message(ERROR "Linking Boost requires enabling rtti. Please use -DLLVM_ENABLE_RTTI=ON in cmake.")
    else()
        # This is to populate the boost link libraries.
        add_clang_library(CConv
          ABounds.cpp
          AVarBoundsInfo.cpp
          AVarGraph.cpp
          ArrayBoundsInferenceConsumer.cpp
          ArrayBoundsInformation.cpp
          CastPlacement.cpp
          CConv.cpp
          CConvInteractiveData.cpp
          CheckedRegions.cpp
          ConstraintBuilder.cpp
          ConstraintResolver.cpp
          Constraints.cpp
          ConstraintsGraph.cpp
          ConstraintVariables.cpp
          GatherTool.cpp
          MappingVisitor.cpp
          PersistentSourceLoc.cpp
          ProgramInfo.cpp
          ProgramVar.cpp
          RewriteUtils.cpp
          StructInit.cpp
          Utils.cpp
          LINK_LIBS
          clangAST
          clangAnalysis
          clangBasic
          clangDriver
          clangFrontend
          clangRewriteFrontend
          clangStaticAnalyzerFrontend
          clangTooling
          clangToolingRefactoring
          ${Boost_LIBRARIES}
        )
        add_subdirectory(unittests)
	target_include_directories(obj.CConv PUBLIC ${Boost_INCLUDE_DIRS})
    endif()
endif()


