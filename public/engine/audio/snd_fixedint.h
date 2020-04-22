//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef SND_FIXEDINT_H
#define SND_FIXEDINT_H

#if defined( _WIN32 )
#pragma once
#endif

// fixed point stuff for real-time resampling
#define FIX_BITS			28
#define FIX_SCALE			(1 << FIX_BITS)
#define FIX_MASK			((1 << FIX_BITS)-1)
#define FIX_FLOAT(a)		((int)((a) * FIX_SCALE))
#define FIX(a)				(((int)(a)) << FIX_BITS)
#define FIX_INTPART(a)		(((int)(a)) >> FIX_BITS)
#define FIX_FRACTION(a,b)	(FIX(a)/(b))
#define FIX_FRACPART(a)		((a) & FIX_MASK)
#define FIX_TODOUBLE(a)		((double)(a) / (double)FIX_SCALE)

typedef unsigned int fixedint;

#define FIX_BITS14			14
#define FIX_SCALE14			(1 << FIX_BITS14)
#define FIX_MASK14			((1 << FIX_BITS14)-1)
#define FIX_FLOAT14(a)		((int)((a) * FIX_SCALE14))
#define FIX14(a)			(((int)(a)) << FIX_BITS14)
#define FIX_INTPART14(a)	(((int)(a)) >> FIX_BITS14)
#define FIX_FRACTION14(a,b)	(FIX14(a)/(b))
#define FIX_FRACPART14(a)	((a) & FIX_MASK14)
#define FIX_14TODOUBLE(a)	((double)(a) / (double)FIX_SCALE14)

#define FIX_28TO14(a)		( (int)( ((unsigned int)(a)) >> (FIX_BITS - 14) ) )

#endif // SND_FIXEDINT_H
