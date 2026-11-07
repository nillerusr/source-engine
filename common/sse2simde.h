//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Provide the SSE intrinsics API on non-x86 / non-ARM targets
//          (e.g. RISC-V) via SIMDe (SIMD Everywhere, https://github.com/simd-everywhere/simde).
//
//          This is the counterpart of sse2neon.h for ARM: code written against
//          <xmmintrin.h>/<emmintrin.h> keeps working, and SIMDe maps it to the
//          native vector extension where it can (RVV on RISC-V) or to portable C.
//
//          Requires the SIMDe headers (Debian/Ubuntu: libsimde-dev, Fedora: simde-devel).
//
//===========================================================================//
#ifndef SSE2SIMDE_H
#define SSE2SIMDE_H

#ifndef SIMDE_ENABLE_NATIVE_ALIASES
#define SIMDE_ENABLE_NATIVE_ALIASES
#endif

#include <simde/x86/sse4.1.h>

#endif // SSE2SIMDE_H
