# Test clang on UNIX using Visual Studio Team Services

set -ue
set -o pipefail
set -x

if [ -z "$LNT" ]; then
  exit 0;
fi

export PATH=$LLVM_OBJ_DIR/bin:$PATH
if [ ! -e "`which clang`" ]; then
  echo "clang compiler not found"
  exit 1
fi

if [ -z "$TEST_TARGET_ARCH" ]; then
  echo "TEST_TARGET_ARCH not set"
  exit 1
fi

for TEST_TARGET in $TEST_TARGET_ARCH; do
  if [ ! -d $TEST_TARGET ]; then
    echo "Unknown TEST_TARGET_ARCH value $TEST_TARGET"
    exit 1
  fi

  export RESULTS_DIR=$LNT_RESULTS_DIR/$TEST_TARGET
  mkdir -p $RESULTS_DIR
  export RESULT_DATA="${RESULTS_DIR}/data.xml"
  export RESULT_SUMMARY="${RESULTS_DIR}/result.log"

  echo "Testing LNT for $TEST_TARGET target"
  $TEST_TARGET/invoke-lnt.sh

  if grep FAIL $RESULT_SUMMARY; then
    echo "LNT testing failed."
    exit 1
  else
    if [ $? -eq 2 ]; then
      echo "Grep of LNT result log unexpectedly failed."
      exit 1
    fi
  fi
done

set +ue
set +o pipefail
