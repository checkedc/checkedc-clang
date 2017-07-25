set(LLVM_LINK_COMPONENTS
  Support
  )

  add_clang_unittest(PartitionRefinementTests
    PartitionRefinementTests.cpp
  )

  target_link_libraries(PartitionRefinementTests
  clangAnalysis
  clangAST
  clangBasic
  )
