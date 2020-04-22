//========== Copyright © 2006, Valve Corporation, All rights reserved. ========
//
// Purpose: Implements a generic infrastucture for functors combining
//			a number of techniques to provide transparent parameter type
//			deduction and packaging. Supports both member and non-member functions.
//
//			See also: http://en.wikipedia.org/wiki/Function_object
//
//			Note that getting into the guts of this file is not for the
//			feint of heart. The core concept here is that the compiler can
//			figure out all the parameter types.
//
//			E.g.:
//
//			struct CMyClass
//			{
//				void foo( int i) {}
//			};
//
//			int bar(int i) { return i; }
//
//			CMyClass myInstance;
//
//			CFunctor *pFunctor = CreateFunctor( &myInstance, CMyClass::foo, 8675 );
//			CFunctor *pFunctor2 = CreateFunctor( &bar, 309 );
//
//			void CallEm()
//			{
//				(*pFunctor)();
//				(*pFunctor2)();
//			}
//
//=============================================================================

#ifndef FUNCTORS_H
#define FUNCTORS_H

#if defined( _WIN32 )
#pragma once
#endif

#include "tier0/platform.h"
#include "tier1/refcount.h"
#include "tier1/utlenvelope.h"
#include <typeinfo>


//-----------------------------------------------------------------------------
//
// Macros used as basis for template generation. Just ignore the man behind the
// curtain
//
//-----------------------------------------------------------------------------

#define	FUNC_SOLO_TEMPLATE_ARG_PARAMS_0
#define	FUNC_TEMPLATE_ARG_PARAMS_0			
#define	FUNC_BASE_TEMPLATE_ARG_PARAMS_0		
#define	FUNC_SOLO_BASE_TEMPLATE_ARG_PARAMS_0
#define	FUNC_ARG_MEMBERS_0					
#define	FUNC_ARG_FORMAL_PARAMS_0			
#define	FUNC_PROXY_ARG_FORMAL_PARAMS_0			
#define	FUNC_CALL_ARGS_INIT_0				
#define	FUNC_SOLO_CALL_ARGS_INIT_0				
#define	FUNC_CALL_MEMBER_ARGS_0				
#define	FUNC_CALL_ARGS_0					
#define	FUNC_CALL_DATA_ARGS_0( _var )
#define	FUNC_FUNCTOR_CALL_ARGS_0			
#define	FUNC_TEMPLATE_FUNC_PARAMS_0			
#define	FUNC_BASE_TEMPLATE_FUNC_PARAMS_0	
#define	FUNC_VALIDATION_STRING_0			V_snprintf( pString, nBufLen, "method( void )" );
#define FUNC_SEPARATOR_0

#define	FUNC_SOLO_TEMPLATE_ARG_PARAMS_1		typename ARG_TYPE_1
#define	FUNC_TEMPLATE_ARG_PARAMS_1			, typename ARG_TYPE_1
#define	FUNC_BASE_TEMPLATE_ARG_PARAMS_1		, ARG_TYPE_1
#define	FUNC_SOLO_BASE_TEMPLATE_ARG_PARAMS_1		ARG_TYPE_1
#define	FUNC_ARG_MEMBERS_1					ARG_TYPE_1 m_arg1
#define	FUNC_ARG_FORMAL_PARAMS_1			, const ARG_TYPE_1 &arg1
#define	FUNC_PROXY_ARG_FORMAL_PARAMS_1		const ARG_TYPE_1 &arg1
#define	FUNC_CALL_ARGS_INIT_1				, m_arg1( arg1 )
#define	FUNC_SOLO_CALL_ARGS_INIT_1			: m_arg1( arg1 )	
#define	FUNC_CALL_MEMBER_ARGS_1				m_arg1
#define	FUNC_CALL_ARGS_1					arg1
#define	FUNC_CALL_DATA_ARGS_1( _var )		_var->m_arg1
#define	FUNC_FUNCTOR_CALL_ARGS_1			, arg1
#define	FUNC_TEMPLATE_FUNC_PARAMS_1			, typename FUNC_ARG_TYPE_1
#define	FUNC_BASE_TEMPLATE_FUNC_PARAMS_1	FUNC_ARG_TYPE_1
#define	FUNC_VALIDATION_STRING_1			V_snprintf( pString, nBufLen, "method( %s )", typeid( ARG_TYPE_1 ).name() );	
#define FUNC_SEPARATOR_1					,

#define	FUNC_SOLO_TEMPLATE_ARG_PARAMS_2		typename ARG_TYPE_1, typename ARG_TYPE_2
#define	FUNC_TEMPLATE_ARG_PARAMS_2			, typename ARG_TYPE_1, typename ARG_TYPE_2
#define	FUNC_BASE_TEMPLATE_ARG_PARAMS_2		, ARG_TYPE_1, ARG_TYPE_2
#define	FUNC_SOLO_BASE_TEMPLATE_ARG_PARAMS_2		ARG_TYPE_1, ARG_TYPE_2
#define	FUNC_ARG_MEMBERS_2					ARG_TYPE_1 m_arg1;	ARG_TYPE_2 m_arg2
#define	FUNC_ARG_FORMAL_PARAMS_2			, const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2
#define	FUNC_PROXY_ARG_FORMAL_PARAMS_2		const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2
#define	FUNC_CALL_ARGS_INIT_2				, m_arg1( arg1 ), m_arg2( arg2 )
#define	FUNC_SOLO_CALL_ARGS_INIT_2			: m_arg1( arg1 ), m_arg2( arg2 )
#define	FUNC_CALL_MEMBER_ARGS_2				m_arg1, m_arg2
#define	FUNC_CALL_ARGS_2					arg1, arg2
#define	FUNC_CALL_DATA_ARGS_2( _var )		_var->m_arg1, _var->m_arg2
#define	FUNC_FUNCTOR_CALL_ARGS_2			, arg1, arg2
#define	FUNC_TEMPLATE_FUNC_PARAMS_2			, typename FUNC_ARG_TYPE_1, typename	FUNC_ARG_TYPE_2
#define	FUNC_BASE_TEMPLATE_FUNC_PARAMS_2	FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2
#define	FUNC_VALIDATION_STRING_2			V_snprintf( pString, nBufLen, "method( %s, %s )", typeid( ARG_TYPE_1 ).name(), typeid( ARG_TYPE_2 ).name() );
#define FUNC_SEPARATOR_2					,

#define	FUNC_SOLO_TEMPLATE_ARG_PARAMS_3		typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3
#define	FUNC_TEMPLATE_ARG_PARAMS_3			, typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3
#define	FUNC_BASE_TEMPLATE_ARG_PARAMS_3		, ARG_TYPE_1, ARG_TYPE_2, ARG_TYPE_3
#define	FUNC_SOLO_BASE_TEMPLATE_ARG_PARAMS_3		ARG_TYPE_1, ARG_TYPE_2, ARG_TYPE_3
#define	FUNC_ARG_MEMBERS_3					ARG_TYPE_1 m_arg1;	ARG_TYPE_2 m_arg2; ARG_TYPE_3 m_arg3
#define	FUNC_ARG_FORMAL_PARAMS_3			, const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3
#define	FUNC_PROXY_ARG_FORMAL_PARAMS_3		const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3
#define	FUNC_CALL_ARGS_INIT_3				, m_arg1( arg1 ), m_arg2( arg2 ), m_arg3( arg3 )
#define	FUNC_SOLO_CALL_ARGS_INIT_3			: m_arg1( arg1 ), m_arg2( arg2 ), m_arg3( arg3 )
#define	FUNC_CALL_MEMBER_ARGS_3				m_arg1, m_arg2, m_arg3
#define	FUNC_CALL_ARGS_3					arg1, arg2, arg3
#define	FUNC_CALL_DATA_ARGS_3( _var )		_var->m_arg1, _var->m_arg2, _var->m_arg3
#define	FUNC_FUNCTOR_CALL_ARGS_3			, arg1, arg2, arg3
#define	FUNC_TEMPLATE_FUNC_PARAMS_3			, typename FUNC_ARG_TYPE_1, typename	FUNC_ARG_TYPE_2, typename FUNC_ARG_TYPE_3
#define	FUNC_BASE_TEMPLATE_FUNC_PARAMS_3	FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2, FUNC_ARG_TYPE_3
#define	FUNC_VALIDATION_STRING_3			V_snprintf( pString, nBufLen, "method( %s, %s, %s )", typeid( ARG_TYPE_1 ).name(), typeid( ARG_TYPE_2 ).name(), typeid( ARG_TYPE_3 ).name() );
#define FUNC_SEPARATOR_3					,

#define	FUNC_SOLO_TEMPLATE_ARG_PARAMS_4		typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4
#define	FUNC_TEMPLATE_ARG_PARAMS_4			, typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4
#define	FUNC_BASE_TEMPLATE_ARG_PARAMS_4		, ARG_TYPE_1, ARG_TYPE_2, ARG_TYPE_3, ARG_TYPE_4
#define	FUNC_SOLO_BASE_TEMPLATE_ARG_PARAMS_4		ARG_TYPE_1, ARG_TYPE_2, ARG_TYPE_3, ARG_TYPE_4
#define	FUNC_ARG_MEMBERS_4					ARG_TYPE_1 m_arg1;	ARG_TYPE_2 m_arg2; ARG_TYPE_3 m_arg3; ARG_TYPE_4 m_arg4
#define	FUNC_ARG_FORMAL_PARAMS_4			, const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4
#define	FUNC_PROXY_ARG_FORMAL_PARAMS_4		const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4
#define	FUNC_CALL_ARGS_INIT_4				, m_arg1( arg1 ), m_arg2( arg2 ), m_arg3( arg3 ), m_arg4( arg4 )
#define	FUNC_SOLO_CALL_ARGS_INIT_4			: m_arg1( arg1 ), m_arg2( arg2 ), m_arg3( arg3 ), m_arg4( arg4 )
#define	FUNC_CALL_MEMBER_ARGS_4				m_arg1, m_arg2, m_arg3, m_arg4
#define	FUNC_CALL_ARGS_4					arg1, arg2, arg3, arg4
#define	FUNC_CALL_DATA_ARGS_4( _var )		_var->m_arg1, _var->m_arg2, _var->m_arg3, _var->m_arg4
#define	FUNC_FUNCTOR_CALL_ARGS_4			, arg1, arg2, arg3, arg4
#define	FUNC_TEMPLATE_FUNC_PARAMS_4			, typename FUNC_ARG_TYPE_1, typename	FUNC_ARG_TYPE_2, typename FUNC_ARG_TYPE_3, typename	FUNC_ARG_TYPE_4
#define	FUNC_BASE_TEMPLATE_FUNC_PARAMS_4	FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2, FUNC_ARG_TYPE_3,	FUNC_ARG_TYPE_4
#define	FUNC_VALIDATION_STRING_4			V_snprintf( pString, nBufLen, "method( %s, %s, %s, %s )", typeid( ARG_TYPE_1 ).name(), typeid( ARG_TYPE_2 ).name(), typeid( ARG_TYPE_3 ).name(), typeid( ARG_TYPE_4 ).name() );
#define FUNC_SEPARATOR_4					,

#define	FUNC_SOLO_TEMPLATE_ARG_PARAMS_5		typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5
#define	FUNC_TEMPLATE_ARG_PARAMS_5			, typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5
#define	FUNC_BASE_TEMPLATE_ARG_PARAMS_5		, ARG_TYPE_1, ARG_TYPE_2, ARG_TYPE_3, ARG_TYPE_4, ARG_TYPE_5
#define	FUNC_SOLO_BASE_TEMPLATE_ARG_PARAMS_5		ARG_TYPE_1, ARG_TYPE_2, ARG_TYPE_3, ARG_TYPE_4, ARG_TYPE_5
#define	FUNC_ARG_MEMBERS_5					ARG_TYPE_1 m_arg1;	ARG_TYPE_2 m_arg2; ARG_TYPE_3 m_arg3; ARG_TYPE_4 m_arg4; ARG_TYPE_5 m_arg5
#define	FUNC_ARG_FORMAL_PARAMS_5			, const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5
#define	FUNC_PROXY_ARG_FORMAL_PARAMS_5		const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5
#define	FUNC_CALL_ARGS_INIT_5				, m_arg1( arg1 ), m_arg2( arg2 ), m_arg3( arg3 ), m_arg4( arg4 ), m_arg5( arg5 )
#define	FUNC_SOLO_CALL_ARGS_INIT_5			: m_arg1( arg1 ), m_arg2( arg2 ), m_arg3( arg3 ), m_arg4( arg4 ), m_arg5( arg5 )
#define	FUNC_CALL_MEMBER_ARGS_5				m_arg1, m_arg2, m_arg3, m_arg4, m_arg5
#define	FUNC_CALL_ARGS_5					arg1, arg2, arg3, arg4, arg5
#define	FUNC_CALL_DATA_ARGS_5( _var )		_var->m_arg1, _var->m_arg2, _var->m_arg3, _var->m_arg4, _var->m_arg5
#define	FUNC_FUNCTOR_CALL_ARGS_5			, arg1, arg2, arg3, arg4, arg5
#define	FUNC_TEMPLATE_FUNC_PARAMS_5			, typename FUNC_ARG_TYPE_1, typename	FUNC_ARG_TYPE_2, typename FUNC_ARG_TYPE_3, typename	FUNC_ARG_TYPE_4, typename FUNC_ARG_TYPE_5
#define	FUNC_BASE_TEMPLATE_FUNC_PARAMS_5	FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2, FUNC_ARG_TYPE_3,	FUNC_ARG_TYPE_4, FUNC_ARG_TYPE_5
#define	FUNC_VALIDATION_STRING_5			V_snprintf( pString, nBufLen, "method( %s, %s, %s, %s, %s )", typeid( ARG_TYPE_1 ).name(), typeid( ARG_TYPE_2 ).name(), typeid( ARG_TYPE_3 ).name(), typeid( ARG_TYPE_4 ).name(), typeid( ARG_TYPE_5 ).name() );
#define FUNC_SEPARATOR_5					,


#define	FUNC_SOLO_TEMPLATE_ARG_PARAMS_6		typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6
#define	FUNC_TEMPLATE_ARG_PARAMS_6			, typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6
#define	FUNC_BASE_TEMPLATE_ARG_PARAMS_6		, ARG_TYPE_1, ARG_TYPE_2, ARG_TYPE_3, ARG_TYPE_4, ARG_TYPE_5, ARG_TYPE_6
#define	FUNC_SOLO_BASE_TEMPLATE_ARG_PARAMS_6		ARG_TYPE_1, ARG_TYPE_2, ARG_TYPE_3, ARG_TYPE_4, ARG_TYPE_5, ARG_TYPE_6
#define	FUNC_ARG_MEMBERS_6					ARG_TYPE_1 m_arg1;	ARG_TYPE_2 m_arg2; ARG_TYPE_3 m_arg3; ARG_TYPE_4 m_arg4; ARG_TYPE_5 m_arg5;	ARG_TYPE_6 m_arg6
#define	FUNC_ARG_FORMAL_PARAMS_6			, const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5,	const ARG_TYPE_6 &arg6
#define	FUNC_PROXY_ARG_FORMAL_PARAMS_6		const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5,	const ARG_TYPE_6 &arg6
#define	FUNC_CALL_ARGS_INIT_6				, m_arg1( arg1 ), m_arg2( arg2 ), m_arg3( arg3 ), m_arg4( arg4 ), m_arg5( arg5 ), m_arg6( arg6 )
#define	FUNC_SOLO_CALL_ARGS_INIT_6			: m_arg1( arg1 ), m_arg2( arg2 ), m_arg3( arg3 ), m_arg4( arg4 ), m_arg5( arg5 ), m_arg6( arg6 )
#define	FUNC_CALL_MEMBER_ARGS_6				m_arg1, m_arg2, m_arg3, m_arg4, m_arg5, m_arg6
#define	FUNC_CALL_ARGS_6					arg1, arg2, arg3, arg4, arg5, arg6
#define	FUNC_CALL_DATA_ARGS_6( _var )		_var->m_arg1, _var->m_arg2, _var->m_arg3, _var->m_arg4, _var->m_arg5, _var->m_arg6
#define	FUNC_FUNCTOR_CALL_ARGS_6			, arg1, arg2, arg3, arg4, arg5, arg6
#define	FUNC_TEMPLATE_FUNC_PARAMS_6			, typename FUNC_ARG_TYPE_1, typename	FUNC_ARG_TYPE_2, typename FUNC_ARG_TYPE_3, typename	FUNC_ARG_TYPE_4, typename FUNC_ARG_TYPE_5, typename	FUNC_ARG_TYPE_6
#define	FUNC_BASE_TEMPLATE_FUNC_PARAMS_6	FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2, FUNC_ARG_TYPE_3,	FUNC_ARG_TYPE_4, FUNC_ARG_TYPE_5, FUNC_ARG_TYPE_6
#define	FUNC_VALIDATION_STRING_6			V_snprintf( pString, nBufLen, "method( %s, %s, %s, %s, %s, %s )", typeid( ARG_TYPE_1 ).name(), typeid( ARG_TYPE_2 ).name(), typeid( ARG_TYPE_3 ).name(), typeid( ARG_TYPE_4 ).name(), typeid( ARG_TYPE_5 ).name(), typeid( ARG_TYPE_6 ).name() );
#define FUNC_SEPARATOR_6					,

#define	FUNC_SOLO_TEMPLATE_ARG_PARAMS_7		typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7
#define	FUNC_TEMPLATE_ARG_PARAMS_7			, typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7
#define	FUNC_BASE_TEMPLATE_ARG_PARAMS_7		, ARG_TYPE_1, ARG_TYPE_2, ARG_TYPE_3, ARG_TYPE_4, ARG_TYPE_5, ARG_TYPE_6, ARG_TYPE_7
#define	FUNC_SOLO_BASE_TEMPLATE_ARG_PARAMS_7		ARG_TYPE_1, ARG_TYPE_2, ARG_TYPE_3, ARG_TYPE_4, ARG_TYPE_5, ARG_TYPE_6, ARG_TYPE_7
#define	FUNC_ARG_MEMBERS_7					ARG_TYPE_1 m_arg1;	ARG_TYPE_2 m_arg2; ARG_TYPE_3 m_arg3; ARG_TYPE_4 m_arg4; ARG_TYPE_5 m_arg5;	ARG_TYPE_6 m_arg6;	ARG_TYPE_7 m_arg7;
#define	FUNC_ARG_FORMAL_PARAMS_7			, const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5,	const ARG_TYPE_6 &arg6,	const ARG_TYPE_7 &arg7
#define	FUNC_PROXY_ARG_FORMAL_PARAMS_7		const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5,	const ARG_TYPE_6 &arg6,	const ARG_TYPE_7 &arg7
#define	FUNC_CALL_ARGS_INIT_7				, m_arg1( arg1 ), m_arg2( arg2 ), m_arg3( arg3 ), m_arg4( arg4 ), m_arg5( arg5 ), m_arg6( arg6 ), m_arg7( arg7 )
#define	FUNC_SOLO_CALL_ARGS_INIT_7			: m_arg1( arg1 ), m_arg2( arg2 ), m_arg3( arg3 ), m_arg4( arg4 ), m_arg5( arg5 ), m_arg6( arg6 ), m_arg7( arg7 )
#define	FUNC_CALL_MEMBER_ARGS_7				m_arg1, m_arg2, m_arg3, m_arg4, m_arg5, m_arg6, m_arg7
#define	FUNC_CALL_ARGS_7					arg1, arg2, arg3, arg4, arg5, arg6, arg7
#define	FUNC_CALL_DATA_ARGS_7( _var )		_var->m_arg1, _var->m_arg2, _var->m_arg3, _var->m_arg4, _var->m_arg5, _var->m_arg6, _var->m_arg7
#define	FUNC_FUNCTOR_CALL_ARGS_7			, arg1, arg2, arg3, arg4, arg5, arg6, arg7
#define	FUNC_TEMPLATE_FUNC_PARAMS_7			, typename FUNC_ARG_TYPE_1, typename	FUNC_ARG_TYPE_2, typename FUNC_ARG_TYPE_3, typename	FUNC_ARG_TYPE_4, typename FUNC_ARG_TYPE_5, typename	FUNC_ARG_TYPE_6, typename FUNC_ARG_TYPE_7
#define	FUNC_BASE_TEMPLATE_FUNC_PARAMS_7	FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2, FUNC_ARG_TYPE_3,	FUNC_ARG_TYPE_4, FUNC_ARG_TYPE_5, FUNC_ARG_TYPE_6, FUNC_ARG_TYPE_7
#define	FUNC_VALIDATION_STRING_7			V_snprintf( pString, nBufLen, "method( %s, %s, %s, %s, %s, %s, %s )", typeid( ARG_TYPE_1 ).name(), typeid( ARG_TYPE_2 ).name(), typeid( ARG_TYPE_3 ).name(), typeid( ARG_TYPE_4 ).name(), typeid( ARG_TYPE_5 ).name(), typeid( ARG_TYPE_6 ).name(), typeid( ARG_TYPE_7 ).name() );
#define FUNC_SEPARATOR_7					,

#define	FUNC_SOLO_TEMPLATE_ARG_PARAMS_8		typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8
#define	FUNC_TEMPLATE_ARG_PARAMS_8			, typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8
#define	FUNC_BASE_TEMPLATE_ARG_PARAMS_8		, ARG_TYPE_1, ARG_TYPE_2, ARG_TYPE_3, ARG_TYPE_4, ARG_TYPE_5, ARG_TYPE_6, ARG_TYPE_7, ARG_TYPE_8
#define	FUNC_SOLO_BASE_TEMPLATE_ARG_PARAMS_8		ARG_TYPE_1, ARG_TYPE_2, ARG_TYPE_3, ARG_TYPE_4, ARG_TYPE_5, ARG_TYPE_6, ARG_TYPE_7, ARG_TYPE_8
#define	FUNC_ARG_MEMBERS_8					ARG_TYPE_1 m_arg1;	ARG_TYPE_2 m_arg2; ARG_TYPE_3 m_arg3; ARG_TYPE_4 m_arg4; ARG_TYPE_5 m_arg5;	ARG_TYPE_6 m_arg6;	ARG_TYPE_7 m_arg7; ARG_TYPE_8 m_arg8;
#define	FUNC_ARG_FORMAL_PARAMS_8			, const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5,	const ARG_TYPE_6 &arg6,	const ARG_TYPE_7 &arg7, const ARG_TYPE_8 &arg8
#define	FUNC_PROXY_ARG_FORMAL_PARAMS_8		const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5,	const ARG_TYPE_6 &arg6,	const ARG_TYPE_7 &arg7, const ARG_TYPE_8 &arg8
#define	FUNC_CALL_ARGS_INIT_8				, m_arg1( arg1 ), m_arg2( arg2 ), m_arg3( arg3 ), m_arg4( arg4 ), m_arg5( arg5 ), m_arg6( arg6 ), m_arg7( arg7 ), m_arg8( arg8 )
#define	FUNC_SOLO_CALL_ARGS_INIT_8			: m_arg1( arg1 ), m_arg2( arg2 ), m_arg3( arg3 ), m_arg4( arg4 ), m_arg5( arg5 ), m_arg6( arg6 ), m_arg7( arg7 ), m_arg8( arg8 )
#define	FUNC_CALL_MEMBER_ARGS_8				m_arg1, m_arg2, m_arg3, m_arg4, m_arg5, m_arg6, m_arg7, m_arg8
#define	FUNC_CALL_ARGS_8					arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8
#define	FUNC_CALL_DATA_ARGS_8( _var )		_var->m_arg1, _var->m_arg2, _var->m_arg3, _var->m_arg4, _var->m_arg5, _var->m_arg6, _var->m_arg7, _var->m_arg8
#define	FUNC_FUNCTOR_CALL_ARGS_8			, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8
#define	FUNC_TEMPLATE_FUNC_PARAMS_8			, typename FUNC_ARG_TYPE_1, typename	FUNC_ARG_TYPE_2, typename FUNC_ARG_TYPE_3, typename	FUNC_ARG_TYPE_4, typename FUNC_ARG_TYPE_5, typename	FUNC_ARG_TYPE_6, typename FUNC_ARG_TYPE_7, typename FUNC_ARG_TYPE_8
#define	FUNC_BASE_TEMPLATE_FUNC_PARAMS_8	FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2, FUNC_ARG_TYPE_3,	FUNC_ARG_TYPE_4, FUNC_ARG_TYPE_5, FUNC_ARG_TYPE_6, FUNC_ARG_TYPE_7, FUNC_ARG_TYPE_8
#define	FUNC_VALIDATION_STRING_8			V_snprintf( pString, nBufLen, "method( %s, %s, %s, %s, %s, %s, %s, %s )", typeid( ARG_TYPE_1 ).name(), typeid( ARG_TYPE_2 ).name(), typeid( ARG_TYPE_3 ).name(), typeid( ARG_TYPE_4 ).name(), typeid( ARG_TYPE_5 ).name(), typeid( ARG_TYPE_6 ).name(), typeid( ARG_TYPE_7 ).name(), typeid( ARG_TYPE_8 ).name() );
#define FUNC_SEPARATOR_8					,

#define	FUNC_SOLO_TEMPLATE_ARG_PARAMS_9		typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9
#define	FUNC_TEMPLATE_ARG_PARAMS_9			, typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9
#define	FUNC_BASE_TEMPLATE_ARG_PARAMS_9		, ARG_TYPE_1, ARG_TYPE_2, ARG_TYPE_3, ARG_TYPE_4, ARG_TYPE_5, ARG_TYPE_6, ARG_TYPE_7, ARG_TYPE_8, ARG_TYPE_9
#define	FUNC_SOLO_BASE_TEMPLATE_ARG_PARAMS_9		ARG_TYPE_1, ARG_TYPE_2, ARG_TYPE_3, ARG_TYPE_4, ARG_TYPE_5, ARG_TYPE_6, ARG_TYPE_7, ARG_TYPE_8, ARG_TYPE_9
#define	FUNC_ARG_MEMBERS_9					ARG_TYPE_1 m_arg1;	ARG_TYPE_2 m_arg2; ARG_TYPE_3 m_arg3; ARG_TYPE_4 m_arg4; ARG_TYPE_5 m_arg5;	ARG_TYPE_6 m_arg6;	ARG_TYPE_7 m_arg7; ARG_TYPE_8 m_arg8; ARG_TYPE_9 m_arg9;
#define	FUNC_ARG_FORMAL_PARAMS_9			, const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5,	const ARG_TYPE_6 &arg6,	const ARG_TYPE_7 &arg7, const ARG_TYPE_8 &arg8, const ARG_TYPE_9 &arg9
#define	FUNC_PROXY_ARG_FORMAL_PARAMS_9		const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5,	const ARG_TYPE_6 &arg6,	const ARG_TYPE_7 &arg7, const ARG_TYPE_8 &arg8, const ARG_TYPE_9 &arg9
#define	FUNC_CALL_ARGS_INIT_9				, m_arg1( arg1 ), m_arg2( arg2 ), m_arg3( arg3 ), m_arg4( arg4 ), m_arg5( arg5 ), m_arg6( arg6 ), m_arg7( arg7 ), m_arg8( arg8 ), m_arg9( arg9 )
#define	FUNC_SOLO_CALL_ARGS_INIT_9			: m_arg1( arg1 ), m_arg2( arg2 ), m_arg3( arg3 ), m_arg4( arg4 ), m_arg5( arg5 ), m_arg6( arg6 ), m_arg7( arg7 ), m_arg8( arg8 ), m_arg9( arg9 )
#define	FUNC_CALL_MEMBER_ARGS_9				m_arg1, m_arg2, m_arg3, m_arg4, m_arg5, m_arg6, m_arg7, m_arg8, m_arg9
#define	FUNC_CALL_ARGS_9					arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9
#define	FUNC_CALL_DATA_ARGS_9( _var )		_var->m_arg1, _var->m_arg2, _var->m_arg3, _var->m_arg4, _var->m_arg5, _var->m_arg6, _var->m_arg7, _var->m_arg8, _var->m_arg9
#define	FUNC_FUNCTOR_CALL_ARGS_9			, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9
#define	FUNC_TEMPLATE_FUNC_PARAMS_9			, typename FUNC_ARG_TYPE_1, typename	FUNC_ARG_TYPE_2, typename FUNC_ARG_TYPE_3, typename	FUNC_ARG_TYPE_4, typename FUNC_ARG_TYPE_5, typename	FUNC_ARG_TYPE_6, typename FUNC_ARG_TYPE_7, typename FUNC_ARG_TYPE_8, typename	FUNC_ARG_TYPE_9
#define	FUNC_BASE_TEMPLATE_FUNC_PARAMS_9	FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2, FUNC_ARG_TYPE_3,	FUNC_ARG_TYPE_4, FUNC_ARG_TYPE_5, FUNC_ARG_TYPE_6, FUNC_ARG_TYPE_7, FUNC_ARG_TYPE_8, FUNC_ARG_TYPE_9
#define	FUNC_VALIDATION_STRING_9			V_snprintf( pString, nBufLen, "method( %s, %s, %s, %s, %s, %s, %s, %s, %s )", typeid( ARG_TYPE_1 ).name(), typeid( ARG_TYPE_2 ).name(), typeid( ARG_TYPE_3 ).name(), typeid( ARG_TYPE_4 ).name(), typeid( ARG_TYPE_5 ).name(), typeid( ARG_TYPE_6 ).name(), typeid( ARG_TYPE_7 ).name(), typeid( ARG_TYPE_8 ).name(), typeid( ARG_TYPE_9 ).name() );
#define FUNC_SEPARATOR_9					,

#define	FUNC_SOLO_TEMPLATE_ARG_PARAMS_10	typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10
#define	FUNC_TEMPLATE_ARG_PARAMS_10			, typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10
#define	FUNC_BASE_TEMPLATE_ARG_PARAMS_10	, ARG_TYPE_1, ARG_TYPE_2, ARG_TYPE_3, ARG_TYPE_4, ARG_TYPE_5, ARG_TYPE_6, ARG_TYPE_7, ARG_TYPE_8, ARG_TYPE_9, ARG_TYPE_10
#define	FUNC_SOLO_BASE_TEMPLATE_ARG_PARAMS_10	ARG_TYPE_1, ARG_TYPE_2, ARG_TYPE_3, ARG_TYPE_4, ARG_TYPE_5, ARG_TYPE_6, ARG_TYPE_7, ARG_TYPE_8, ARG_TYPE_9, ARG_TYPE_10
#define	FUNC_ARG_MEMBERS_10					ARG_TYPE_1 m_arg1;	ARG_TYPE_2 m_arg2; ARG_TYPE_3 m_arg3; ARG_TYPE_4 m_arg4; ARG_TYPE_5 m_arg5;	ARG_TYPE_6 m_arg6;	ARG_TYPE_7 m_arg7; ARG_TYPE_8 m_arg8; ARG_TYPE_9 m_arg9; ARG_TYPE_10 m_arg10;
#define	FUNC_ARG_FORMAL_PARAMS_10			, const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5,	const ARG_TYPE_6 &arg6,	const ARG_TYPE_7 &arg7, const ARG_TYPE_8 &arg8, const ARG_TYPE_9 &arg9, const ARG_TYPE_10 &arg10
#define	FUNC_PROXY_ARG_FORMAL_PARAMS_10		const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5,	const ARG_TYPE_6 &arg6,	const ARG_TYPE_7 &arg7, const ARG_TYPE_8 &arg8, const ARG_TYPE_9 &arg9, const ARG_TYPE_10 &arg10
#define	FUNC_CALL_ARGS_INIT_10				, m_arg1( arg1 ), m_arg2( arg2 ), m_arg3( arg3 ), m_arg4( arg4 ), m_arg5( arg5 ), m_arg6( arg6 ), m_arg7( arg7 ), m_arg8( arg8 ), m_arg9( arg9 ), m_arg10( arg10 )
#define	FUNC_SOLO_CALL_ARGS_INIT_10			: m_arg1( arg1 ), m_arg2( arg2 ), m_arg3( arg3 ), m_arg4( arg4 ), m_arg5( arg5 ), m_arg6( arg6 ), m_arg7( arg7 ), m_arg8( arg8 ), m_arg9( arg9 ), m_arg10( arg10 )
#define	FUNC_CALL_MEMBER_ARGS_10			m_arg1, m_arg2, m_arg3, m_arg4, m_arg5, m_arg6, m_arg7, m_arg8, m_arg9, m_arg10
#define	FUNC_CALL_ARGS_10					arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10
#define	FUNC_CALL_DATA_ARGS_10( _var )		_var->m_arg1, _var->m_arg2, _var->m_arg3, _var->m_arg4, _var->m_arg5, _var->m_arg6, _var->m_arg7, _var->m_arg8, _var->m_arg9, _var->m_arg10
#define	FUNC_FUNCTOR_CALL_ARGS_10			, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10
#define	FUNC_TEMPLATE_FUNC_PARAMS_10		, typename FUNC_ARG_TYPE_1, typename	FUNC_ARG_TYPE_2, typename FUNC_ARG_TYPE_3, typename	FUNC_ARG_TYPE_4, typename FUNC_ARG_TYPE_5, typename	FUNC_ARG_TYPE_6, typename FUNC_ARG_TYPE_7, typename FUNC_ARG_TYPE_8, typename	FUNC_ARG_TYPE_9, typename FUNC_ARG_TYPE_10
#define	FUNC_BASE_TEMPLATE_FUNC_PARAMS_10	FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2, FUNC_ARG_TYPE_3,	FUNC_ARG_TYPE_4, FUNC_ARG_TYPE_5, FUNC_ARG_TYPE_6, FUNC_ARG_TYPE_7, FUNC_ARG_TYPE_8, FUNC_ARG_TYPE_9, FUNC_ARG_TYPE_10
#define	FUNC_VALIDATION_STRING_10			V_snprintf( pString, nBufLen, "method( %s, %s, %s, %s, %s, %s, %s, %s, %s, %s )", typeid( ARG_TYPE_1 ).name(), typeid( ARG_TYPE_2 ).name(), typeid( ARG_TYPE_3 ).name(), typeid( ARG_TYPE_4 ).name(), typeid( ARG_TYPE_5 ).name(), typeid( ARG_TYPE_6 ).name(), typeid( ARG_TYPE_7 ).name(), typeid( ARG_TYPE_8 ).name(), typeid( ARG_TYPE_9 ).name(), typeid( ARG_TYPE_10 ).name() );
#define FUNC_SEPARATOR_10					,

#define	FUNC_SOLO_TEMPLATE_ARG_PARAMS_11	typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10, typename ARG_TYPE_11
#define	FUNC_TEMPLATE_ARG_PARAMS_11			, typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10, typename ARG_TYPE_11
#define	FUNC_BASE_TEMPLATE_ARG_PARAMS_11	, ARG_TYPE_1, ARG_TYPE_2, ARG_TYPE_3, ARG_TYPE_4, ARG_TYPE_5, ARG_TYPE_6, ARG_TYPE_7, ARG_TYPE_8, ARG_TYPE_9, ARG_TYPE_10, ARG_TYPE_11
#define	FUNC_SOLO_BASE_TEMPLATE_ARG_PARAMS_11	ARG_TYPE_1, ARG_TYPE_2, ARG_TYPE_3, ARG_TYPE_4, ARG_TYPE_5, ARG_TYPE_6, ARG_TYPE_7, ARG_TYPE_8, ARG_TYPE_9, ARG_TYPE_10, ARG_TYPE_11
#define	FUNC_ARG_MEMBERS_11					ARG_TYPE_1 m_arg1;	ARG_TYPE_2 m_arg2; ARG_TYPE_3 m_arg3; ARG_TYPE_4 m_arg4; ARG_TYPE_5 m_arg5;	ARG_TYPE_6 m_arg6;	ARG_TYPE_7 m_arg7; ARG_TYPE_8 m_arg8; ARG_TYPE_9 m_arg9; ARG_TYPE_10 m_arg10;	ARG_TYPE_11 m_arg11
#define	FUNC_ARG_FORMAL_PARAMS_11			, const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5,	const ARG_TYPE_6 &arg6,	const ARG_TYPE_7 &arg7, const ARG_TYPE_8 &arg8, const ARG_TYPE_9 &arg9, const ARG_TYPE_10 &arg10,	const ARG_TYPE_11 &arg11
#define	FUNC_PROXY_ARG_FORMAL_PARAMS_11		const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5,	const ARG_TYPE_6 &arg6,	const ARG_TYPE_7 &arg7, const ARG_TYPE_8 &arg8, const ARG_TYPE_9 &arg9, const ARG_TYPE_10 &arg10,	const ARG_TYPE_11 &arg11
#define	FUNC_CALL_ARGS_INIT_11				, m_arg1( arg1 ), m_arg2( arg2 ), m_arg3( arg3 ), m_arg4( arg4 ), m_arg5( arg5 ), m_arg6( arg6 ), m_arg7( arg7 ), m_arg8( arg8 ), m_arg9( arg9 ), m_arg10( arg10 ), m_arg11( arg11 )
#define	FUNC_SOLO_CALL_ARGS_INIT_11			: m_arg1( arg1 ), m_arg2( arg2 ), m_arg3( arg3 ), m_arg4( arg4 ), m_arg5( arg5 ), m_arg6( arg6 ), m_arg7( arg7 ), m_arg8( arg8 ), m_arg9( arg9 ), m_arg10( arg10 ), m_arg11( arg11 )
#define	FUNC_CALL_MEMBER_ARGS_11			m_arg1, m_arg2, m_arg3, m_arg4, m_arg5, m_arg6, m_arg7, m_arg8, m_arg9, m_arg10, m_arg11
#define	FUNC_CALL_ARGS_11					arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11
#define	FUNC_CALL_DATA_ARGS_11( _var )		_var->m_arg1, _var->m_arg2, _var->m_arg3, _var->m_arg4, _var->m_arg5, _var->m_arg6, _var->m_arg7, _var->m_arg8, _var->m_arg9, _var->m_arg10, _var->m_arg11
#define	FUNC_FUNCTOR_CALL_ARGS_11			, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11
#define	FUNC_TEMPLATE_FUNC_PARAMS_11		, typename FUNC_ARG_TYPE_1, typename	FUNC_ARG_TYPE_2, typename FUNC_ARG_TYPE_3, typename	FUNC_ARG_TYPE_4, typename FUNC_ARG_TYPE_5, typename	FUNC_ARG_TYPE_6, typename FUNC_ARG_TYPE_7, typename FUNC_ARG_TYPE_8, typename	FUNC_ARG_TYPE_9, typename FUNC_ARG_TYPE_10, typename FUNC_ARG_TYPE_11
#define	FUNC_BASE_TEMPLATE_FUNC_PARAMS_11	FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2, FUNC_ARG_TYPE_3,	FUNC_ARG_TYPE_4, FUNC_ARG_TYPE_5, FUNC_ARG_TYPE_6, FUNC_ARG_TYPE_7, FUNC_ARG_TYPE_8, FUNC_ARG_TYPE_9, FUNC_ARG_TYPE_10, FUNC_ARG_TYPE_11
#define	FUNC_VALIDATION_STRING_11			V_snprintf( pString, nBufLen, "method( %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s )", typeid( ARG_TYPE_1 ).name(), typeid( ARG_TYPE_2 ).name(), typeid( ARG_TYPE_3 ).name(), typeid( ARG_TYPE_4 ).name(), typeid( ARG_TYPE_5 ).name(), typeid( ARG_TYPE_6 ).name(), typeid( ARG_TYPE_7 ).name(), typeid( ARG_TYPE_8 ).name(), typeid( ARG_TYPE_9 ).name(), typeid( ARG_TYPE_10 ).name(), typeid( ARG_TYPE_11 ).name() );
#define FUNC_SEPARATOR_11					,

#define	FUNC_SOLO_TEMPLATE_ARG_PARAMS_12	typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10, typename ARG_TYPE_11, typename ARG_TYPE_12
#define	FUNC_TEMPLATE_ARG_PARAMS_12			, typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10, typename ARG_TYPE_11, typename ARG_TYPE_12
#define	FUNC_BASE_TEMPLATE_ARG_PARAMS_12	, ARG_TYPE_1, ARG_TYPE_2, ARG_TYPE_3, ARG_TYPE_4, ARG_TYPE_5, ARG_TYPE_6, ARG_TYPE_7, ARG_TYPE_8, ARG_TYPE_9, ARG_TYPE_10, ARG_TYPE_11, ARG_TYPE_12
#define	FUNC_SOLO_BASE_TEMPLATE_ARG_PARAMS_12	ARG_TYPE_1, ARG_TYPE_2, ARG_TYPE_3, ARG_TYPE_4, ARG_TYPE_5, ARG_TYPE_6, ARG_TYPE_7, ARG_TYPE_8, ARG_TYPE_9, ARG_TYPE_10, ARG_TYPE_11, ARG_TYPE_12
#define	FUNC_ARG_MEMBERS_12					ARG_TYPE_1 m_arg1;	ARG_TYPE_2 m_arg2; ARG_TYPE_3 m_arg3; ARG_TYPE_4 m_arg4; ARG_TYPE_5 m_arg5;	ARG_TYPE_6 m_arg6;	ARG_TYPE_7 m_arg7; ARG_TYPE_8 m_arg8; ARG_TYPE_9 m_arg9; ARG_TYPE_10 m_arg10; ARG_TYPE_11 m_arg11; ARG_TYPE_12 m_arg12
#define	FUNC_ARG_FORMAL_PARAMS_12			, const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5,	const ARG_TYPE_6 &arg6,	const ARG_TYPE_7 &arg7, const ARG_TYPE_8 &arg8, const ARG_TYPE_9 &arg9, const ARG_TYPE_10 &arg10, const ARG_TYPE_11 &arg11, const ARG_TYPE_12 &arg12
#define	FUNC_PROXY_ARG_FORMAL_PARAMS_12		const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5,	const ARG_TYPE_6 &arg6,	const ARG_TYPE_7 &arg7, const ARG_TYPE_8 &arg8, const ARG_TYPE_9 &arg9, const ARG_TYPE_10 &arg10, const ARG_TYPE_11 &arg11, const ARG_TYPE_12 &arg12
#define	FUNC_CALL_ARGS_INIT_12				, m_arg1( arg1 ), m_arg2( arg2 ), m_arg3( arg3 ), m_arg4( arg4 ), m_arg5( arg5 ), m_arg6( arg6 ), m_arg7( arg7 ), m_arg8( arg8 ), m_arg9( arg9 ), m_arg10( arg10 ), m_arg11( arg11 ), m_arg12( arg12 )
#define	FUNC_SOLO_CALL_ARGS_INIT_12			: m_arg1( arg1 ), m_arg2( arg2 ), m_arg3( arg3 ), m_arg4( arg4 ), m_arg5( arg5 ), m_arg6( arg6 ), m_arg7( arg7 ), m_arg8( arg8 ), m_arg9( arg9 ), m_arg10( arg10 ), m_arg11( arg11 ), m_arg12( arg12 )
#define	FUNC_CALL_MEMBER_ARGS_12			m_arg1, m_arg2, m_arg3, m_arg4, m_arg5, m_arg6, m_arg7, m_arg8, m_arg9, m_arg10, m_arg11, m_arg12
#define	FUNC_CALL_ARGS_12					arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12
#define	FUNC_CALL_DATA_ARGS_12( _var )		_var->m_arg1, _var->m_arg2, _var->m_arg3, _var->m_arg4, _var->m_arg5, _var->m_arg6, _var->m_arg7, _var->m_arg8, _var->m_arg9, _var->m_arg10, _var->m_arg11, _var->m_arg12
#define	FUNC_FUNCTOR_CALL_ARGS_12			, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12
#define	FUNC_TEMPLATE_FUNC_PARAMS_12		, typename FUNC_ARG_TYPE_1, typename	FUNC_ARG_TYPE_2, typename FUNC_ARG_TYPE_3, typename	FUNC_ARG_TYPE_4, typename FUNC_ARG_TYPE_5, typename	FUNC_ARG_TYPE_6, typename FUNC_ARG_TYPE_7, typename FUNC_ARG_TYPE_8, typename	FUNC_ARG_TYPE_9, typename FUNC_ARG_TYPE_10, typename FUNC_ARG_TYPE_11, typename FUNC_ARG_TYPE_12
#define	FUNC_BASE_TEMPLATE_FUNC_PARAMS_12	FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2, FUNC_ARG_TYPE_3,	FUNC_ARG_TYPE_4, FUNC_ARG_TYPE_5, FUNC_ARG_TYPE_6, FUNC_ARG_TYPE_7, FUNC_ARG_TYPE_8, FUNC_ARG_TYPE_9, FUNC_ARG_TYPE_10, FUNC_ARG_TYPE_11, FUNC_ARG_TYPE_12
#define	FUNC_VALIDATION_STRING_12			V_snprintf( pString, nBufLen, "method( %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s )", typeid( ARG_TYPE_1 ).name(), typeid( ARG_TYPE_2 ).name(), typeid( ARG_TYPE_3 ).name(), typeid( ARG_TYPE_4 ).name(), typeid( ARG_TYPE_5 ).name(), typeid( ARG_TYPE_6 ).name(), typeid( ARG_TYPE_7 ).name(), typeid( ARG_TYPE_8 ).name(), typeid( ARG_TYPE_9 ).name(), typeid( ARG_TYPE_10 ).name(), typeid( ARG_TYPE_11 ).name(), typeid( ARG_TYPE_12 ).name() );
#define FUNC_SEPARATOR_12					,

#define	FUNC_SOLO_TEMPLATE_ARG_PARAMS_13	typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10, typename ARG_TYPE_11, typename ARG_TYPE_12, typename ARG_TYPE_13
#define	FUNC_TEMPLATE_ARG_PARAMS_13			, typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10, typename ARG_TYPE_11, typename ARG_TYPE_12, typename ARG_TYPE_13
#define	FUNC_BASE_TEMPLATE_ARG_PARAMS_13	, ARG_TYPE_1, ARG_TYPE_2, ARG_TYPE_3, ARG_TYPE_4, ARG_TYPE_5, ARG_TYPE_6, ARG_TYPE_7, ARG_TYPE_8, ARG_TYPE_9, ARG_TYPE_10, ARG_TYPE_11, ARG_TYPE_12, ARG_TYPE_13
#define	FUNC_SOLO_BASE_TEMPLATE_ARG_PARAMS_13	ARG_TYPE_1, ARG_TYPE_2, ARG_TYPE_3, ARG_TYPE_4, ARG_TYPE_5, ARG_TYPE_6, ARG_TYPE_7, ARG_TYPE_8, ARG_TYPE_9, ARG_TYPE_10, ARG_TYPE_11, ARG_TYPE_12, ARG_TYPE_13
#define	FUNC_ARG_MEMBERS_13					ARG_TYPE_1 m_arg1;	ARG_TYPE_2 m_arg2; ARG_TYPE_3 m_arg3; ARG_TYPE_4 m_arg4; ARG_TYPE_5 m_arg5;	ARG_TYPE_6 m_arg6;	ARG_TYPE_7 m_arg7; ARG_TYPE_8 m_arg8; ARG_TYPE_9 m_arg9; ARG_TYPE_10 m_arg10; ARG_TYPE_11 m_arg11; ARG_TYPE_12 m_arg12; ARG_TYPE_13 m_arg13
#define	FUNC_ARG_FORMAL_PARAMS_13			, const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5,	const ARG_TYPE_6 &arg6,	const ARG_TYPE_7 &arg7, const ARG_TYPE_8 &arg8, const ARG_TYPE_9 &arg9, const ARG_TYPE_10 &arg10, const ARG_TYPE_11 &arg11, const ARG_TYPE_12 &arg12, const ARG_TYPE_13 &arg13
#define	FUNC_PROXY_ARG_FORMAL_PARAMS_13		const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5,	const ARG_TYPE_6 &arg6,	const ARG_TYPE_7 &arg7, const ARG_TYPE_8 &arg8, const ARG_TYPE_9 &arg9, const ARG_TYPE_10 &arg10, const ARG_TYPE_11 &arg11, const ARG_TYPE_12 &arg12, const ARG_TYPE_13 &arg13
#define	FUNC_CALL_ARGS_INIT_13				, m_arg1( arg1 ), m_arg2( arg2 ), m_arg3( arg3 ), m_arg4( arg4 ), m_arg5( arg5 ), m_arg6( arg6 ), m_arg7( arg7 ), m_arg8( arg8 ), m_arg9( arg9 ), m_arg10( arg10 ), m_arg11( arg11 ), m_arg12( arg12 ), m_arg13( arg13 )
#define	FUNC_SOLO_CALL_ARGS_INIT_13			: m_arg1( arg1 ), m_arg2( arg2 ), m_arg3( arg3 ), m_arg4( arg4 ), m_arg5( arg5 ), m_arg6( arg6 ), m_arg7( arg7 ), m_arg8( arg8 ), m_arg9( arg9 ), m_arg10( arg10 ), m_arg11( arg11 ), m_arg12( arg12 ), m_arg13( arg13 )
#define	FUNC_CALL_MEMBER_ARGS_13			m_arg1, m_arg2, m_arg3, m_arg4, m_arg5, m_arg6, m_arg7, m_arg8, m_arg9, m_arg10, m_arg11, m_arg12, m_arg13
#define	FUNC_CALL_ARGS_13					arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13
#define	FUNC_CALL_DATA_ARGS_13( _var )		_var->m_arg1, _var->m_arg2, _var->m_arg3, _var->m_arg4, _var->m_arg5, _var->m_arg6, _var->m_arg7, _var->m_arg8, _var->m_arg9, _var->m_arg10, _var->m_arg11, _var->m_arg12, _var->m_arg13
#define	FUNC_FUNCTOR_CALL_ARGS_13			, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13
#define	FUNC_TEMPLATE_FUNC_PARAMS_13		, typename FUNC_ARG_TYPE_1, typename	FUNC_ARG_TYPE_2, typename FUNC_ARG_TYPE_3, typename	FUNC_ARG_TYPE_4, typename FUNC_ARG_TYPE_5, typename	FUNC_ARG_TYPE_6, typename FUNC_ARG_TYPE_7, typename FUNC_ARG_TYPE_8, typename	FUNC_ARG_TYPE_9, typename FUNC_ARG_TYPE_10, typename FUNC_ARG_TYPE_11, typename FUNC_ARG_TYPE_12, typename FUNC_ARG_TYPE_13
#define	FUNC_BASE_TEMPLATE_FUNC_PARAMS_13	FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2, FUNC_ARG_TYPE_3,	FUNC_ARG_TYPE_4, FUNC_ARG_TYPE_5, FUNC_ARG_TYPE_6, FUNC_ARG_TYPE_7, FUNC_ARG_TYPE_8, FUNC_ARG_TYPE_9, FUNC_ARG_TYPE_10, FUNC_ARG_TYPE_11, FUNC_ARG_TYPE_12, FUNC_ARG_TYPE_13
#define	FUNC_VALIDATION_STRING_13			V_snprintf( pString, nBufLen, "method( %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s )", typeid( ARG_TYPE_1 ).name(), typeid( ARG_TYPE_2 ).name(), typeid( ARG_TYPE_3 ).name(), typeid( ARG_TYPE_4 ).name(), typeid( ARG_TYPE_5 ).name(), typeid( ARG_TYPE_6 ).name(), typeid( ARG_TYPE_7 ).name(), typeid( ARG_TYPE_8 ).name(), typeid( ARG_TYPE_9 ).name(), typeid( ARG_TYPE_10 ).name(), typeid( ARG_TYPE_11 ).name(), typeid( ARG_TYPE_12 ).name(), typeid( ARG_TYPE_13 ).name() );
#define FUNC_SEPARATOR_13					,

#define	FUNC_SOLO_TEMPLATE_ARG_PARAMS_14	typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10, typename ARG_TYPE_11, typename ARG_TYPE_12, typename ARG_TYPE_13, typename ARG_TYPE_14
#define	FUNC_TEMPLATE_ARG_PARAMS_14			, typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10, typename ARG_TYPE_11, typename ARG_TYPE_12, typename ARG_TYPE_13, typename ARG_TYPE_14
#define	FUNC_BASE_TEMPLATE_ARG_PARAMS_14	, ARG_TYPE_1, ARG_TYPE_2, ARG_TYPE_3, ARG_TYPE_4, ARG_TYPE_5, ARG_TYPE_6, ARG_TYPE_7, ARG_TYPE_8, ARG_TYPE_9, ARG_TYPE_10, ARG_TYPE_11, ARG_TYPE_12, ARG_TYPE_13, ARG_TYPE_14
#define	FUNC_SOLO_BASE_TEMPLATE_ARG_PARAMS_14	ARG_TYPE_1, ARG_TYPE_2, ARG_TYPE_3, ARG_TYPE_4, ARG_TYPE_5, ARG_TYPE_6, ARG_TYPE_7, ARG_TYPE_8, ARG_TYPE_9, ARG_TYPE_10, ARG_TYPE_11, ARG_TYPE_12, ARG_TYPE_13, ARG_TYPE_14
#define	FUNC_ARG_MEMBERS_14					ARG_TYPE_1 m_arg1;	ARG_TYPE_2 m_arg2; ARG_TYPE_3 m_arg3; ARG_TYPE_4 m_arg4; ARG_TYPE_5 m_arg5;	ARG_TYPE_6 m_arg6;	ARG_TYPE_7 m_arg7; ARG_TYPE_8 m_arg8; ARG_TYPE_9 m_arg9; ARG_TYPE_10 m_arg10; ARG_TYPE_11 m_arg11; ARG_TYPE_12 m_arg12; ARG_TYPE_13 m_arg13; ARG_TYPE_14 m_arg14
#define	FUNC_ARG_FORMAL_PARAMS_14			, const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5,	const ARG_TYPE_6 &arg6,	const ARG_TYPE_7 &arg7, const ARG_TYPE_8 &arg8, const ARG_TYPE_9 &arg9, const ARG_TYPE_10 &arg10, const ARG_TYPE_11 &arg11, const ARG_TYPE_12 &arg12, const ARG_TYPE_13 &arg13, const ARG_TYPE_14 &arg14
#define	FUNC_PROXY_ARG_FORMAL_PARAMS_14		const ARG_TYPE_1 &arg1,	const ARG_TYPE_2 &arg2, const ARG_TYPE_3 &arg3, const ARG_TYPE_4 &arg4, const ARG_TYPE_5 &arg5,	const ARG_TYPE_6 &arg6,	const ARG_TYPE_7 &arg7, const ARG_TYPE_8 &arg8, const ARG_TYPE_9 &arg9, const ARG_TYPE_10 &arg10, const ARG_TYPE_11 &arg11, const ARG_TYPE_12 &arg12, const ARG_TYPE_13 &arg13, const ARG_TYPE_14 &arg14
#define	FUNC_CALL_ARGS_INIT_14				, m_arg1( arg1 ), m_arg2( arg2 ), m_arg3( arg3 ), m_arg4( arg4 ), m_arg5( arg5 ), m_arg6( arg6 ), m_arg7( arg7 ), m_arg8( arg8 ), m_arg9( arg9 ), m_arg10( arg10 ), m_arg11( arg11 ), m_arg12( arg12 ), m_arg13( arg13 ), m_arg14( arg14 )
#define	FUNC_SOLO_CALL_ARGS_INIT_14			: m_arg1( arg1 ), m_arg2( arg2 ), m_arg3( arg3 ), m_arg4( arg4 ), m_arg5( arg5 ), m_arg6( arg6 ), m_arg7( arg7 ), m_arg8( arg8 ), m_arg9( arg9 ), m_arg10( arg10 ), m_arg11( arg11 ), m_arg12( arg12 ), m_arg13( arg13 ), m_arg14( arg14 )
#define	FUNC_CALL_MEMBER_ARGS_14			m_arg1, m_arg2, m_arg3, m_arg4, m_arg5, m_arg6, m_arg7, m_arg8, m_arg9, m_arg10, m_arg11, m_arg12, m_arg13, m_arg14
#define	FUNC_CALL_ARGS_14					arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13, arg14
#define	FUNC_CALL_DATA_ARGS_14( _var )		_var->m_arg1, _var->m_arg2, _var->m_arg3, _var->m_arg4, _var->m_arg5, _var->m_arg6, _var->m_arg7, _var->m_arg8, _var->m_arg9, _var->m_arg10, _var->m_arg11, _var->m_arg12, _var->m_arg13, _var->m_arg14
#define	FUNC_FUNCTOR_CALL_ARGS_14			, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13, arg14
#define	FUNC_TEMPLATE_FUNC_PARAMS_14		, typename FUNC_ARG_TYPE_1, typename	FUNC_ARG_TYPE_2, typename FUNC_ARG_TYPE_3, typename	FUNC_ARG_TYPE_4, typename FUNC_ARG_TYPE_5, typename	FUNC_ARG_TYPE_6, typename FUNC_ARG_TYPE_7, typename FUNC_ARG_TYPE_8, typename	FUNC_ARG_TYPE_9, typename FUNC_ARG_TYPE_10, typename FUNC_ARG_TYPE_11, typename FUNC_ARG_TYPE_12, typename FUNC_ARG_TYPE_13, typename FUNC_ARG_TYPE_14
#define	FUNC_BASE_TEMPLATE_FUNC_PARAMS_14	FUNC_ARG_TYPE_1, FUNC_ARG_TYPE_2, FUNC_ARG_TYPE_3,	FUNC_ARG_TYPE_4, FUNC_ARG_TYPE_5, FUNC_ARG_TYPE_6, FUNC_ARG_TYPE_7, FUNC_ARG_TYPE_8, FUNC_ARG_TYPE_9, FUNC_ARG_TYPE_10, FUNC_ARG_TYPE_11, FUNC_ARG_TYPE_12, FUNC_ARG_TYPE_13, FUNC_ARG_TYPE_14
#define	FUNC_VALIDATION_STRING_14			V_snprintf( pString, nBufLen, "method( %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s )", typeid( ARG_TYPE_1 ).name(), typeid( ARG_TYPE_2 ).name(), typeid( ARG_TYPE_3 ).name(), typeid( ARG_TYPE_4 ).name(), typeid( ARG_TYPE_5 ).name(), typeid( ARG_TYPE_6 ).name(), typeid( ARG_TYPE_7 ).name(), typeid( ARG_TYPE_8 ).name(), typeid( ARG_TYPE_9 ).name(), typeid( ARG_TYPE_10 ).name(), typeid( ARG_TYPE_11 ).name(), typeid( ARG_TYPE_12 ).name(), typeid( ARG_TYPE_13 ).name(), typeid( ARG_TYPE_14 ).name() );
#define FUNC_SEPARATOR_14					,

#define FUNC_GENERATE_ALL_BUT0( INNERMACRONAME ) \
	INNERMACRONAME(1); \
	INNERMACRONAME(2); \
	INNERMACRONAME(3); \
	INNERMACRONAME(4); \
	INNERMACRONAME(5); \
	INNERMACRONAME(6); \
	INNERMACRONAME(7); \
	INNERMACRONAME(8); \
	INNERMACRONAME(9); \
	INNERMACRONAME(10);\
	INNERMACRONAME(11);\
	INNERMACRONAME(12);\
	INNERMACRONAME(13);\
	INNERMACRONAME(14)

#define FUNC_GENERATE_ALL( INNERMACRONAME ) \
	INNERMACRONAME(0); \
	FUNC_GENERATE_ALL_BUT0( INNERMACRONAME )


//-----------------------------------------------------------------------------
//
// Purpose: Base class of all function objects
//
//-----------------------------------------------------------------------------
abstract_class CFunctor : public IRefCounted
{
public:
	CFunctor()
	{
#ifdef DEBUG
		m_nUserID = 0;
#endif
	}
	virtual ~CFunctor() {}
	virtual void operator()() = 0;

	unsigned m_nUserID; // For debugging
};


//-----------------------------------------------------------------------------
// NOTE: Functor data + functor callback are tied together
// The basic idea is that someone creates the functor data. At a later point,
// the functor data is passed to a functor callback. Validation strings
// are compared in debug builds to ensure the data matches the callback
//-----------------------------------------------------------------------------
abstract_class CFunctorData : public IRefCounted
{
public:
	virtual void ComputeValidationString( char *pString, size_t nBufLen ) const = 0; 
};

abstract_class CFunctorCallback : public IRefCounted
{
public:
	virtual bool IsEqual( CFunctorCallback *pSrc ) const = 0;
	virtual void operator()( CFunctorData *pData ) = 0;
	virtual void ComputeValidationString( char *pString, size_t nBufLen ) const = 0; 
	virtual const char *GetImplClassName() const = 0;
	virtual const void *GetTarget() const = 0;
};


//-----------------------------------------------------------------------------
// When calling through a functor, care needs to be taken to not pass objects that might go away. 
// Since this code determines the type to store in the functor based on the actual arguments,
// this is achieved by changing the point of call. 
//
// See also CUtlEnvelope
//-----------------------------------------------------------------------------
// convert a reference to a passable value
template <typename T>
inline T RefToVal(const T &item)
{
   return item;
}


//-----------------------------------------------------------------------------
// This class can be used to pass into a functor a proxy object for a pointer
// to be resolved later. For example, you can execute a "call" on a resource
// whose actual value is not known until a later time
//-----------------------------------------------------------------------------
template <typename T>
class CLateBoundPtr
{
public:
	CLateBoundPtr( T **ppObject )
		: m_ppObject( ppObject )
	{
	}

	T *operator->() { return *m_ppObject;	}
	T &operator *() { return **m_ppObject;	}
	operator T *() const { return (T*)(*m_ppObject);	}
	operator void *() { return *m_ppObject;	}

private:
	T **m_ppObject;
};


//-----------------------------------------------------------------------------
//
// Purpose: Classes to define memory management policies when operating
//			on pointers to members
//
//-----------------------------------------------------------------------------
class CFuncMemPolicyNone
{
public:
	static void OnAcquire(void *pObject)	{}
	static void OnRelease(void *pObject)	{}
};

template <class OBJECT_TYPE_PTR = IRefCounted *>
class CFuncMemPolicyRefCount
{
public:
	static void OnAcquire(OBJECT_TYPE_PTR pObject)	{ pObject->AddRef(); }
	static void OnRelease(OBJECT_TYPE_PTR pObject)	{ pObject->Release(); }
};


//-----------------------------------------------------------------------------
//
// Purpose: Function proxy is a generic facility for holding a function
//			pointer. Can be used on own, though primarily for use
//			by this file
//
//-----------------------------------------------------------------------------
template <class OBJECT_TYPE_PTR, typename FUNCTION_TYPE, class MEM_POLICY = CFuncMemPolicyNone >
class CMemberFuncProxyBase
{
public:
	bool operator==( const CMemberFuncProxyBase &src ) const
	{
		return m_pfnProxied == src.m_pfnProxied && m_pObject == src.m_pObject;
	}

	const void *GetTarget() const
	{
		return m_pObject;
	}

protected:
	CMemberFuncProxyBase( OBJECT_TYPE_PTR pObject, FUNCTION_TYPE pfnProxied )
	  : m_pObject( pObject ),
		m_pfnProxied( pfnProxied )
	{
		MEM_POLICY::OnAcquire(m_pObject);
	}

	~CMemberFuncProxyBase()
	{
		MEM_POLICY::OnRelease(m_pObject);
	}
	
	void Set( OBJECT_TYPE_PTR pObject, FUNCTION_TYPE pfnProxied )
	{
		m_pfnProxied = pfnProxied;
		m_pObject = pObject;
	}

	void OnCall()
	{
		Assert( (void *)m_pObject != NULL );
	}

	FUNCTION_TYPE m_pfnProxied;
	OBJECT_TYPE_PTR m_pObject;
};


#define DEFINE_MEMBER_FUNC_PROXY( N ) \
	template <class OBJECT_TYPE_PTR, typename FUNCTION_TYPE FUNC_TEMPLATE_ARG_PARAMS_##N, class MEM_POLICY = CFuncMemPolicyNone> \
	class CMemberFuncProxy##N : public CMemberFuncProxyBase<OBJECT_TYPE_PTR, FUNCTION_TYPE, MEM_POLICY> \
	{ \
	public: \
		CMemberFuncProxy##N( OBJECT_TYPE_PTR pObject = NULL, FUNCTION_TYPE pfnProxied = NULL ) \
		: CMemberFuncProxyBase<OBJECT_TYPE_PTR, FUNCTION_TYPE, MEM_POLICY >( pObject, pfnProxied ) \
		{ \
		} \
	\
		void operator()( FUNC_PROXY_ARG_FORMAL_PARAMS_##N ) \
		{ \
			this->OnCall(); \
			((*this->m_pObject).*this->m_pfnProxied)( FUNC_CALL_ARGS_##N ); \
		} \
	}

FUNC_GENERATE_ALL( DEFINE_MEMBER_FUNC_PROXY );


//-----------------------------------------------------------------------------
//
// The actual functor implementation
//
//-----------------------------------------------------------------------------

#include "tier0/memdbgon.h"

typedef CRefCounted1<CFunctor, CRefCountServiceMT> CFunctorBase;

#define DEFINE_FUNCTOR_TEMPLATE(N) \
	template <typename FUNC_TYPE FUNC_TEMPLATE_ARG_PARAMS_##N, class FUNCTOR_BASE = CFunctorBase> \
	class CFunctor##N : public CFunctorBase \
	{ \
	public: \
		CFunctor##N( FUNC_TYPE pfnProxied FUNC_ARG_FORMAL_PARAMS_##N ) : m_pfnProxied( pfnProxied ) FUNC_CALL_ARGS_INIT_##N {} \
		void operator()() { m_pfnProxied(FUNC_CALL_MEMBER_ARGS_##N); } \
	\
	private: \
		FUNC_TYPE m_pfnProxied; \
		FUNC_ARG_MEMBERS_##N; \
	}

FUNC_GENERATE_ALL( DEFINE_FUNCTOR_TEMPLATE );

#define DEFINE_MEMBER_FUNCTOR( N ) \
	template <class OBJECT_TYPE_PTR, typename FUNCTION_TYPE FUNC_TEMPLATE_ARG_PARAMS_##N, class FUNCTOR_BASE = CFunctorBase, class MEM_POLICY = CFuncMemPolicyNone> \
	class CMemberFunctor##N : public FUNCTOR_BASE \
	{ \
	public: \
		CMemberFunctor##N( OBJECT_TYPE_PTR pObject, FUNCTION_TYPE pfnProxied FUNC_ARG_FORMAL_PARAMS_##N ) : m_Proxy( pObject, pfnProxied ) FUNC_CALL_ARGS_INIT_##N {} \
		void operator()() { m_Proxy(FUNC_CALL_MEMBER_ARGS_##N); } \
		\
	private: \
		CMemberFuncProxy##N<OBJECT_TYPE_PTR, FUNCTION_TYPE FUNC_BASE_TEMPLATE_ARG_PARAMS_##N, MEM_POLICY> m_Proxy; \
		FUNC_ARG_MEMBERS_##N; \
	};


FUNC_GENERATE_ALL( DEFINE_MEMBER_FUNCTOR );

typedef CRefCounted1<CFunctorData, CRefCountServiceMT> CFunctorDataBase;
class CFunctorCallbackBase : public CRefCounted1<CFunctorCallback, CRefCountServiceMT>
{
protected:
	virtual void ValidateFunctorData( CFunctorData *pData )
	{
#ifdef _DEBUG
		char pDataString[1024];
		char pCallbackString[1024];
		ComputeValidationString( pCallbackString, sizeof(pCallbackString) );
		pData->ComputeValidationString( pDataString, sizeof(pDataString) );
		bool bMatch = !V_stricmp( pDataString, pCallbackString );
		if ( !bMatch )
		{
			Warning( "Functor doesn't match data!\n\tExpected:\t%s\n\tEncountered:\t%s\n",
				pCallbackString, pDataString );
			Assert( 0 );
		}
#endif
	}
};

#define DEFINE_FUNCTOR_DATA_TEMPLATE(N) \
	template < FUNC_SOLO_TEMPLATE_ARG_PARAMS_##N > \
	class CFunctorData##N : public CFunctorDataBase \
	{ \
	public: \
		CFunctorData##N( FUNC_PROXY_ARG_FORMAL_PARAMS_##N ) FUNC_SOLO_CALL_ARGS_INIT_##N {} \
		virtual void ComputeValidationString( char *pString, size_t nBufLen ) const { FUNC_VALIDATION_STRING_##N } \
		FUNC_ARG_MEMBERS_##N; \
	}

class CFunctorData0 : public CFunctorDataBase
{
public:
	CFunctorData0( ) {}
	virtual void ComputeValidationString( char *pString, size_t nBufLen ) const { FUNC_VALIDATION_STRING_0 }
};

FUNC_GENERATE_ALL_BUT0( DEFINE_FUNCTOR_DATA_TEMPLATE );

#define DEFINE_FUNCTOR_CALLBACK_TEMPLATE(N) \
	template < FUNC_SOLO_TEMPLATE_ARG_PARAMS_##N > \
	class CFunctorCallback##N : public CFunctorCallbackBase \
	{ \
		typedef void (*Callback_t)( FUNC_PROXY_ARG_FORMAL_PARAMS_##N ); \
	public: \
		CFunctorCallback##N( Callback_t pfnProxied ) : m_pfnProxied( pfnProxied ) {} \
		void operator()( CFunctorData *pFunctorDataBase ) \
		{ \
			ValidateFunctorData( pFunctorDataBase ); \
			CFunctorData##N< FUNC_SOLO_BASE_TEMPLATE_ARG_PARAMS_##N > *pFunctorData = static_cast< CFunctorData##N< FUNC_SOLO_BASE_TEMPLATE_ARG_PARAMS_##N >* >( pFunctorDataBase ); \
			m_pfnProxied( FUNC_CALL_DATA_ARGS_##N(pFunctorData) ); \
		} \
		virtual bool IsEqual( CFunctorCallback *pSrc ) const { return !V_stricmp( GetImplClassName(), pSrc->GetImplClassName() ) && ( m_pfnProxied == static_cast< CFunctorCallback##N * >( pSrc )->m_pfnProxied ); } \
		virtual void ComputeValidationString( char *pString, size_t nBufLen ) const { FUNC_VALIDATION_STRING_##N } \
		virtual const char *GetImplClassName() const { return "CFunctorCallback" #N; } \
		virtual const void *GetTarget() const { return m_pfnProxied; } \
	private: \
		Callback_t m_pfnProxied; \
	}

class CFunctorCallback0 : public CFunctorCallbackBase
{
	typedef void (*Callback_t)( ); 
public:
	CFunctorCallback0( Callback_t pfnProxied ) : m_pfnProxied( pfnProxied ) {}
	void operator()( CFunctorData *pFunctorDataBase )
	{
		ValidateFunctorData( pFunctorDataBase );
		m_pfnProxied( );
	}
	virtual void ComputeValidationString( char *pString, size_t nBufLen ) const { FUNC_VALIDATION_STRING_0 }
	virtual bool IsEqual( CFunctorCallback *pSrc ) const 
	{ 
		if ( V_stricmp( GetImplClassName(), pSrc->GetImplClassName() ) )
			return false;
		return m_pfnProxied == static_cast< CFunctorCallback0* >( pSrc )->m_pfnProxied; 
	}
	virtual const char *GetImplClassName() const { return "CFunctorCallback0"; }
	virtual const void *GetTarget() const { return ( void * )m_pfnProxied; }
private:
	Callback_t m_pfnProxied; 
};

FUNC_GENERATE_ALL_BUT0( DEFINE_FUNCTOR_CALLBACK_TEMPLATE );

#define DEFINE_MEMBER_FUNCTOR_CALLBACK_TEMPLATE( N ) \
	template < class FUNCTION_CLASS FUNC_TEMPLATE_ARG_PARAMS_##N, class MEM_POLICY = CFuncMemPolicyNone > \
	class CMemberFunctorCallback##N : public CFunctorCallbackBase \
	{ \
		typedef void (FUNCTION_CLASS::*MemberCallback_t)( FUNC_PROXY_ARG_FORMAL_PARAMS_##N ); \
	public: \
		CMemberFunctorCallback##N( FUNCTION_CLASS *pObject, MemberCallback_t pfnProxied ) : m_Proxy( pObject, pfnProxied ) {} \
		void operator()( CFunctorData *pFunctorDataBase ) \
		{ \
			ValidateFunctorData( pFunctorDataBase ); \
			CFunctorData##N< FUNC_SOLO_BASE_TEMPLATE_ARG_PARAMS_##N > *pFunctorData = static_cast< CFunctorData##N< FUNC_SOLO_BASE_TEMPLATE_ARG_PARAMS_##N >* >( pFunctorDataBase ); \
			m_Proxy( FUNC_CALL_DATA_ARGS_##N( pFunctorData ) ); \
		} \
		virtual void ComputeValidationString( char *pString, size_t nBufLen ) const { FUNC_VALIDATION_STRING_##N } \
		virtual bool IsEqual( CFunctorCallback *pSrc ) const { return !V_stricmp( GetImplClassName(), pSrc->GetImplClassName() ) && ( m_Proxy == static_cast< CMemberFunctorCallback##N* >( pSrc )->m_Proxy ); } \
		virtual const char *GetImplClassName() const { return "CMemberFunctorCallback" #N; } \
		virtual const void *GetTarget() const { return m_Proxy.GetTarget(); } \
	private: \
		CMemberFuncProxy##N< FUNCTION_CLASS *, MemberCallback_t FUNC_BASE_TEMPLATE_ARG_PARAMS_##N, MEM_POLICY> m_Proxy; \
	}

template < class FUNCTION_CLASS, class MEM_POLICY = CFuncMemPolicyNone > 
class CMemberFunctorCallback0 : public CFunctorCallbackBase
{
	typedef void (FUNCTION_CLASS::*MemberCallback_t)( );
public:
	CMemberFunctorCallback0( FUNCTION_CLASS *pObject, MemberCallback_t pfnProxied ) : m_Proxy( pObject, pfnProxied ) {} 
	void operator()( CFunctorData *pFunctorDataBase )
	{
		ValidateFunctorData( pFunctorDataBase );
		m_Proxy( );
	}
	virtual void ComputeValidationString( char *pString, size_t nBufLen ) const { FUNC_VALIDATION_STRING_0 }
	virtual bool IsEqual( CFunctorCallback *pSrc ) const 
	{ 
		if ( V_stricmp( GetImplClassName(), pSrc->GetImplClassName() ) )
			return false;
		return m_Proxy == static_cast< CMemberFunctorCallback0 * >( pSrc )->m_Proxy; 
	}
	virtual const char *GetImplClassName() const { return "CMemberFunctorCallback0"; } 
	virtual const void *GetTarget() const { return m_Proxy.GetTarget(); }
private:
	CMemberFuncProxy0< FUNCTION_CLASS *, MemberCallback_t, MEM_POLICY > m_Proxy;
};

FUNC_GENERATE_ALL_BUT0( DEFINE_MEMBER_FUNCTOR_CALLBACK_TEMPLATE );


//-----------------------------------------------------------------------------
//
// The real magic, letting the compiler figure out all the right template parameters
//
//-----------------------------------------------------------------------------

#define DEFINE_NONMEMBER_FUNCTOR_FACTORY(N) \
	template <typename FUNCTION_RETTYPE FUNC_TEMPLATE_FUNC_PARAMS_##N FUNC_TEMPLATE_ARG_PARAMS_##N> \
	inline CFunctor *CreateFunctor(FUNCTION_RETTYPE (*pfnProxied)( FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N ) FUNC_ARG_FORMAL_PARAMS_##N ) \
	{ \
		typedef FUNCTION_RETTYPE (*Func_t)(FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N); \
		return new CFunctor##N<Func_t FUNC_BASE_TEMPLATE_ARG_PARAMS_##N>( pfnProxied FUNC_FUNCTOR_CALL_ARGS_##N ); \
	}

FUNC_GENERATE_ALL( DEFINE_NONMEMBER_FUNCTOR_FACTORY );

//-------------------------------------

#define DEFINE_MEMBER_FUNCTOR_FACTORY(N) \
	template <typename OBJECT_TYPE_PTR, typename FUNCTION_CLASS, typename FUNCTION_RETTYPE FUNC_TEMPLATE_FUNC_PARAMS_##N FUNC_TEMPLATE_ARG_PARAMS_##N> \
	inline CFunctor *CreateFunctor(OBJECT_TYPE_PTR pObject, FUNCTION_RETTYPE ( FUNCTION_CLASS::*pfnProxied )( FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N ) FUNC_ARG_FORMAL_PARAMS_##N ) \
	{ \
		return new CMemberFunctor##N<OBJECT_TYPE_PTR, FUNCTION_RETTYPE (FUNCTION_CLASS::*)(FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N) FUNC_BASE_TEMPLATE_ARG_PARAMS_##N>(pObject, pfnProxied FUNC_FUNCTOR_CALL_ARGS_##N); \
	}

FUNC_GENERATE_ALL( DEFINE_MEMBER_FUNCTOR_FACTORY );

//-------------------------------------

#define DEFINE_CONST_MEMBER_FUNCTOR_FACTORY(N) \
	template <typename OBJECT_TYPE_PTR, typename FUNCTION_CLASS, typename FUNCTION_RETTYPE FUNC_TEMPLATE_FUNC_PARAMS_##N FUNC_TEMPLATE_ARG_PARAMS_##N> \
	inline CFunctor *CreateFunctor(OBJECT_TYPE_PTR pObject, FUNCTION_RETTYPE ( FUNCTION_CLASS::*pfnProxied )( FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N ) const FUNC_ARG_FORMAL_PARAMS_##N ) \
	{ \
		return new CMemberFunctor##N<OBJECT_TYPE_PTR, FUNCTION_RETTYPE (FUNCTION_CLASS::*)(FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N) const FUNC_BASE_TEMPLATE_ARG_PARAMS_##N>(pObject, pfnProxied FUNC_FUNCTOR_CALL_ARGS_##N); \
	}

FUNC_GENERATE_ALL( DEFINE_CONST_MEMBER_FUNCTOR_FACTORY );

//-------------------------------------

#define DEFINE_REF_COUNTING_MEMBER_FUNCTOR_FACTORY(N) \
	template <typename OBJECT_TYPE_PTR, typename FUNCTION_CLASS, typename FUNCTION_RETTYPE FUNC_TEMPLATE_FUNC_PARAMS_##N FUNC_TEMPLATE_ARG_PARAMS_##N> \
	inline CFunctor *CreateRefCountingFunctor(OBJECT_TYPE_PTR pObject, FUNCTION_RETTYPE ( FUNCTION_CLASS::*pfnProxied )( FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N ) FUNC_ARG_FORMAL_PARAMS_##N ) \
	{ \
		return new CMemberFunctor##N<OBJECT_TYPE_PTR, FUNCTION_RETTYPE (FUNCTION_CLASS::*)(FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N) FUNC_BASE_TEMPLATE_ARG_PARAMS_##N, CFuncMemPolicyRefCount<OBJECT_TYPE_PTR> >(pObject, pfnProxied FUNC_FUNCTOR_CALL_ARGS_##N); \
	}

FUNC_GENERATE_ALL( DEFINE_REF_COUNTING_MEMBER_FUNCTOR_FACTORY );

//-------------------------------------

#define DEFINE_REF_COUNTING_CONST_MEMBER_FUNCTOR_FACTORY(N) \
	template <typename OBJECT_TYPE_PTR, typename FUNCTION_CLASS, typename FUNCTION_RETTYPE FUNC_TEMPLATE_FUNC_PARAMS_##N FUNC_TEMPLATE_ARG_PARAMS_##N> \
	inline CFunctor *CreateRefCountingFunctor(OBJECT_TYPE_PTR pObject, FUNCTION_RETTYPE ( FUNCTION_CLASS::*pfnProxied )( FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N ) const FUNC_ARG_FORMAL_PARAMS_##N ) \
	{ \
		return new CMemberFunctor##N<OBJECT_TYPE_PTR, FUNCTION_RETTYPE (FUNCTION_CLASS::*)(FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N) const FUNC_BASE_TEMPLATE_ARG_PARAMS_##N, CFuncMemPolicyRefCount<OBJECT_TYPE_PTR> >(pObject, pfnProxied FUNC_FUNCTOR_CALL_ARGS_##N); \
	}

FUNC_GENERATE_ALL( DEFINE_REF_COUNTING_CONST_MEMBER_FUNCTOR_FACTORY );

#define DEFINE_FUNCTOR_DATA_FACTORY(N) \
	template < FUNC_SOLO_TEMPLATE_ARG_PARAMS_##N > \
	inline CFunctorData *CreateFunctorData( FUNC_PROXY_ARG_FORMAL_PARAMS_##N ) \
	{ \
		return new CFunctorData##N< FUNC_SOLO_BASE_TEMPLATE_ARG_PARAMS_##N >( FUNC_CALL_ARGS_##N ); \
	}

inline CFunctorData *CreateFunctorData()
{
	return new CFunctorData0(); 
}

FUNC_GENERATE_ALL_BUT0( DEFINE_FUNCTOR_DATA_FACTORY );

#define DEFINE_FUNCTOR_CALLBACK_FACTORY(N) \
	template < FUNC_SOLO_TEMPLATE_ARG_PARAMS_##N > \
	inline CFunctorCallback *CreateFunctorCallback( void (*pfnProxied)( FUNC_PROXY_ARG_FORMAL_PARAMS_##N ) ) \
	{ \
		return new CFunctorCallback##N< FUNC_SOLO_BASE_TEMPLATE_ARG_PARAMS_##N >( pfnProxied ); \
	}

inline CFunctorCallback *CreateFunctorCallback( void (*pfnProxied)() )
{
	return new CFunctorCallback0( pfnProxied );
}

FUNC_GENERATE_ALL_BUT0( DEFINE_FUNCTOR_CALLBACK_FACTORY );

#define DEFINE_MEMBER_FUNCTOR_CALLBACK_FACTORY(N) \
	template < typename FUNCTION_CLASS FUNC_TEMPLATE_ARG_PARAMS_##N > \
	inline CFunctorCallback *CreateFunctorCallback( FUNCTION_CLASS *pObject, void ( FUNCTION_CLASS::*pfnProxied )( FUNC_PROXY_ARG_FORMAL_PARAMS_##N ) ) \
	{ \
		return new CMemberFunctorCallback##N< FUNCTION_CLASS FUNC_BASE_TEMPLATE_ARG_PARAMS_##N >( pObject, pfnProxied ); \
	}

FUNC_GENERATE_ALL( DEFINE_MEMBER_FUNCTOR_CALLBACK_FACTORY );


//-----------------------------------------------------------------------------
//
// Templates to assist early-out direct call code
//
//-----------------------------------------------------------------------------

#define DEFINE_NONMEMBER_FUNCTOR_DIRECT(N) \
	template <typename FUNCTION_RETTYPE FUNC_TEMPLATE_FUNC_PARAMS_##N FUNC_TEMPLATE_ARG_PARAMS_##N> \
	inline void FunctorDirectCall(FUNCTION_RETTYPE (*pfnProxied)( FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N ) FUNC_ARG_FORMAL_PARAMS_##N ) \
{ \
	(*pfnProxied)( FUNC_CALL_ARGS_##N ); \
}

FUNC_GENERATE_ALL( DEFINE_NONMEMBER_FUNCTOR_DIRECT );


//-------------------------------------

#define DEFINE_MEMBER_FUNCTOR_DIRECT(N) \
	template <typename OBJECT_TYPE_PTR, typename FUNCTION_CLASS, typename FUNCTION_RETTYPE FUNC_TEMPLATE_FUNC_PARAMS_##N FUNC_TEMPLATE_ARG_PARAMS_##N> \
	inline void FunctorDirectCall(OBJECT_TYPE_PTR pObject, FUNCTION_RETTYPE ( FUNCTION_CLASS::*pfnProxied )( FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N ) FUNC_ARG_FORMAL_PARAMS_##N ) \
{ \
	((*pObject).*pfnProxied)(FUNC_CALL_ARGS_##N); \
}

FUNC_GENERATE_ALL( DEFINE_MEMBER_FUNCTOR_DIRECT );

//-------------------------------------

#define DEFINE_CONST_MEMBER_FUNCTOR_DIRECT(N) \
	template <typename OBJECT_TYPE_PTR, typename FUNCTION_CLASS, typename FUNCTION_RETTYPE FUNC_TEMPLATE_FUNC_PARAMS_##N FUNC_TEMPLATE_ARG_PARAMS_##N> \
	inline void FunctorDirectCall(OBJECT_TYPE_PTR pObject, FUNCTION_RETTYPE ( FUNCTION_CLASS::*pfnProxied )( FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N ) const FUNC_ARG_FORMAL_PARAMS_##N ) \
{ \
	((*pObject).*pfnProxied)(FUNC_CALL_ARGS_##N); \
}

FUNC_GENERATE_ALL( DEFINE_CONST_MEMBER_FUNCTOR_DIRECT );

#include "tier0/memdbgoff.h"

//-----------------------------------------------------------------------------
// Factory class useable as templated traits
//-----------------------------------------------------------------------------

class CDefaultFunctorFactory
{
public:
	FUNC_GENERATE_ALL( DEFINE_NONMEMBER_FUNCTOR_FACTORY );
	FUNC_GENERATE_ALL( DEFINE_MEMBER_FUNCTOR_FACTORY );
	FUNC_GENERATE_ALL( DEFINE_CONST_MEMBER_FUNCTOR_FACTORY );
	FUNC_GENERATE_ALL( DEFINE_REF_COUNTING_MEMBER_FUNCTOR_FACTORY );
	FUNC_GENERATE_ALL( DEFINE_REF_COUNTING_CONST_MEMBER_FUNCTOR_FACTORY );
};

template <class CAllocator, class CCustomFunctorBase = CFunctorBase>
class CCustomizedFunctorFactory
{
public:
	void SetAllocator( CAllocator *pAllocator )
	{
		m_pAllocator = pAllocator;
	}
	
	#define DEFINE_NONMEMBER_FUNCTOR_FACTORY_CUSTOM(N) \
		template <typename FUNCTION_RETTYPE FUNC_TEMPLATE_FUNC_PARAMS_##N FUNC_TEMPLATE_ARG_PARAMS_##N> \
		inline CFunctor *CreateFunctor( FUNCTION_RETTYPE (*pfnProxied)( FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N ) FUNC_ARG_FORMAL_PARAMS_##N ) \
		{ \
			typedef FUNCTION_RETTYPE (*Func_t)(FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N); \
			return new (m_pAllocator->Alloc( sizeof(CFunctor##N<Func_t FUNC_BASE_TEMPLATE_ARG_PARAMS_##N, CCustomFunctorBase>) )) CFunctor##N<Func_t FUNC_BASE_TEMPLATE_ARG_PARAMS_##N, CCustomFunctorBase>( pfnProxied FUNC_FUNCTOR_CALL_ARGS_##N ); \
		}

	FUNC_GENERATE_ALL( DEFINE_NONMEMBER_FUNCTOR_FACTORY_CUSTOM );

	//-------------------------------------

	#define DEFINE_MEMBER_FUNCTOR_FACTORY_CUSTOM(N) \
		template <typename OBJECT_TYPE_PTR, typename FUNCTION_CLASS, typename FUNCTION_RETTYPE FUNC_TEMPLATE_FUNC_PARAMS_##N FUNC_TEMPLATE_ARG_PARAMS_##N> \
		inline CFunctor *CreateFunctor(OBJECT_TYPE_PTR pObject, FUNCTION_RETTYPE ( FUNCTION_CLASS::*pfnProxied )( FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N ) FUNC_ARG_FORMAL_PARAMS_##N ) \
		{ \
		return new (m_pAllocator->Alloc( sizeof(CMemberFunctor##N<OBJECT_TYPE_PTR, FUNCTION_RETTYPE (FUNCTION_CLASS::*)(FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N) FUNC_BASE_TEMPLATE_ARG_PARAMS_##N, CCustomFunctorBase>) )) CMemberFunctor##N<OBJECT_TYPE_PTR, FUNCTION_RETTYPE (FUNCTION_CLASS::*)(FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N) FUNC_BASE_TEMPLATE_ARG_PARAMS_##N, CCustomFunctorBase>(pObject, pfnProxied FUNC_FUNCTOR_CALL_ARGS_##N); \
		}

	FUNC_GENERATE_ALL( DEFINE_MEMBER_FUNCTOR_FACTORY_CUSTOM );

	//-------------------------------------

	#define DEFINE_CONST_MEMBER_FUNCTOR_FACTORY_CUSTOM(N) \
		template <typename OBJECT_TYPE_PTR, typename FUNCTION_CLASS, typename FUNCTION_RETTYPE FUNC_TEMPLATE_FUNC_PARAMS_##N FUNC_TEMPLATE_ARG_PARAMS_##N> \
		inline CFunctor *CreateFunctor( OBJECT_TYPE_PTR pObject, FUNCTION_RETTYPE ( FUNCTION_CLASS::*pfnProxied )( FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N ) const FUNC_ARG_FORMAL_PARAMS_##N ) \
		{ \
			return new (m_pAllocator->Alloc( sizeof(CMemberFunctor##N<OBJECT_TYPE_PTR, FUNCTION_RETTYPE (FUNCTION_CLASS::*)(FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N) const FUNC_BASE_TEMPLATE_ARG_PARAMS_##N, CCustomFunctorBase>) )) CMemberFunctor##N<OBJECT_TYPE_PTR, FUNCTION_RETTYPE (FUNCTION_CLASS::*)(FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N) const FUNC_BASE_TEMPLATE_ARG_PARAMS_##N, CCustomFunctorBase>(pObject, pfnProxied FUNC_FUNCTOR_CALL_ARGS_##N); \
		}

	FUNC_GENERATE_ALL( DEFINE_CONST_MEMBER_FUNCTOR_FACTORY_CUSTOM );

	//-------------------------------------

	#define DEFINE_REF_COUNTING_MEMBER_FUNCTOR_FACTORY_CUSTOM(N) \
		template <typename OBJECT_TYPE_PTR, typename FUNCTION_CLASS, typename FUNCTION_RETTYPE FUNC_TEMPLATE_FUNC_PARAMS_##N FUNC_TEMPLATE_ARG_PARAMS_##N> \
		inline CFunctor *CreateRefCountingFunctor( OBJECT_TYPE_PTR pObject, FUNCTION_RETTYPE ( FUNCTION_CLASS::*pfnProxied )( FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N ) FUNC_ARG_FORMAL_PARAMS_##N ) \
		{ \
			return new (m_pAllocator->Alloc( sizeof(CMemberFunctor##N<OBJECT_TYPE_PTR, FUNCTION_RETTYPE (FUNCTION_CLASS::*)(FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N) FUNC_BASE_TEMPLATE_ARG_PARAMS_##N, CCustomFunctorBase, CFuncMemPolicyRefCount<OBJECT_TYPE_PTR> >) )) CMemberFunctor##N<OBJECT_TYPE_PTR, FUNCTION_RETTYPE (FUNCTION_CLASS::*)(FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N) FUNC_BASE_TEMPLATE_ARG_PARAMS_##N, CCustomFunctorBase, CFuncMemPolicyRefCount<OBJECT_TYPE_PTR> >(pObject, pfnProxied FUNC_FUNCTOR_CALL_ARGS_##N); \
		}

	FUNC_GENERATE_ALL( DEFINE_REF_COUNTING_MEMBER_FUNCTOR_FACTORY_CUSTOM );

	//-------------------------------------

	#define DEFINE_REF_COUNTING_CONST_MEMBER_FUNCTOR_FACTORY_CUSTOM(N) \
		template <typename OBJECT_TYPE_PTR, typename FUNCTION_CLASS, typename FUNCTION_RETTYPE FUNC_TEMPLATE_FUNC_PARAMS_##N FUNC_TEMPLATE_ARG_PARAMS_##N> \
		inline CFunctor *CreateRefCountingFunctor( OBJECT_TYPE_PTR pObject, FUNCTION_RETTYPE ( FUNCTION_CLASS::*pfnProxied )( FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N ) const FUNC_ARG_FORMAL_PARAMS_##N ) \
		{ \
			return new (m_pAllocator->Alloc( sizeof(CMemberFunctor##N<OBJECT_TYPE_PTR, FUNCTION_RETTYPE (FUNCTION_CLASS::*)(FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N) const FUNC_BASE_TEMPLATE_ARG_PARAMS_##N, CCustomFunctorBase, CFuncMemPolicyRefCount<OBJECT_TYPE_PTR> >) )) CMemberFunctor##N<OBJECT_TYPE_PTR, FUNCTION_RETTYPE (FUNCTION_CLASS::*)(FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N) const FUNC_BASE_TEMPLATE_ARG_PARAMS_##N, CCustomFunctorBase, CFuncMemPolicyRefCount<OBJECT_TYPE_PTR> >(pObject, pfnProxied FUNC_FUNCTOR_CALL_ARGS_##N); \
		}

	FUNC_GENERATE_ALL( DEFINE_REF_COUNTING_CONST_MEMBER_FUNCTOR_FACTORY_CUSTOM );

private:
	CAllocator *m_pAllocator;
	
};

//-----------------------------------------------------------------------------

#endif // FUNCTORS_H
