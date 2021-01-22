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

# clangd3c is unmaintained and believed not to work, but as of 2020-11-22, it
# builds successfully and we continue to include it in some bulk code edits. We
# may revive it in the future.
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
