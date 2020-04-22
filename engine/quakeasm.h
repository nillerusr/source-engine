//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
//
// quakeasm.h: general asm header file
//

// #define GLQUAKE	1

#if defined(_WIN32) && !defined(WINDED)

#if defined(_M_IX86)
#define __i386__	1
#endif

#endif

#ifdef __i386__
#define id386	1
#else
#define id386	0
#endif

// !!! must be kept the same as in d_iface.h !!!
#define TRANSPARENT_COLOR	255

// !!! if this is changes, it must be changed in bspfile.h too !!!
#define	MAX_MAP_LEAFS		8192
//#define	MAX_MAP_NODES		(65535-MAX_MAP_LEAFS)
#define	MAX_MAP_NODES		57343

.extern C(snd_scaletable)
.extern C(paintbuffer)
.extern C(snd_linear_count)
.extern C(snd_p)
.extern C(snd_vol)
.extern C(snd_out)
.extern C(BOPS_Error)

