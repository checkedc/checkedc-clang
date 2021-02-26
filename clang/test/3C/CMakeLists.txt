# Custom `check-3c` target that depends only on the tools used by the 3C
# regression tests, unlike `check-clang-3c`, which depends on all of
# CLANG_TEST_DEPS. This may significantly speed up the build.

# If you want to run `lit` yourself, you can use `check-3c-deps` to build the
# dependencies of the test suite without running it.
add_custom_target(check-3c-deps
  DEPENDS
  # Tools actually used by our RUN lines.
  3c
  clang
  FileCheck
  # llvm_config.use_default_substitutions() in ../lit.cfg.py refuses to run if
  # these do not exist.
  count
  not
  # llvm_config.feature_config(...) in ../lit.cfg.py directly runs llvm-config
  # to gather some information.
  llvm-config
  )

# The llvm_config.add_tool_substitutions(...) call in ../lit.cfg.py looks for
# some additional tools and warns if they don't exist. Currently, we allow these
# warnings to be printed. If the warnings bother us, we could build the tools;
# they account for a small fraction of the difference between check-3c-deps and
# CLANG_TEST_DEPS.
#
# Note that lit.local.cfg runs after ../lit.cfg.py and thus cannot remove any of
# the tool dependencies. We might be able to overcome that by defining our own
# lit.cfg.py, but that doesn't seem worthwhile since it risks making the
# configuration of the 3C tests diverge more seriously from that of the other
# Clang tests, even when the 3C tests are run via check-clang-3c or the like.

add_lit_testsuite(check-3c "Running the 3C regression tests"
  ${CMAKE_CURRENT_BINARY_DIR}
  PARAMS ${CLANG_TEST_PARAMS}
  DEPENDS check-3c-deps
  ARGS ${CLANG_TEST_EXTRA_ARGS}
  )
