#!/bin/bash

# Note: Do the following before running this script:
# 1. Checkout your old clang master branch (say <somedir>/llvm/tools/clang). Call this SRCDIR.
# 2. Checkout the new monorepo (say <somedir>/src. Call this TGTDIR.
# 3. cd $SRCDIR
# 4. git log --pretty=oneline --format=%H > patchlist
# 5. for i in `cat patchlist`; do git diff ${i}~1 ${i} > ${i}.patch; done

SRCDIR=<>
TGTDIR=<>
PROJ=clang
PATCHLIST=$SRCDIR/patchlist

if [[ ! -f $PATCHLIST ]]; then
  echo "error: $PATCHLIST not found."
  exit 1
fi

SHA=`tail -n1 $PATCHLIST`
if [[ ! -f "${SRCDIR}/${SHA}.patch" ]]; then
  echo "error: ${SHA}.patch not found."
  exit 1
fi

cd $SRCDIR
COMMIT_TITLE="`git log -n 1 --pretty=oneline --format=%s $SHA`"
COMMIT_MSG="`git log -n 1 --pretty=full $SHA`"

cd $TGTDIR
echo "Applying patch $SHA ..."

patch -p1 --directory=$TGTDIR/$PROJ < $SRCDIR/${SHA}.patch
if [[ $? -ne 0 ]]; then
  echo "error: Patch $SHA failed to apply cleanly."
  exit 1
fi

git add $PROJ && git commit -m "${COMMIT_TITLE}

Cherry-picked from ${COMMIT_MSG}"

if [[ $? -ne 0 ]]; then
  echo "error: Cherry-pick failed."
  exit 1
fi

echo "Successfully applied patch $SHA"

sed -i '$ d' $PATCHLIST
rm -rf ${SRCDIR}/${SHA}.patch

echo "Patches remaining: `wc -l $PATCHLIST | cut -f1 -d' '`"
