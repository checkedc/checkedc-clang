include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

add_clang_tool(clangd
  ClangdMain.cpp
  )

set(LLVM_LINK_COMPONENTS
  support
  Option
  )

target_link_libraries(clangd
  PRIVATE
  clangBasic
  clangDaemon
  CConvert
  clangFormat
  clangFrontend
  clangSema
  clangTooling
  clangToolingCore
  )
