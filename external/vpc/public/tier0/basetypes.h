//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef BASETYPES_H
#define BASETYPES_H

#ifdef COMPILER_MSVC
#pragma once
#endif

// This is a trick to get the DLL extension off the -D option on the command line.
#define DLLExtTokenPaste(x) #x
#define DLLExtTokenPaste2(x) DLLExtTokenPaste(x)
#define DLL_EXT_STRING DLLExtTokenPaste2( _DLL_EXT )

//////////////////////////////////////////////////////////////////////////

#ifndef schema
#define schema namespace ValveSchemaMarker {}
#endif

#ifdef COMPILING_SCHEMA
#define UNSCHEMATIZED_METHOD( x )
#else
#define UNSCHEMATIZED_METHOD( x ) x
#endif

//////////////////////////////////////////////////////////////////////////

#include "tier0/platform.h"
#include "commonmacros.h"
#include "wchartypes.h"
#ifdef _PS3
#include <float.h>
#elif defined( PLATFORM_POSIX )
#include <math.h>
#endif

#include "tier0/valve_off.h"

// There's a different version of this file in the xbox codeline
// so the PC version built in the xbox branch includes things like 
// tickrate changes.
#include "xbox_codeline_defines.h"

#if defined(_PS3)
#include <ppu_intrinsics.h>
#include <sys/fs.h>
#define PATH_MAX CELL_FS_MAX_FS_PATH_LENGTH
#define _MAX_PATH PATH_MAX
#endif
// stdio.h
#ifndef NULL
#define NULL 0
#endif

#ifdef PLATFORM_POSIX
#include <stdint.h>

template<class T>
T abs( const T &a )
{
	if ( a < 0 )
		return -a;
	else
		return a;
}
#endif

#define ExecuteNTimes( nTimes, x )	\
	{								\
	static int __executeCount=0;\
	if ( __executeCount < nTimes )\
		{							\
			++__executeCount;		\
			x;						\
		}							\
	}


#define ExecuteOnce( x )			ExecuteNTimes( 1, x )



// Pad a number so it lies on an N byte boundary.
// So PAD_NUMBER(0,4) is 0 and PAD_NUMBER(1,4) is 4
#define PAD_NUMBER(number, boundary) \
	( ((number) + ((boundary)-1)) / (boundary) ) * (boundary)

// In case this ever changes
#ifndef M_PI
#define M_PI			3.14159265358979323846
#endif


// #define COMPILETIME_MAX and COMPILETIME_MIN for max/min in constant expressions
#define COMPILETIME_MIN( a, b ) ( ( ( a ) < ( b ) ) ? ( a ) : ( b ) )
#define COMPILETIME_MAX( a, b ) ( ( ( a ) > ( b ) ) ? ( a ) : ( b ) )
#ifndef MIN
#define MIN( a, b ) ( ( ( a ) < ( b ) ) ? ( a ) : ( b ) )
#endif

#ifndef MAX
#define MAX( a, b ) ( ( ( a ) > ( b ) ) ? ( a ) : ( b ) )
#endif




#ifdef __cplusplus

template< class T, class Y, class X >
inline T clamp( T const &val, Y const &minVal, X const &maxVal )
{
	if( val < minVal )
		return minVal;
	else if( val > maxVal )
		return maxVal;
	else
		return val;
}

#else

#define clamp(val, min, max) (((val) > (max)) ? (max) : (((val) < (min)) ? (min) : (val)))

#endif

#ifndef FALSE
#define FALSE 0
#define TRUE (!FALSE)
#endif

//-----------------------------------------------------------------------------
// fsel
//-----------------------------------------------------------------------------
#if !defined(_PS3) && !defined(_X360)

#define fsel(c,x,y) ( (c) >= 0 ? (x) : (y) )

// integer conditional move
// if a >= 0, return x, else y
#define isel(a,x,y) ( ((a) >= 0) ? (x) : (y) )

// if x = y, return a, else b
#define ieqsel(x,y,a,b) (( (x) == (y) ) ? (a) : (b))

// if the nth bit of a is set (counting with 0 = LSB),
// return x, else y
// this is fast if nbit is a compile-time immediate 
#define ibitsel(a, nbit, x, y) ( ( ((a) & (1 << (nbit))) != 0 ) ? (x) : (y) )


FORCEINLINE double fpmin( double a, double b )
{
	return a > b  ? b : a;
}

FORCEINLINE double fpmax( double a, double b )
{
	return a >= b ? a : b;
}

// clamp x to lie inside [a,b]. Assumes b>a
FORCEINLINE float fclamp( float x, float a, float b )
{
	return fpmin( fpmax( x, a ), b );
}
// clamp x to lie inside [a,b]. Assumes b>a
FORCEINLINE double fclamp( double x, double a, double b )
{
	return fpmin( fpmax( x, a ), b );
}

// At some point, we will need a unified API.
#define imin( x, y ) ( (x) < (y) ? (x) : (y) )
#define imax( x, y ) ( (x) > (y) ? (x) : (y) )
#define iclamp clamp

#else

// __fsel(double fComparand, double fValGE, double fLT) == fComparand >= 0 ? fValGE : fLT
// this is much faster than if ( aFloat > 0 ) { x = .. }
// the XDK defines two intrinsics, one for floats and one for doubles -- it's the same
// opcode, but the __fself version tells the compiler not to do a wasteful unnecessary
// rounding op after each sel.
// #define fsel __fsel
#ifdef _X360
FORCEINLINE double fsel(double fComparand, double fValGE, double fLT) { return __fsel( fComparand, fValGE, fLT ); }
FORCEINLINE float fsel(float fComparand, float fValGE, float fLT) { return __fself( fComparand, fValGE, fLT ); }
#else
FORCEINLINE double fsel(double fComparand, double fValGE, double fLT) { return __fsel( fComparand, fValGE, fLT ); }
FORCEINLINE float fsel(float fComparand, float fValGE, float fLT) { return __fsels( fComparand, fValGE, fLT ); }
#endif

#if !defined(_X360)
FORCEINLINE float fpmin( float a, float b )
{
	return fsel( a-b, b,a);
}
FORCEINLINE double fpmin( double a, double b )
{
	return fsel( a-b, b,a);
}

FORCEINLINE float fpmax( float a, float b )
{
	return fsel( a-b, a,b);
}
FORCEINLINE double fpmax( double a, double b )
{
	return fsel( a-b, a,b);
}

// any mixed calls should promote to double
FORCEINLINE double fpmax(float a, double b)
{
	return fpmax( (double) a, b );
}
// any mixed calls should promote to double
FORCEINLINE double fpmax(double a, float b)
{
	return fpmax( (double) a, (double) b );
}
#endif

// clamp x to lie inside [a,b]. Assumes b>a
FORCEINLINE float fclamp( float x, float a, float b )
{
	return fpmin( fpmax( x, a ), b );
}
// clamp x to lie inside [a,b]. Assumes b>a
FORCEINLINE double fclamp( double x, double a, double b )
{
	return fpmin( fpmax( x, a ), b );
}

// if a >= 0, return x, else y
FORCEINLINE int isel( int a, int x, int y )
{
	int mask = a >> 31; // arithmetic shift right, splat out the sign bit
	return x + ((y - x) & mask);
};

// if a >= 0, return x, else y
FORCEINLINE unsigned isel( int a, unsigned x, unsigned y )
{
	int mask = a >> 31; // arithmetic shift right, splat out the sign bit
	return x + ((y - x) & mask);
};

// ( x == y ) ? a : b
FORCEINLINE unsigned ieqsel( unsigned x, unsigned y, unsigned a, unsigned b )
{
	unsigned mask = (x == y) ? 0 : -1;
	return a + ((b - a) & mask);
};

// ( x == y ) ? a : b
FORCEINLINE int ieqsel( int x, int y, int a, int b )
{
	int mask = (x == y) ? 0 : -1;
	return a + ((b - a) & mask);
};

FORCEINLINE int imin( int x, int y )
{
	int nMaxSign = x - y;									// Positive if x greater than y
	int nMaxMask = nMaxSign >> 31;							// 0 if x greater than y, 0xffffffff if x smaller than y
	int nMaxSaturated = y + ( nMaxSign & nMaxMask );
	return nMaxSaturated;
}

FORCEINLINE int imax( int x, int y )
{
	int nMinSign = y - x;									// Positive if x smaller than y
	int nMinMask = nMinSign >> 31;							// 0 if x smaller than y, 0xffffffff if x greater than y
	int nMinSaturated = y - ( nMinSign & nMinMask);
	return nMinSaturated;
}

FORCEINLINE int iclamp( int x, int min, int max )
{
	int nResult = imin( x, max );
	return imax( nResult, min );
}

// if the nth bit of a is set (counting with 0 = LSB),
// return x, else y
// this is fast if nbit is a compile-time immediate 
#define ibitsel(a, nbit, x, y) ( (x) + (((y) - (x)) & (((a) & (1 << (nbit))) ? 0 : -1)) )

#endif

#if CROSS_PLATFORM_VERSION < 1

#ifndef DONT_DEFINE_BOOL // Needed for Cocoa stuff to compile.
typedef int BOOL;
#endif

typedef int qboolean;
//typedef uint32 ULONG;
typedef uint8 BYTE;
typedef uint8 byte;
typedef uint16 word;
#endif

#if defined( _WIN32 ) || defined( _PS3 )
typedef wchar_t ucs2; // under windows & PS3 wchar_t is ucs2
#else
typedef unsigned short ucs2;
#endif

enum ThreeState_t
{
	TRS_FALSE,
	TRS_TRUE,
	TRS_NONE,
};

typedef float vec_t;
#ifdef _WIN32
typedef __int32 vec_t_as_gpr; // a general purpose register type equal in size to a vec_t (in case we have to avoid the fpu for some reason)
#endif


template <typename T>
inline T AlignValue( T val, uintp alignment )
{
	return (T)( ( (uintp)val + alignment - 1 ) & ~( alignment - 1 ) );
}


// FIXME: this should move 
#ifndef __cplusplus
#define true TRUE
#define false FALSE
#endif

//-----------------------------------------------------------------------------
// look for NANs, infinities, and underflows. 
// This assumes the ANSI/IEEE 754-1985 standard
//-----------------------------------------------------------------------------

#ifdef __cplusplus

inline uint32 FloatBits( const vec_t &f )
{
	union Convertor_t
	{
		vec_t f;
		uint32 ul;
	}tmp;
	
	switch( false ) { case false: ; case (sizeof( tmp ) == 4 && sizeof( tmp.f ) == 4 && sizeof( tmp.ul ) == 4):; };
	tmp.f = f;
	return tmp.ul;
}

inline vec_t BitsToFloat( uint32 i )
{
	union Convertor_t
	{
		vec_t f;
		uint32 ul;
	}tmp;
	switch( false ) { case false: ; case (sizeof( tmp ) == 4 && sizeof( tmp.f ) == 4 && sizeof( tmp.ul ) == 4 ):; };
	tmp.ul = i;
	return tmp.f;
}

inline bool IsFinite( const vec_t &f )
{
#ifdef _GAMECONSOLE
	return f == f && fabs(f) <= FLT_MAX;
#else
	return ((FloatBits(f) & 0x7F800000) != 0x7F800000);
#endif
}

#if defined( WIN32 )

//#include <math.h>
// Just use prototype from math.h
#ifdef __cplusplus
extern "C" 
{
#endif
	double __cdecl fabs(double);
	//_CRT_JIT_INTRINSIC  _CRTIMP float  __cdecl fabsf( __in float  _X);
	float __cdecl fabsf( _In_ float );
#ifdef __cplusplus
}
#endif

// In win32 try to use the intrinsic fabs so the optimizer can do it's thing inline in the code
#pragma intrinsic( fabs )
// Also, alias float make positive to use fabs, too
// NOTE:  Is there a perf issue with double<->float conversion?
inline float FloatMakePositive( vec_t f )
{
	return fabsf( f );
}
#else
inline float FloatMakePositive( vec_t f )
{
	return fabsf(f); // was since 2002: BitsToFloat( FloatBits(f) & 0x7FFFFFFF ); fixed in 2010
}
#endif

inline float FloatNegate( vec_t f )
{
	return -f; //BitsToFloat( FloatBits(f) ^ 0x80000000 );
}


#define FLOAT32_NAN_BITS     (uint32)0x7FC00000	// not a number!
#define FLOAT32_NAN          BitsToFloat( FLOAT32_NAN_BITS )

#define VEC_T_NAN FLOAT32_NAN

#endif

inline float FloatMakeNegative( vec_t f )
{
	return -fabsf( f );// was since 2002: BitsToFloat( FloatBits(f) | 0x80000000 ); fixed in 2010
}


// FIXME: why are these here?  Hardly anyone actually needs them.
struct color24
{
	byte r, g, b;
};

typedef struct color32_s
{
	bool operator!=( const struct color32_s &other ) const;
	byte r, g, b, a; 

	// assign and copy by using the whole register rather
	// than byte-by-byte copy. (No, the compiler is not
	// smart enough to do this for you. /FAcs if you 
	// don't believe me.)
	inline unsigned *asInt(void) { return reinterpret_cast<unsigned*>(this); }
	inline const unsigned *asInt(void) const { return reinterpret_cast<const unsigned*>(this); } 
	// This thing is in a union elsewhere, and union members can't have assignment
	// operators, so you have to explicitly assign using this, or be slow. SUCK.
	inline void Copy(const color32_s &rhs)
	{
		*asInt() = *rhs.asInt();
	}

} color32;

inline bool color32::operator!=( const color32 &other ) const
{
	return r != other.r || g != other.g || b != other.b || a != other.a;
}

struct colorVec
{
	unsigned r, g, b, a;
};


#ifndef NOTE_UNUSED
#define NOTE_UNUSED(arg) ((void)arg)	// for pesky compiler / lint warnings
#endif
#ifdef __cplusplus

struct vrect_t
{
	int				x,y,width,height;
	vrect_t			*pnext;
};

#endif


//-----------------------------------------------------------------------------
// MaterialRect_t struct - used for DrawDebugText
//-----------------------------------------------------------------------------
struct Rect_t
{
	int x, y;
	int width, height;
};

struct Rect3D_t
{
	int x, y, z;
	int width, height, depth;

	FORCEINLINE Rect3D_t( int nX, int nY, int nZ, int nWidth, int nHeight, int nDepth )
	{
		x = nX;
		y = nY;
		z = nZ;
		width = nWidth;
		height = nHeight;
		depth = nDepth;
	}

	FORCEINLINE Rect3D_t( void )
	{
	}

};



//-----------------------------------------------------------------------------
// Interval, used by soundemittersystem + the game
//-----------------------------------------------------------------------------
struct interval_t
{
	float start;
	float range;
};


//-----------------------------------------------------------------------------
// Declares a type-safe handle type; you can't assign one handle to the next
//-----------------------------------------------------------------------------

// 32-bit pointer handles.

// Typesafe 8-bit and 16-bit handles.
template< class HandleType >
class CBaseIntHandle
{
public:

	inline bool			operator==( const CBaseIntHandle &other )	{ return m_Handle == other.m_Handle; }
	inline bool			operator!=( const CBaseIntHandle &other )	{ return m_Handle != other.m_Handle; }

	// Only the code that doles out these handles should use these functions.
	// Everyone else should treat them as a transparent type.
	inline HandleType	GetHandleValue()					{ return m_Handle; }
	inline void			SetHandleValue( HandleType val )	{ m_Handle = val; }

	typedef HandleType	HANDLE_TYPE;

protected:

	HandleType	m_Handle;
};

template< class DummyType >
class CIntHandle16 : public CBaseIntHandle< unsigned short >
{
public:
	inline			CIntHandle16() {}

	static inline	CIntHandle16<DummyType> MakeHandle( HANDLE_TYPE val )
	{
		return CIntHandle16<DummyType>( val );
	}

protected:
	inline			CIntHandle16( HANDLE_TYPE val )
	{
		m_Handle = val;
	}
};


template< class DummyType >
class CIntHandle32 : public CBaseIntHandle< uint32 >
{
public:
	inline			CIntHandle32() {}

	static inline	CIntHandle32<DummyType> MakeHandle( HANDLE_TYPE val )
	{
		return CIntHandle32<DummyType>( val );
	}

protected:
	inline			CIntHandle32( HANDLE_TYPE val )
	{
		m_Handle = val;
	}
};


// NOTE: This macro is the same as windows uses; so don't change the guts of it
#define DECLARE_HANDLE_16BIT(name)	typedef CIntHandle16< struct name##__handle * > name;
#define DECLARE_HANDLE_32BIT(name)	typedef CIntHandle32< struct name##__handle * > name;

#define DECLARE_POINTER_HANDLE(name) struct name##__ { int unused; }; typedef struct name##__ *name
#define FORWARD_DECLARE_HANDLE(name) typedef struct name##__ *name

#define DECLARE_DERIVED_POINTER_HANDLE( _name, _basehandle ) struct _name##__ : public _basehandle##__ {}; typedef struct _name##__ *_name
#define DECLARE_ALIASED_POINTER_HANDLE( _name, _alias ) typedef struct _alias##__ *name

// @TODO: Find a better home for this
#if !defined(_STATIC_LINKED) && !defined(PUBLISH_DLL_SUBSYSTEM)
// for platforms built with dynamic linking, the dll interface does not need spoofing
#define PUBLISH_DLL_SUBSYSTEM()
#endif

#define UID_PREFIX generated_id_
#define UID_CAT1(a,c) a ## c
#define UID_CAT2(a,c) UID_CAT1(a,c)
#define EXPAND_CONCAT(a,c) UID_CAT1(a,c)
#ifdef _MSC_VER
#define UNIQUE_ID UID_CAT2(UID_PREFIX,__COUNTER__)
#else
#define UNIQUE_ID UID_CAT2(UID_PREFIX,__LINE__)
#endif

#define _MKSTRING(arg) #arg
#define MKSTRING(arg) _MKSTRING(arg)


// this allows enumerations to be used as flags, and still remain type-safe!
#define DEFINE_ENUM_BITWISE_OPERATORS( Type ) \
	inline Type  operator|  ( Type  a, Type b ) { return Type( int( a ) | int( b ) ); } \
	inline Type  operator&  ( Type  a, Type b ) { return Type( int( a ) & int( b ) ); } \
	inline Type  operator^  ( Type  a, Type b ) { return Type( int( a ) ^ int( b ) ); } \
	inline Type  operator<< ( Type  a, int  b ) { return Type( int( a ) << b ); } \
	inline Type  operator>> ( Type  a, int  b ) { return Type( int( a ) >> b ); } \
	inline Type &operator|= ( Type &a, Type b ) { return a = a |  b; } \
	inline Type &operator&= ( Type &a, Type b ) { return a = a &  b; } \
	inline Type &operator^= ( Type &a, Type b ) { return a = a ^  b; } \
	inline Type &operator<<=( Type &a, int  b ) { return a = a << b; } \
	inline Type &operator>>=( Type &a, int  b ) { return a = a >> b; } \
	inline Type  operator~( Type a ) { return Type( ~int( a ) ); }

// defines increment/decrement operators for enums for easy iteration
#define DEFINE_ENUM_INCREMENT_OPERATORS( Type ) \
	inline Type &operator++( Type &a      ) { return a = Type( int( a ) + 1 ); } \
	inline Type &operator--( Type &a      ) { return a = Type( int( a ) - 1 ); } \
	inline Type  operator++( Type &a, int ) { Type t = a; ++a; return t; } \
	inline Type  operator--( Type &a, int ) { Type t = a; --a; return t; }

// Max 2 player splitscreen in portal (don't merge this back), saves a bunch of memory [8/31/2010 tom]
#define MAX_SPLITSCREEN_CLIENT_BITS 1
// this should == MAX_JOYSTICKS in InputEnums.h
#define MAX_SPLITSCREEN_CLIENTS	( 1 << MAX_SPLITSCREEN_CLIENT_BITS ) // 2

#include "tier0/valve_on.h"

#endif // BASETYPES_H
