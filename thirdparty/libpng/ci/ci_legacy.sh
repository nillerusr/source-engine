#!/usr/bin/env bash
set -e

# ci_legacy.sh
# Continuously integrate libpng using the legacy makefiles.
#
# Copyright (c) 2019-2020 Cosmin Truta.
#
# This software is released under the libpng license.
# For conditions of distribution and use, see the disclaimer
# and license in png.h.

readonly CI_SCRIPTNAME="$(basename "$0")"
readonly CI_SCRIPTDIR="$(cd "$(dirname "$0")" && pwd)"
readonly CI_SRCDIR="$(dirname "$CI_SCRIPTDIR")"
readonly CI_BUILDDIR="$CI_SRCDIR"

function ci_info {
    printf >&2 "%s: %s\\n" "$CI_SCRIPTNAME" "$*"
}

function ci_err {
    printf >&2 "%s: error: %s\\n" "$CI_SCRIPTNAME" "$*"
    exit 2
}

function ci_spawn {
    printf >&2 "%s: executing:" "$CI_SCRIPTNAME"
    printf >&2 " %q" "$@"
    printf >&2 "\\n"
    "$@"
}

function ci_init_legacy {
    # Initialize the CI_ variables with default values, where applicable.
    CI_MAKE="${CI_MAKE:-make}"
    [[ $(uname -s || echo unknown) == Darwin ]] && CI_CC="${CI_CC:-clang}"
    [[ $CI_CC == *clang* ]] &&
        CI_LEGACY_MAKEFILES="${CI_LEGACY_MAKEFILES:-scripts/makefile.clang}"
    CI_LEGACY_MAKEFILES="${CI_LEGACY_MAKEFILES:-scripts/makefile.gcc}"
    CI_LD="${CI_LD:-$CI_CC}"
    CI_LIBS="${CI_LIBS:--lz -lm}"
    # Print the CI_ variables.
    ci_info "source directory: $CI_SRCDIR"
    ci_info "build directory: $CI_BUILDDIR"
    ci_info "environment option: \$CI_LEGACY_MAKEFILES='$CI_LEGACY_MAKEFILES'"
    ci_info "environment option: \$CI_MAKE='$CI_MAKE'"
    ci_info "environment option: \$CI_MAKE_FLAGS='$CI_MAKE_FLAGS'"
    ci_info "environment option: \$CI_MAKE_VARS='$CI_MAKE_VARS'"
    ci_info "environment option: \$CI_CC='$CI_CC'"
    ci_info "environment option: \$CI_CC_FLAGS='$CI_CC_FLAGS'"
    ci_info "environment option: \$CI_CPP='$CI_CPP'"
    ci_info "environment option: \$CI_CPP_FLAGS='$CI_CPP_FLAGS'"
    ci_info "environment option: \$CI_LD='$CI_LD'"
    ci_info "environment option: \$CI_LD_FLAGS='$CI_LD_FLAGS'"
    ci_info "environment option: \$CI_LIBS='$CI_LIBS'"
    ci_info "environment option: \$CI_SANITIZERS='$CI_SANITIZERS'"
    ci_info "environment option: \$CI_NO_TEST='$CI_NO_TEST'"
    ci_info "environment option: \$CI_NO_CLEAN='$CI_NO_CLEAN'"
}

function ci_build_legacy {
    # Initialize ALL_CC_FLAGS and ALL_LD_FLAGS as strings.
    local ALL_CC_FLAGS="$CI_CC_FLAGS"
    local ALL_LD_FLAGS="$CI_LD_FLAGS"
    [[ $CI_SANITIZERS ]] && {
        ALL_CC_FLAGS="-fsanitize=$CI_SANITIZERS -O2 $ALL_CC_FLAGS"
        ALL_LD_FLAGS="-fsanitize=$CI_SANITIZERS $ALL_LD_FLAGS"
    }
    # Initialize ALL_MAKE_ARGS as an array;
    # expand CI_MAKE_FLAGS at the beginning and CI_MAKE_VARS at the end.
    local -a ALL_MAKE_ARGS=()
    ALL_MAKE_ARGS+=($CI_MAKE_FLAGS)
    [[ $CI_CC ]] && ALL_MAKE_ARGS+=("CC=$CI_CC")
    [[ $ALL_CC_FLAGS ]] && ALL_MAKE_ARGS+=("CFLAGS=$ALL_CC_FLAGS")
    [[ $CI_CPP ]] && ALL_MAKE_ARGS+=("CPP=$CI_CPP")
    [[ $CI_CPP_FLAGS ]] && ALL_MAKE_ARGS+=("CPPFLAGS=$CI_CPP_FLAGS")
    [[ $CI_LD ]] && ALL_MAKE_ARGS+=("LD=$CI_LD")
    [[ $ALL_LD_FLAGS ]] && ALL_MAKE_ARGS+=("LDFLAGS=$ALL_LD_FLAGS")
    ALL_MAKE_ARGS+=("LIBS=$CI_LIBS")
    ALL_MAKE_ARGS+=($CI_MAKE_VARS)
    # Build!
    ci_spawn cd "$CI_SRCDIR"
    local MY_MAKEFILE
    for MY_MAKEFILE in $CI_LEGACY_MAKEFILES
    do
        ci_info "using makefile: $MY_MAKEFILE"
        ci_spawn "$CI_MAKE" "${ALL_MAKE_ARGS[@]}" -f $MY_MAKEFILE
        [[ $CI_NO_TEST ]] || ci_spawn "$CI_MAKE" "${ALL_MAKE_ARGS[@]}" -f $MY_MAKEFILE test
        [[ $CI_NO_CLEAN ]] || ci_spawn "$CI_MAKE" "${ALL_MAKE_ARGS[@]}" -f $MY_MAKEFILE clean
    done
    ci_info "success!"
}

ci_init_legacy
[[ ! $* ]] || {
    ci_info "note: this program accepts environment options only"
    ci_err "unexpected command arguments: '$*'"
}
ci_build_legacy
