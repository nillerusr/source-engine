//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANORAMACXX_H 
#define PANORAMACXX_H

#ifdef _WIN32
#pragma once
#endif

namespace panorama
{

// TEMPLATE CLASS integral_constant
template<class _Ty,
	_Ty _Val>
struct panorama_integral_constant
{	// convenient template for integral constant types
	static const _Ty value = _Val;

	typedef _Ty value_type;
	typedef panorama_integral_constant<_Ty, _Val> type;

	operator value_type() const
	{	// return stored value
		return (value);
	}
};

typedef panorama_integral_constant<bool, true> true_type;
typedef panorama_integral_constant<bool, false> false_type;


// TEMPLATE CLASS _Cat_base
template<bool>
struct panorama_Cat_base
	: false_type
{	// base class for type predicates
};

template<>
struct panorama_Cat_base < true >
	: true_type
{	// base class for type predicates
};

// TEMPLATE CLASS enable_if
template<bool _Test,
class _Ty = void>
struct panorama_enable_if
{	// type is undefined for assumed !_Test
};

template<class _Ty>
struct panorama_enable_if < true, _Ty >
{	// type is _Ty for _Test
	typedef _Ty type;
};

#define PANORAMA_IS_BASE_OF(_Base, _Der)	\
	: panorama_Cat_base<__is_base_of(_Base, _Der)>

// TEMPLATE CLASS is_base_of
template<class _Base, class _Der>
struct panorama_is_base_of PANORAMA_IS_BASE_OF( _Base, _Der )
{	// determine whether _Base is a base of or the same as _Der
};

#define PANORAMA_IS_ENUM(_Ty)	\
	: panorama_Cat_base<__is_enum(_Ty)>

// TEMPLATE CLASS is_enum
template<class _Ty>
struct panorama_is_enum
	PANORAMA_IS_ENUM( _Ty )
{	// determine whether _Ty is an enumerated type
};


// TEMPLATE CLASS is_lvalue_reference
template<class _Ty>
struct panorama_is_lvalue_reference
	: false_type
{	// determine whether _Ty is an lvalue reference
};

template<class _Ty>
struct panorama_is_lvalue_reference < _Ty& >
	: true_type
{	// determine whether _Ty is an lvalue reference
};


// TEMPLATE remove_reference
template<class _Ty>
struct panorama_remove_reference
{	// remove reference
	typedef _Ty type;
};

template<class _Ty>
struct panorama_remove_reference < _Ty& >
{	// remove reference
	typedef _Ty type;
};

template<class _Ty>
struct panorama_remove_reference < _Ty&& >
{	// remove rvalue reference
	typedef _Ty type;
};

// TEMPLATE FUNCTION forward
template<class _Ty> inline
_Ty&& panorama_forward( typename panorama_remove_reference<_Ty>::type& _Arg )
{	// forward an lvalue
	return (static_cast<_Ty&&>(_Arg));
}

template<class _Ty> inline
_Ty&& panorama_forward( typename panorama_remove_reference<_Ty>::type&& _Arg )
{	// forward anything
	static_assert(!panorama_is_lvalue_reference<_Ty>::value, "bad forward call");
	return (static_cast<_Ty&&>(_Arg));
}

}

#endif // PANORAMACXX_H
