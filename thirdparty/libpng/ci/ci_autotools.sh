#!/usr/bin/env bash
set -e

# ci_autotools.sh
# Continuously integrate libpng using the GNU Autotools.
#
# Copyright (c) 2019-2020 Cosmin Truta.
#
# This software is released under the libpng license.
# For conditions of distribution and use, see the disclaimer
# and license in png.h.

readonly CI_SCRIPTNAME="$(basename "$0")"
readonly CI_SCRIPTDIR="$(cd "$(dirname "$0")" && pwd)"
readonly CI_SRCDIR="$(dirname "$CI_SCRIPTDIR")"
readonly CI_BUILDDIR="$CI_SRCDIR/out/autotools.build"
readonly CI_INSTALLDIR="$CI_SRCDIR/out/autotools.install"

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

function ci_init_autotools {
    # Initialize the CI_ variables with default values, where applicable.
    CI_MAKE="${CI_MAKE:-make}"
    [[ $(uname -s || echo unknown) == Darwin ]] && CI_CC="${CI_CC:-clang}"
    # Print the CI_ variables.
    ci_info "source directory: $CI_SRCDIR"
    ci_info "build directory: $CI_BUILDDIR"
    ci_info "install directory: $CI_INSTALLDIR"
    ci_info "environment option: \$CI_CONFIGURE_FLAGS='$CI_CONFIGURE_FLAGS'"
    ci_info "environment option: \$CI_MAKE='$CI_MAKE'"
    ci_info "environment option: \$CI_MAKE_FLAGS='$CI_MAKE_FLAGS'"
    ci_info "environment option: \$CI_CC='$CI_CC'"
    ci_info "environment option: \$CI_CC_FLAGS='$CI_CC_FLAGS'"
    ci_info "environment option: \$CI_SANITIZERS='$CI_SANITIZERS'"
    ci_info "environment option: \$CI_NO_TEST='$CI_NO_TEST'"
    ci_info "environment option: \$CI_NO_INSTALL='$CI_NO_INSTALL'"
    ci_info "environment option: \$CI_NO_CLEAN='$CI_NO_CLEAN'"
    # Avoid using the CI_ variables that cannot be customized reliably.
    [[ ! $CI_CONFIGURE_VARS ]] || ci_err "unexpected: \$CI_CONFIGURE_VARS='$CI_CONFIGURE_VARS'"
    [[ ! $CI_MAKE_VARS ]] || ci_err "unexpected: \$CI_MAKE_VARS='$CI_MAKE_VARS'"
}

function ci_build_autotools {
    # Export the configure build environment.
    [[ $CI_CC ]] && ci_spawn export CC="$CI_CC"
    [[ $CI_CC_FLAGS ]] && ci_spawn export CFLAGS="$CI_CC_FLAGS"
    [[ $CI_SANITIZERS ]] && ci_spawn export CFLAGS="-fsanitize=$CI_SANITIZERS -O2 $CFLAGS"
    # Build and install.
    ci_spawn rm -fr "$CI_BUILDDIR" "$CI_INSTALLDIR"
    ci_spawn mkdir -p "$CI_BUILDDIR"
    ci_spawn cd "$CI_BUILDDIR"
    ci_spawn "$CI_SRCDIR/configure" --prefix="$CI_INSTALLDIR" $CI_CONFIGURE_FLAGS
    ci_spawn "$CI_MAKE" $CI_MAKE_FLAGS
    [[ $CI_NO_TEST ]] || ci_spawn "$CI_MAKE" $CI_MAKE_FLAGS test
    [[ $CI_NO_INSTALL ]] || ci_spawn "$CI_MAKE" $CI_MAKE_FLAGS install
    [[ $CI_NO_CLEAN ]] || ci_spawn "$CI_MAKE" $CI_MAKE_FLAGS clean
    [[ $CI_NO_CLEAN ]] || ci_spawn "$CI_MAKE" $CI_MAKE_FLAGS distclean
    ci_info "success!"
}

ci_init_autotools
[[ ! $* ]] || {
    ci_info "note: this program accepts environment options only"
    ci_err "unexpected command arguments: '$*'"
}
ci_build_autotools
