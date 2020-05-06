set( LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  Option
  Support
  )
if (LLVM_BOOST_FOUND)
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

    install(TARGETS cconv-standalone
      RUNTIME DESTINATION bin)
endif()
