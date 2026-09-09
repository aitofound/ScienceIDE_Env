#!/bin/bash

# Trim leading and trailing whitespace from this given string. For example:
# `trim "  a b  "` yields `"a b"`. Adapted from
# https://stackoverflow.com/a/3352015.
trim() {
    local var="$*"
    # leading whitespace
    var="${var#"${var%%[![:space:]]*}"}"
    # trailing whitespace
    var="${var%"${var##*[![:space:]]}"}"
    printf '%s' "$var"
}

# Define text formatting commands.
# - textbf: Use bold-face.
# - textnm: Reset to normal.
# - startul: Start underlined text.
# - endul: End underlined text.
textbf=$(tput bold)
textnm=$(tput sgr0)
startul=$(tput smul)
endul=$(tput rmul)

# Define text colors.
c_green="\033[0;32m"
c_yellow="\033[0;33m"
c_red="\033[0;31m"
c_none="\033[0m"

# Print a character enough times to fill the screen width.
# - first argument: the character to print
# - second argument: the number of columns to pad on both sides (default=0)
print_break() {
    local c=${1}
    local pad=${2:-0}
    if [ $pad == 0 ]; then
        let "length=$(tput cols)"
        printf "$c%.0s" $(seq 1 $length)
    else
        let "length=$(tput cols)-(2*$pad)"
        printf " %.0s" $(seq 1 $pad)
        printf "$c%.0s" $(seq 1 $length)
        printf " %.0s" $(seq 1 $pad)
    fi
    echo 
}

# Print the given text in the center of the screen. Adapted from
# https://superuser.com/a/1677589
center_text() {
    echo ${@} | sed  -e :a -e "s/^.\{1,$(tput cols)\}$/ & /;ta" | tr -d '\n' | head -c $(tput cols)
}

# Print a section header:
# 
# ```
# ======================
#      <some text>      
# ======================
# ```
print_header() {
    print_break "="
    if [ -n "${@}" ]; then
        center_text ${@}
        print_break "="
    fi
}

