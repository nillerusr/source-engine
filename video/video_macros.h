//========= Copyright Valve Corporation, All rights reserved. ============//
//
//  videomacros.h - Commone macros used by valve video services
//
//  Purpose - to save typing, make things cleaer, prep for c++0x, prep for 64-bit,
//            make lots of money, and drive Brian crazy
//
// ===========================================================================

#ifndef VIDEOMACROS_H
#define VIDEOMACROS_H

#ifdef _WIN32
	#pragma once
#endif

#include <tier0/dbg.h>

// ------------------------------------------------------------------------
// MACROS
// ------------------------------------------------------------------------

#define nullchar	( (char) 0x00 )
#ifndef nullptr
#define nullptr		( 0 )
#endif

#define ZeroVar( var )				V_memset( &var, nullchar, sizeof( var ) )
#define ZeroVarPtr( pVar )			V_memset( pVar, nullchar, sizeof( *pVar) ) 

#define SAFE_DELETE( var )			if ( var != nullptr ) { delete var; var = nullptr; }
#define SAFE_DELETE_ARRAY( var )	if ( var != nullptr ) { delete[] var; var = nullptr; }

#define SAFE_RELEASE( var )			if ( var != nullptr ) { var->Release(); var = nullptr; }
#define SAFE_FREE( var )			if ( var != nullptr ) { free( var ); var = nullptr; }

// Common Assert Patterns

#define AssertPtr( _exp )			Assert( ( _exp ) != nullptr )
#define AssertNull( _exp )			Assert( ( _exp ) == nullptr )
#define AssertStr( _str )			Assert( ( _str ) != nullptr && *( _str ) != nullchar )

#define AssertInRange( _exp, low, high )	Assert( ( _exp ) > ( low ) && ( _exp ) < ( high ) )
#define AssertIncRange( _exp, low, high )	Assert( ( _exp ) >= ( low ) && ( _exp ) <= ( high ) )

// AssertExit macros .. in release builds, or when Assert is disabled, they exit (w/ opt return value)
//                      if the assert condition is false
// 

#ifdef DBGFLAG_ASSERT

	#define AssertExit( _exp )				Assert( _exp )
	#define AssertExitV( _exp , _rval )		Assert( _exp )
	#define AssertExitF( _exp )				Assert( _exp )
	#define AssertExitN( _exp )				Assert( _exp )
	#define AssertExitFunc( _exp, _func )	Assert( _exp )
	
	#define AssertPtrExit( _exp )			Assert( ( _exp ) != nullptr )
	#define AssertPtrExitV( _exp , _rval )	Assert( ( _exp ) != nullptr )
	#define AssertPtrExitF( _exp )			Assert( ( _exp ) != nullptr )
	#define AssertPtrExitN( _exp )			Assert( ( _exp ) != nullptr )

	#define AssertInRangeExit( _exp , low , high )				Assert( ( _exp ) > ( low ) && ( _exp ) < ( high ) )
	#define AssertInRangeExitV( _exp , low , high, _rval )		Assert( ( _exp ) > ( low ) && ( _exp ) < ( high ) )
	#define AssertInRangeExitF( _exp , low , high )				Assert( ( _exp ) > ( low ) && ( _exp ) < ( high ) )
	#define AssertInRangeExitN( _exp , low , high )				Assert( ( _exp ) > ( low ) && ( _exp ) < ( high ) )


#else	// Asserts not enabled

	#define AssertExit( _exp )				if ( !( _exp ) ) return
	#define AssertExitV( _exp , _rval )		if ( !( _exp ) ) return _rval
	#define AssertExitF( _exp )				if ( !( _exp ) ) return false
	#define AssertExitN( _exp )				if ( !( _exp ) ) return nullptr
	#define AssertExitFunc( _exp, _func )	if ( !( _exp ) ) { _func; return; }
	
	#define AssertPtrExit( _exp )			if ( ( _exp ) == nullptr ) return
	#define AssertPtrExitV( _exp , _rval )	if ( ( _exp ) == nullptr ) return _rval
	#define AssertPtrExitF( _exp )			if ( ( _exp ) == nullptr  ) return false
	#define AssertPtrExitN( _exp )			if ( ( _exp ) == nullptr  ) return nullptr

	#define AssertInRangeExit( _exp, low, high )				if ( ( _exp ) <= ( low ) || ( _exp ) >= ( high ) ) return
	#define AssertInRangeExitV( _exp, low, high, _rval )		if ( ( _exp ) <= ( low ) || ( _exp ) >= ( high ) ) return _rval
	#define AssertInRangeExitF( _exp, low, high )				if ( ( _exp ) <= ( low ) || ( _exp ) >= ( high ) ) return false
	#define AssertInRangeExitN( _exp, low, high )				if ( ( _exp ) <= ( low ) || ( _exp ) >= ( high ) ) return nullptr

#endif

#define WarningAssert( _msg )		AssertMsg( false, _msg )


#define STRINGS_MATCH		0
#define IS_NOT_EMPTY( str )		( (str) != nullptr && *(str) != nullchar )
#define IS_EMPTY_STR( str )		( (str) == nullptr || *(str) == nullchar )

#define IS_IN_RANGE( var, low, high )		( (var) >= (low) && (var) <= (high) )
#define IS_IN_RANGECOUNT( var, low, high )		( (var) >= (low) && (var) < (high)  )

#define IS_OUT_OF_RANGE( var, low, high )	( (var) < (low) || (var) > (high) )
#define IS_OUT_OF_RANGECOUNT( var, low, high )	( (var) < (low) || (var) >= (high) )

#define BITFLAGS_SET( var, bits )		( ( (var) & (bits) ) == (bits) )
#define ANY_BITFLAGS_SET( var, bits )	( ( (var) & (bits) ) != 0 )

#define MAKE_UINT64( highVal, lowVal )  ( ( (uint64) highVal << 32 ) | (uint64) lowVal )

#define CONTAINING_MULTIPLE_OF( var, multVal )    ( (var) + ( multVal - 1) ) - ( ( (var) - 1) % multVal )

// use this whenever we do address arithmetic in bytes
typedef		unsigned char*		memaddr_t;
typedef		int32				memoffset_t;


#endif	// VIDEOMACROS_H