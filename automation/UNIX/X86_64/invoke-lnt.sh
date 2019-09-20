# Test clang on UNIX using Visual Studio Team Services.

set -ue
set -o pipefail
set -x

CC=clang
CXX=${CC}++
CFLAGS="-fcheckedc-extension"

if [[ "$BMARK" = "yes" ]]; then
  "$LNT_SCRIPT" runtest test-suite \
    -v \
    --sandbox "$RESULTS_DIR" \
    --cc "$CC" \
    --cxx "$CXX" \
    --cflags "$CFLAGS" \
    --test-suite "${BUILD_SOURCESDIRECTORY}/llvm-test-suite" \
    --submit "${LNT_DB_DIR}" \
    --threads 1 \
    --build-threads 1 \
    --only-test "SingleSource/Benchmarks/Linpack" \
    --run-order "$USER" \
    2>&1 | tee ${RESULT_SUMMARY}

else
  "$LNT_SCRIPT" runtest nt \
    -v \
    --sandbox "$RESULTS_DIR" \
    --cc "$CC" \
    --cxx "$CXX" \
    --cflags "$CFLAGS" \
    --test-suite "${BUILD_SOURCESDIRECTORY}/llvm-test-suite" \
    --output "${RESULT_DATA}" \
    -j${BUILD_CPU_COUNT} \
    2>&1 | tee ${RESULT_SUMMARY}
fi

set +ue
set +o pipefail
