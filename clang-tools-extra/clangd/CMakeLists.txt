add_subdirectory(support)

# Configure the Features.inc file.
if (NOT DEFINED CLANGD_BUILD_XPC)
  if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(CLANGD_BUILD_XPC_DEFAULT ON)
  else ()
    set(CLANGD_BUILD_XPC_DEFAULT OFF)
  endif ()

  llvm_canonicalize_cmake_booleans(CLANGD_BUILD_XPC_DEFAULT)

  set(CLANGD_BUILD_XPC ${CLANGD_BUILD_XPC_DEFAULT} CACHE BOOL "Build XPC Support For Clangd." FORCE)
  unset(CLANGD_BUILD_XPC_DEFAULT)
endif ()

llvm_canonicalize_cmake_booleans(CLANGD_BUILD_XPC)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/Features.inc.in
  ${CMAKE_CURRENT_BINARY_DIR}/Features.inc
)

set(LLVM_LINK_COMPONENTS
  Support
  AllTargetsInfos
  FrontendOpenMP
  Option
  )

# CMake function to generate clangDaemon and 3cClangDaemon targets, minimizing
# the diff in this file from upstream Clang to ease future merging.
function(add_clangDaemon_target target_name)
cmake_parse_arguments(ARG "" "" "EXTRA_LIBS" ${ARGN})

add_clang_library(${target_name}
  # If we omitted these from the clangDaemon target, llvm_check_source_file_list
  # would raise an error. We could use LLVM_OPTIONAL_SOURCES, but it doesn't
  # seem worth it.
  3CCommands.cpp
  3CDiagnostics.cpp

  AST.cpp
  ClangdLSPServer.cpp
  ClangdServer.cpp
  CodeComplete.cpp
  CodeCompletionStrings.cpp
  CollectMacros.cpp
  CompileCommands.cpp
  Compiler.cpp
  Config.cpp
  ConfigCompile.cpp
  ConfigProvider.cpp
  ConfigYAML.cpp
  Diagnostics.cpp
  DraftStore.cpp
  ExpectedTypes.cpp
  FindSymbols.cpp
  FindTarget.cpp
  FileDistance.cpp
  Format.cpp
  FS.cpp
  FuzzyMatch.cpp
  GlobalCompilationDatabase.cpp
  Headers.cpp
  HeaderSourceSwitch.cpp
  Hover.cpp
  IncludeFixer.cpp
  JSONTransport.cpp
  PathMapping.cpp
  Protocol.cpp
  Quality.cpp
  ParsedAST.cpp
  Preamble.cpp
  RIFF.cpp
  Selection.cpp
  SemanticHighlighting.cpp
  SemanticSelection.cpp
  SourceCode.cpp
  QueryDriverDatabase.cpp
  TUScheduler.cpp
  URI.cpp
  XRefs.cpp

  index/Background.cpp
  index/BackgroundIndexLoader.cpp
  index/BackgroundIndexStorage.cpp
  index/BackgroundQueue.cpp
  index/BackgroundRebuild.cpp
  index/CanonicalIncludes.cpp
  index/FileIndex.cpp
  index/Index.cpp
  index/IndexAction.cpp
  index/MemIndex.cpp
  index/Merge.cpp
  index/Ref.cpp
  index/Relation.cpp
  index/Serialization.cpp
  index/Symbol.cpp
  index/SymbolCollector.cpp
  index/SymbolID.cpp
  index/SymbolLocation.cpp
  index/SymbolOrigin.cpp
  index/YAMLSerialization.cpp

  index/dex/Dex.cpp
  index/dex/Iterator.cpp
  index/dex/PostingList.cpp
  index/dex/Trigram.cpp

  refactor/Rename.cpp
  refactor/Tweak.cpp

  LINK_LIBS
  clangdSupport
  clangTidy
  ${LLVM_PTHREAD_LIB}
  ${ALL_CLANG_TIDY_CHECKS}

  DEPENDS
  omp_gen
  )

clang_target_link_libraries(${target_name}
  PRIVATE
  clangAST
  clangASTMatchers
  clangBasic
  clangDriver
  clangFormat
  clangFrontend
  clangIndex
  clangLex
  clangSema
  clangSerialization
  clangTooling
  clangToolingCore
  clangToolingInclusions
  clangToolingRefactoring
  clangToolingSyntax
  ${ARG_EXTRA_LIBS}
  )

endfunction()

# Some whole-program analysis tools (including IDEs) do not cope well with the
# clangd source files being included in the compilation database twice with
# different preprocessor flags that lead to significantly different portions of
# each file being included in the compilation. To avoid this problem,
# CLANGD_EXCLUSIVE_VARIANT can be set to include only one variant of clangd in
# the configuration. This can be used, for example, to rename a program element
# that is part of 3C and update references to it in clangd3c. If you want to
# rename an upstream Clang program element that is referenced in both
# INTERACTIVE3C and !INTERACTIVE3C blocks, this won't help; fortunately, we have
# never needed to do that so far.
set(CLANGD_KNOWN_VARIANTS "clangd;clangd3c")
set(CLANGD_EXCLUSIVE_VARIANT "" CACHE STRING
  "Variant of clangd to include in the configuration. If empty, include all variants.")
set_property(CACHE CLANGD_EXCLUSIVE_VARIANT PROPERTY STRINGS ";${CLANGD_KNOWN_VARIANTS}")
foreach(variant ${CLANGD_KNOWN_VARIANTS})
  if(CLANGD_EXCLUSIVE_VARIANT STREQUAL "" OR CLANGD_EXCLUSIVE_VARIANT STREQUAL variant)
    set(CLANGD_INCLUDE_VARIANT_${variant} ON)
  endif()
endforeach()

if(CLANGD_INCLUDE_VARIANT_clangd)
add_clangDaemon_target(clangDaemon)
endif()

if(CLANGD_INCLUDE_VARIANT_clangd3c)
add_clangDaemon_target(3cClangDaemon EXTRA_LIBS clang3C)
target_compile_definitions(obj.3cClangDaemon PRIVATE INTERACTIVE3C=1)
endif()

add_subdirectory(refactor/tweaks)
if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  # FIXME: Make fuzzer not use linux-specific APIs, build it everywhere.
  add_subdirectory(fuzzer)
endif()
add_subdirectory(tool)
add_subdirectory(indexer)

if (LLVM_INCLUDE_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()
if ( CLANGD_BUILD_XPC )
  add_subdirectory(xpc)
endif ()

if(CLANG_INCLUDE_TESTS AND CLANGD_INCLUDE_VARIANT_clangd)
add_subdirectory(test)
add_subdirectory(unittests)
endif()

# FIXME(kirillbobyrev): Document this in the LLVM docs once remote index is stable.
option(CLANGD_ENABLE_REMOTE "Use gRPC library to enable remote index support for Clangd" OFF)
set(GRPC_INSTALL_PATH "" CACHE PATH "Path to gRPC library manual installation.")

if (CLANGD_ENABLE_REMOTE)
  include(FindGRPC)
endif()
add_subdirectory(index/remote)
add_subdirectory(index/dex/dexp)
