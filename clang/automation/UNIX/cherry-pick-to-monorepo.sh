#!/bin/bash

# Here's a brief overview of how this script works:
# You create a list of patches (SHAs) in a file and then run this script for
# each patch.  After successfully applying a patch this script removes the
# patch from the file.

# Here are detailed steps to cherry-pick your clang patches to the new
# checkedc-clang monorepo. The steps remain the same for cherry-picking llvm
# patches.

# 1. Let's say you have your clang patches at <somedir>/llvm/tools/clang. Call
# this SRCDIR.

# 2. Create a file at SRCDIR with the list of patches to be cherry-picked to
# the new monorepo:
#    git log --pretty=oneline --format=%H > SRCDIR/patchlist

# 3. Note: The above command will add all the SHAs in the clang repo to the
# patchlist. You need to remove from patchlist the SHAs you do not want to cherry-pick.

# 4. Now checkout the new checkedc-clang monorepo (at say
# <somedir>/checkedc-clang). Call this TGTDIR.

# 5. Checkout a new branch in the monorepo (say NEW_BRANCH).

# 6. Now run this script as follows:
#    ./cherry-pick-to-monorepo.sh <SRCDIR> <TGTDIR>

# 7. After successfully applying a patch this script will remove the patch from
# the patchlist. Repeat step 6 for each patch remaining in the patchlist.

# 8. After all the patches have been cherry-picked, push your branch to GitHub:
#    git push origin NEW_BRANCH

SRCDIR=$1
TGTDIR=$2
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
