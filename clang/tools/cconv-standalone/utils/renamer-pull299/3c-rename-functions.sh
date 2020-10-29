# Mike says be liberal with commits.
function commit {
    local msg="$1"; shift
    # https://en.wikipedia.org/wiki/Leaning_toothpick_syndrome
    for arg; do msg+=" '${arg//\'/\'\\\'\'}'"; done
    git commit -a -m "$msg"
}

function safe_git_mv {
    # If the destination is an existing directory (which can easily happen if
    # .orig files were left in the working tree from previous "git mergetool"
    # runs), then "git mv" would move the source into it instead of onto it, and
    # we'd silently get a wrong result.
    ! [ -e "$2" ] || {
        echo >&2 "Error: Move destination $2 exists.  Please correct and start over."
        false
    }
    git mv "$1" "$2"
}
function commit_git_mv {
    safe_git_mv "$1" "$2"
    commit "git mv" "$@"
}

function repl_basename_pfx {
    pfx="$1"; rpl="$2"; path_pat="$3"
    # https://stackoverflow.com/a/54162211
    #
    # "tac" is the easiest way to avoid shallower renames breaking deeper ones
    # without us having to iterate.
    for f in $(git ls-tree --name-only -rt HEAD | grep -E "$path_pat" | tac | grep -E "/$pfx[^/]*\$"); do
        # This costs a subprocess but is much easier than trying to do it in bash...
        f2="$(sed -re "s,/$pfx([^/]*)\$,/$rpl\1," <<<"$f")"
        safe_git_mv "$f" "$f2"
    done
    commit "repl_basename_pfx" "$@"
}

function repl {
    pat="$1"; repl="$2"; path_pat="$3"
    # Limiting to files with matches is a minor optimization.
    # We could consider batching multiple "repl" calls with the same $path_pat
    # as a further optimization.
    sed -ri -e "s,$pat,$repl,g" $(git grep -E --files-with-matches "$pat" | grep -E "$path_pat")
    commit "repl" "$@"
}
# This is a lot of variants but actually seems to be easier to read than
# flags to repl.
function repl_word {
    # Avoid generating '' in the commit message if no third argument was passed.
    repl "\<$1\>" "$2" ${3+"$3"}
}
function repl_cpp {
    repl "$1" "$2" '\.(h|cpp)$'
}
function repl_cpp_word {
    repl_cpp "\<$1\>" "$2"
}
