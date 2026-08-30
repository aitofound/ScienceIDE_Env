#!/bin/sh

# Generate a COMMIT file containing a string which describes the
# commit status of the current source tree.

# The commit string has five parts. The first three come from git-describe
# And correspond to tag, commits since tag and commit hash.
# The fourth part is the subversion revision number and the fifth
# part indicates wether there are uncommitted changes

# If any of these fields is unavailable then the tag 'unknown' will be
# used. If none of the tags are known then the COMMIT file is left untouched

COMMIT_FILE_BASE=commit_info.h
OUTDIR=$1
COMMIT_FILE=$OUTDIR/$COMMIT_FILE_BASE

LF='
'

state='clean'
commit_string=""

git show-ref > /dev/null 2>&1
if [ $? -eq 0 ]; then
# in a git repo
  gitdescribe=$(git describe --long HEAD 2>/dev/null)

  if [ $? -ne 0 ]; then
    always=$(git describe --always --long HEAD 2>/dev/null)
    gitdescribe=unknown-unknown-g$always
  fi

  git update-index -q --refresh
  test -z "$(git diff-index --name-only HEAD --)" || state='dirty'

  commit_string=$gitdescribe-$state
  commit_date=$(git log --pretty=format:%cd -1 HEAD)
else
# not in a git repo
  [[ $COMMIT_FILE_BASE -nt $COMMIT_FILE ]] && cp $COMMIT_FILE_BASE $COMMIT_FILE
  grep "SDF_COMMIT_ID" $COMMIT_FILE > /dev/null 2>&1
  [ $? -eq 0 ] && exit
  commit_string=unknown-unknown-unknown-unknown
  commit_date=unknown
fi

[ -z $commit_string ] && exit

grep "$commit_string" $COMMIT_FILE > /dev/null 2>&1
if [ $? -eq 0 ]; then
  exit
else
  echo "#define SDF_COMMIT_ID \"$commit_string\"" > $COMMIT_FILE
  echo "#define SDF_COMMIT_DATE \"$commit_date\"" >> $COMMIT_FILE
  exit
fi
