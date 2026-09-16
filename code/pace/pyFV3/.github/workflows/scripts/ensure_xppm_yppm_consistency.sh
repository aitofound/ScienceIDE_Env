#!/bin/bash

# Call the automatic conversion
./.github/workflows/scripts/convert_xppm_yppm.sh

# Check for uncommitted changes in the working tree
if [ -n "$(git status --porcelain)" ]; then
    echo "It looks like x-direction and y-direction advection are out of sync. We found uncommitted"
    echo "changes in working tree after YPPM was generated from XPPM sources. Please ensure to make"
    echo "symmetrical changes to XPPM and YPPM codes."
    echo ""
    echo "If this is a false positive, please fix \".github/workflows/scripts/convert_xppm_yppm.sh\""
    echo "or re-evaluate the need for this part of the linting workflow."
    echo ""
    echo "git status"
    echo "----------"
    git status
    echo ""
    echo "git diff"
    echo "--------"
    git --no-pager diff
    echo ""

    exit 1
else
    echo "XPPM and YPPM are in sync."
fi
