#!/usr/bin/env bash
set -e

# ci_cmake.sh
# Continuously integrate libpng using CMake.
#
# Copyright (c) 2019-2020 Cosmin Truta.
#
# This software is released under the libpng license.
# For conditions of distribution and use, see the disclaimer
# and license in png.h.

readonly CI_SCRIPTNAME="$(basename "$0")"
readonly CI_SCRIPTDIR="$(cd "$(dirname "$0")" && pwd)"
readonly CI_SRCDIR="$(dirname "$CI_SCRIPTDIR")"
readonly CI_BUILDDIR="$CI_SRCDIR/out/cmake.build"
readonly CI_INSTALLDIR="$CI_SRCDIR/out/cmake.install"

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

function ci_init_cmake {
    # Initialize the CI_ variables with default values, where applicable.
    CI_CMAKE="${CI_CMAKE:-cmake}"
    CI_CTEST="${CI_CTEST:-ctest}"
    [[ $(uname -s || echo unknown) == Darwin ]] && CI_CC="${CI_CC:-clang}"
    CI_CMAKE_BUILD_TYPE="${CI_CMAKE_BUILD_TYPE:-Release}"
    # Print the CI_ variables.
    ci_info "source directory: $CI_SRCDIR"
    ci_info "build directory: $CI_BUILDDIR"
    ci_info "install directory: $CI_INSTALLDIR"
    ci_info "environment option: \$CI_CMAKE='$CI_CMAKE'"
    ci_info "environment option: \$CI_CMAKE_GENERATOR='$CI_CMAKE_GENERATOR'"
    ci_info "environment option: \$CI_CMAKE_GENERATOR_PLATFORM='$CI_CMAKE_GENERATOR_PLATFORM'"
    ci_info "environment option: \$CI_CMAKE_BUILD_TYPE='$CI_CMAKE_BUILD_TYPE'"
    ci_info "environment option: \$CI_CMAKE_BUILD_FLAGS='$CI_CMAKE_BUILD_FLAGS'"
    ci_info "environment option: \$CI_CMAKE_VARS='$CI_CMAKE_VARS'"
    ci_info "environment option: \$CI_CTEST='$CI_CTEST'"
    ci_info "environment option: \$CI_CTEST_FLAGS='$CI_CTEST_FLAGS'"
    ci_info "environment option: \$CI_CC='$CI_CC'"
    ci_info "environment option: \$CI_CC_FLAGS='$CI_CC_FLAGS'"
    ci_info "environment option: \$CI_SANITIZERS='$CI_SANITIZERS'"
    ci_info "environment option: \$CI_NO_TEST='$CI_NO_TEST'"
    ci_info "environment option: \$CI_NO_INSTALL='$CI_NO_INSTALL'"
    ci_info "environment option: \$CI_NO_CLEAN='$CI_NO_CLEAN'"
    # Print the CMake/CTest program versions.
    ci_spawn "$(command -v "$CI_CMAKE")" --version
    ci_spawn "$(command -v "$CI_CTEST")" --version
}

function ci_build_cmake {
    # Initialize ALL_CC_FLAGS as a string.
    local ALL_CC_FLAGS="$CI_CC_FLAGS"
    [[ $CI_SANITIZERS ]] && ALL_CC_FLAGS="-fsanitize=$CI_SANITIZERS $ALL_CC_FLAGS"
    # Initialize ALL_CMAKE_VARS, ALL_CMAKE_BUILD_FLAGS and ALL_CTEST_FLAGS as arrays.
    local -a ALL_CMAKE_VARS=()
    [[ $CI_CC ]] && ALL_CMAKE_VARS+=(-DCMAKE_C_COMPILER="$CI_CC")
    [[ $ALL_CC_FLAGS ]] && ALL_CMAKE_VARS+=(-DCMAKE_C_FLAGS="$ALL_CC_FLAGS")
    ALL_CMAKE_VARS+=(-DCMAKE_BUILD_TYPE="$CI_CMAKE_BUILD_TYPE")
    ALL_CMAKE_VARS+=(-DCMAKE_INSTALL_PREFIX="$CI_INSTALLDIR")
    ALL_CMAKE_VARS+=(-DCMAKE_VERBOSE_MAKEFILE=ON)
    [[ $CI_NO_TEST ]] && ALL_CMAKE_VARS+=(-DPNG_TESTS=OFF)
    ALL_CMAKE_VARS+=($CI_CMAKE_VARS)
    local -a ALL_CMAKE_BUILD_FLAGS=($CI_CMAKE_BUILD_FLAGS)
    local -a ALL_CTEST_FLAGS=($CI_CTEST_FLAGS)
    # Export the CMake build environment.
    [[ $CI_CMAKE_GENERATOR ]] &&
        ci_spawn export CMAKE_GENERATOR="$CI_CMAKE_GENERATOR"
    [[ $CI_CMAKE_GENERATOR_PLATFORM ]] &&
        ci_spawn export CMAKE_GENERATOR_PLATFORM="$CI_CMAKE_GENERATOR_PLATFORM"
    # Build and install.
    ci_spawn "$CI_CMAKE" -E remove_directory "$CI_BUILDDIR"
    ci_spawn "$CI_CMAKE" -E remove_directory "$CI_INSTALLDIR"
    ci_spawn "$CI_CMAKE" -E make_directory "$CI_BUILDDIR"
    ci_spawn cd "$CI_BUILDDIR"
    ci_spawn "$CI_CMAKE" "${ALL_CMAKE_VARS[@]}" "$CI_SRCDIR"
    ci_spawn "$CI_CMAKE" --build . \
                         --config "$CI_CMAKE_BUILD_TYPE" \
                         "${ALL_CMAKE_BUILD_FLAGS[@]}"
    [[ $CI_NO_TEST ]] ||
        ci_spawn "$CI_CTEST" --build-config "$CI_CMAKE_BUILD_TYPE" \
                             "${ALL_CTEST_FLAGS[@]}"
    [[ $CI_NO_INSTALL ]] ||
        ci_spawn "$CI_CMAKE" --build . \
                             --config "$CI_CMAKE_BUILD_TYPE" \
                             --target install \
                             "${ALL_CMAKE_BUILD_FLAGS[@]}"
    [[ $CI_NO_CLEAN ]] ||
        ci_spawn "$CI_CMAKE" --build . \
                             --config "$CI_CMAKE_BUILD_TYPE" \
                             --target clean \
                             "${ALL_CMAKE_BUILD_FLAGS[@]}"
    ci_info "success!"
}

ci_init_cmake
[[ ! $* ]] || {
    ci_info "note: this program accepts environment options only"
    ci_err "unexpected command arguments: '$*'"
}
ci_build_cmake
