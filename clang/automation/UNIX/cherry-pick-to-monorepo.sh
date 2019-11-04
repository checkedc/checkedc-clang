#!/bin/bash

# Note: Do the following before running this script:
# 1. Checkout your old clang master branch (say <somedir>/llvm/tools/clang). Call this SRCDIR.
# 2. Checkout the new monorepo (say <somedir>/src. Call this TGTDIR.
# 3. cd $SRCDIR
# 4. git log --pretty=oneline --format=%H > patchlist
# 5. Remove from patchlist the commit ids you do not want to cherry-pick.

# Note : Currently you need to run this script for each patch in the patchlist.
# We have followed a conservative approach and do not apply all patches at
# once. This is done so that the user has control over each patch. You can
# easily modify this script to iterate over all patches in the patchlist.

SRCDIR=<>
TGTDIR=<>
PROJ=clang
PATCHLIST=$SRCDIR/patchlist

if [[ ! -f $PATCHLIST ]]; then
  echo "error: $PATCHLIST not found."
  exit 1
fi

cd $SRCDIR
SHA=`tail -n1 $PATCHLIST`
git diff ${SHA}~1 ${SHA} > ${SHA}.patch

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
