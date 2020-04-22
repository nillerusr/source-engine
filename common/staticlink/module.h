//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: static link module stitching
//
//=============================================================================//

// cannot be part of precompiled header chain
#if defined(_WIN32) && defined(_STATIC_LINKED)

// subsystems define _MODULE to create a unique symbol that can be referenced to force linkage
// subsystems don't define _MODULE for "pure" stateless shared modules which prevents the module from having a "state" symbol
// and causing linkage collision errors
#if defined(_MODULE)
DECLARE_MODULE(_MODULE)
#endif

#endif