include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/..)

add_clang_tool(clangd
  ClangdMain.cpp
  $<TARGET_OBJECTS:obj.clangDaemonTweaks>
  )

set(LLVM_LINK_COMPONENTS
  support
  )

set(CLANGD_XPC_LIBS "")
if(CLANGD_BUILD_XPC)
  list(APPEND CLANGD_XPC_LIBS "clangdXpcJsonConversions" "clangdXpcTransport")
endif()

target_link_libraries(clangd
  PRIVATE
  clangAST
  clangBasic
  clangTidy
  clangDaemon
  clangFormat
  clangFrontend
  clangSema
  clangTooling
  clangToolingCore
  clangToolingSyntax
  ${CLANGD_XPC_LIBS}
  )

# clangd3c is unmaintained and has failed to build (under its old name, icconv)
# at least since 3892e314426e8d90b4d3d411e1aaaf636dac47ea (which was merged to
# CCI's mainline in 0f8d6dc6f31b77af1c7fc593ed16680cf3b6c40e on 2020-10-22),
# though we continue to include it in some bulk code edits.  We may revive it in
# the future.
if(FALSE)
add_clang_tool(clangd3c
        ClangdMain.cpp
        $<TARGET_OBJECTS:obj.clangDaemonTweaks>
        )

target_link_libraries(clangd3c
  PRIVATE
  clangAST
  clangBasic
  clangTidy
  3cClangDaemon
  clang3C
  clangFormat
  clangFrontend
  clangSema
  clangTooling
  clangToolingCore
  clangToolingSyntax
  ${CLANGD_XPC_LIBS}
  )

target_compile_definitions(clangd3c PUBLIC INTERACTIVE3C=1)
endif()
