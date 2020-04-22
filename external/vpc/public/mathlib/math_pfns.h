//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef _MATH_PFNS_H_
#define _MATH_PFNS_H_

#include <limits>

#if defined( _X360 )
#include <xboxmath.h>
#elif defined(_PS3)

#ifndef SPU
#include <ppu_asm_intrinsics.h>
#endif

// Note that similar defines exist in ssemath.h
// Maybe we should consolidate in one place for all platforms.

#define _VEC_0x7ff		(vec_int4){0x7ff,0x7ff,0x7ff,0x7ff}
#define _VEC_0x3ff		(vec_int4){0x3ff,0x3ff,0x3ff,0x3ff}
#define _VEC_22L		(vector unsigned int){22,22,22,22}
#define _VEC_11L		(vector unsigned int){11,11,11,11}
#define _VEC_0L			(vector unsigned int){0,0,0,0}
#define _VEC_255F		(vector float){255.0f,255.0f,255.0f,255.0f}
#define _VEC_NEGONEF	(vector float){-1.0f,-1.0f,-1.0f,-1.0f}
#define _VEC_ONEF		(vector float){1.0f,1.0f,1.0f,1.0f}
#define _VEC_ZEROF		(vector float){0.0f,0.0f,0.0f,0.0f}
#define _VEC_ZEROxyzONEwF (vector float){0.0f,0.0f,0.0f,1.0f}
#define _VEC_HALFF		(vector float){0.5f,0.5f,0.5f,0.5f}
#define _VEC_HALFxyzZEROwF	(vector float){0.5f,0.5f,0.5f,0.0f}
#define _VEC_PERMUTE_XYZ0W1   (vector unsigned char){0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x1c,0x1d,0x1e,0x1f}

#define _VEC_IEEEHACK (vector float){(float)(1 << 23),(float)(1 << 23),(float)(1 << 23),(float)(1 << 23)}
#define _VEC_PERMUTE_FASTFTOC (vector unsigned char){0,0,0,0,0,0,0,0,0,0,0,0,0x03,0x07,0x0b,0x0f}

// AngleQuaternion
#define _VEC_PERMUTE_AQsxsxcxcx (vector unsigned char) {0x00,0x01,0x02,0x03,0x00,0x01,0x02,0x03,0x10,0x11,0x12,0x13,0x10,0x11,0x12,0x13}	
#define _VEC_PERMUTE_AQczszszcz (vector unsigned char) {0x18,0x19,0x1a,0x1b,0x08,0x09,0x0a,0x0b,0x08,0x09,0x0a,0x0b,0x18,0x19,0x1a,0x1b}	
#define _VEC_PERMUTE_AQcxcxsxsx (vector unsigned char) {0x10,0x11,0x12,0x13,0x10,0x11,0x12,0x13,0x00,0x01,0x02,0x03,0x00,0x01,0x02,0x03}	
#define _VEC_PERMUTE_AQszczczsz (vector unsigned char) {0x08,0x09,0x0a,0x0b,0x18,0x19,0x1a,0x1b,0x18,0x19,0x1a,0x1b,0x08,0x09,0x0a,0x0b}	
#define _VEC_PERMUTE_ANGLEQUAT  (vector unsigned char) {0x10,0x11,0x12,0x13,0x04,0x05,0x06,0x07,0x18,0x19,0x1a,0x1b,0x0c,0x0d,0x0e,0x0f}	

#define _VEC_EPSILONF		(__vector float)			{FLT_EPSILON,FLT_EPSILON,FLT_EPSILON,FLT_EPSILON}

#endif

#if !(defined( PLATFORM_PPC ) || defined(SPU))
// If we are not PPC based or SPU based, then assumes it is SSE2. We should make this code cleaner.

#include <xmmintrin.h>

// These globals are initialized by mathlib and redirected based on available fpu features

// The following are not declared as macros because they are often used in limiting situations,
// and sometimes the compiler simply refuses to inline them for some reason
FORCEINLINE float FastSqrt( float x )
{
	__m128 root = _mm_sqrt_ss( _mm_load_ss( &x ) );
	return *( reinterpret_cast<float *>( &root ) );
}

FORCEINLINE float FastRSqrtFast( float x )
{
	// use intrinsics
	__m128 rroot = _mm_rsqrt_ss( _mm_load_ss( &x ) );
	return *( reinterpret_cast<float *>( &rroot ) );
}
// Single iteration NewtonRaphson reciprocal square root:
// 0.5 * rsqrtps * (3 - x * rsqrtps(x) * rsqrtps(x)) 	
// Very low error, and fine to use in place of 1.f / sqrtf(x).	
FORCEINLINE float FastRSqrt( float x )
{
	float rroot = FastRSqrtFast( x );
	return (0.5f * rroot) * (3.f - (x * rroot) * rroot);
}

void FastSinCos( float x, float* s, float* c );  // any x
float FastCos( float x );



inline float FastRecip(float x) {return 1.0f / x;}
// Simple SSE rsqrt.  Usually accurate to around 6 (relative) decimal places 
// or so, so ok for closed transforms.  (ie, computing lighting normals)
inline float FastSqrtEst(float x) { return FastRSqrtFast(x) * x; }


#else // !defined( PLATFORM_PPC ) && !defined(_SPU)

#ifndef SPU
// We may not need this for SPU, so let's not bother for now

FORCEINLINE float _VMX_Sqrt( float x )
{
	return __fsqrts( x );
}

FORCEINLINE double _VMX_RSqrt( double x )
{
	double rroot = __frsqrte( x );

	// Single iteration NewtonRaphson on reciprocal square root estimate
	return (0.5f * rroot) * (3.0f - (x * rroot) * rroot);
}

FORCEINLINE double _VMX_RSqrtFast( double x )
{
	return __frsqrte( x );
}

#ifdef _X360
FORCEINLINE void _VMX_SinCos( float a, float *pS, float *pC )
{
	XMScalarSinCos( pS, pC, a );
}

FORCEINLINE float _VMX_Cos( float a )
{
	return XMScalarCos( a );
}
#endif

// the 360 has fixed hw and calls directly
#define FastSqrt(x)			_VMX_Sqrt(x)
#define	FastRSqrt(x)		_VMX_RSqrt(x)
#define FastRSqrtFast(x)	_VMX_RSqrtFast(x)
#define FastSinCos(x,s,c)	_VMX_SinCos(x,s,c)
#define FastCos(x)			_VMX_Cos(x)

inline double FastRecip(double x) {return __fres(x);}
inline double FastSqrtEst(double x) { return __frsqrte(x) * x; }

#endif // !defined( PLATFORM_PPC ) && !defined(_SPU)

// if x is infinite, return FLT_MAX
inline float FastClampInfinity( float x )
{
#ifdef PLATFORM_PPC
	return fsel( std::numeric_limits<float>::infinity() - x, x, FLT_MAX );
#else
	return ( x > FLT_MAX ? FLT_MAX : x );
#endif
}

#if defined (_PS3) && !defined(SPU)

// extern float cosvf(float);      /* single precision cosine      */
// extern float sinvf(float);      /* single precision sine        */
// TODO: need a faster single precision equivalent
#define cosvf cosf
#define sinvf sinf

inline int _rotl( int x, int c )
{
	return __rlwimi(x,x,c,0,31);
}

inline int64 _rotl64( int64 x, int c )
{
	return __rldicl( x, c, 0 );
}

//-----------------------------------------------------------------
// Vector Unions
//-----------------------------------------------------------------

//-----------------------------------------------------------------
// Floats
//-----------------------------------------------------------------
typedef union
{
	vector float vf;
	float f[4];
} vector_float_union;

//-----------------------------------------------------------------
// Ints
//-----------------------------------------------------------------
typedef union
{
	vector int vi;
	int i[4];
} vector_int4_union;

typedef union
{
	vector unsigned int vui;
	unsigned int ui[4];
} vector_uint4_union;

//-----------------------------------------------------------------
// Shorts
//-----------------------------------------------------------------
typedef union
{
	vector signed short vs;
	signed short s[8];
} vector_short8_union;

typedef union
{
	vector unsigned short vus;
	unsigned short us[8];
} vector_ushort8_union;

//-----------------------------------------------------------------
// Chars
//-----------------------------------------------------------------
typedef union
{
	vector signed char vc;
	signed char c[16];
} vector_char16_union;

typedef union
{
	vector unsigned char vuc;
	unsigned char uc[16];
} vector_uchar16_union;

/*
FORCEINLINE float _VMX_Sqrt( float x )
{
	vector_float_union vIn, vOut;

	vIn.f[0] = x;

	vOut.vf = sqrtf4(vIn.vf);
	
	return vOut.f[0];
}

FORCEINLINE float _VMX_RSqrt( float x )
{
	vector_float_union vIn, vOut;

	vIn.f[0] = x;

	vOut.vf = rsqrtf4(vIn.vf);

	return vOut.f[0];
}

FORCEINLINE float _VMX_RSqrtFast( float x )
{
	vector_float_union vIn, vOut;

	vIn.f[0] = x;

	vOut.vf = rsqrtf4fast(vIn.vf);

	return vOut.f[0];
}
*/

FORCEINLINE void _VMX_SinCos( float a, float *pS, float *pC )
{
	*pS=sinvf(a);
	*pC=cosvf(a);
}

FORCEINLINE float _VMX_Cos( float a )
{
	return cosvf(a);
}


// the 360 has fixed hw and calls directly
/*
#define FastSqrt(x)			_VMX_Sqrt(x)
#define	FastRSqrt(x)		_VMX_RSqrt(x)
#define FastRSqrtFast(x)	_VMX_RSqrtFast(x)
#define FastSinCos(x,s,c)	_VMX_SinCos(x,s,c)
#define FastCos(x)			_VMX_Cos(x)
*/
#endif	// _PS3
#endif	// #ifndef SPU

#endif // _MATH_PFNS_H_
