set( LLVM_LINK_COMPONENTS
        ${LLVM_TARGETS_TO_BUILD}
        Option
        Support
        )
if (LLVM_BOOST_FOUND)
    if(NOT LLVM_ENABLE_RTTI)
        message(ERROR "Linking Boost requires enabling rtti. Please use -DLLVM_ENABLE_RTTI=ON in cmake.")
    else()
        add_clang_library(CConv
          ArrayBoundsInferenceConsumer.cpp
          ArrayBoundsInformation.cpp
          CConv.cpp
          CConvInteractiveData.cpp
          ConstraintBuilder.cpp
          ConstraintsGraph.cpp
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
        target_link_libraries(obj.CConv ${Boost_GRAPH_LIBRARY})
        add_subdirectory(unittests)
    endif()
endif()


