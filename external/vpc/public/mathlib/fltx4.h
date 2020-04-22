//===== Copyright 1996-2010, Valve Corporation, All rights reserved. ======//
//
// Purpose: - defines the type fltx4 - Avoid cyclic includion.
//
//===========================================================================//

#ifndef FLTX4_H
#define FLTX4_H

#if defined(GNUC)
#define USE_STDC_FOR_SIMD 0
#else
#define USE_STDC_FOR_SIMD 0
#endif

#if (!defined(PLATFORM_PPC) && (USE_STDC_FOR_SIMD == 0))
#define _SSE1 1
#endif

// I thought about defining a class/union for the SIMD packed floats instead of using fltx4,
// but decided against it because (a) the nature of SIMD code which includes comparisons is to blur
// the relationship between packed floats and packed integer types and (b) not sure that the
// compiler would handle generating good code for the intrinsics.

#if USE_STDC_FOR_SIMD

#error "hello"
typedef union
{
	float  m128_f32[4];
	uint32 m128_u32[4];
} fltx4;

typedef fltx4 i32x4;
typedef fltx4 u32x4;

#ifdef _PS3
typedef fltx4 u32x4;
typedef fltx4 i32x4;
#endif
typedef fltx4 bi32x4;

#elif ( defined( _PS3 ) )

typedef union
{
	// This union allows float/int access (which generally shouldn't be done in inner loops)

	vec_float4	vmxf;
	vec_int4	vmxi;
	vec_uint4	vmxui;
	__vector bool vmxbi;

	struct 
	{
		float x;
		float y;
		float z;
		float w;
	};

	float		m128_f32[4];
	uint32		m128_u32[4];
	int32		m128_i32[4];

} fltx4_union;

typedef vec_float4 fltx4;
typedef vec_uint4  u32x4;
typedef vec_int4   i32x4;
typedef __vector bool bi32x4;
#define DIFFERENT_NATIVE_VECTOR_TYPES // true if the compiler has different types for float4, uint4, int4, etc

#elif ( defined( _X360 ) )

typedef union
{
	// This union allows float/int access (which generally shouldn't be done in inner loops)
	__vector4	vmx;
	float		m128_f32[4];
	uint32		m128_u32[4];
} fltx4_union;

typedef __vector4 fltx4;
typedef __vector4 i32x4; // a VMX register; just a way of making it explicit that we're doing integer ops.
typedef __vector4 u32x4; // a VMX register; just a way of making it explicit that we're doing unsigned integer ops.
typedef fltx4 bi32x4;
#else

typedef __m128 fltx4;
typedef __m128 i32x4;
typedef __m128 u32x4;
typedef fltx4 bi32x4;

#endif

#endif
