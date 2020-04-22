//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "bitbuf.h"
#include "bitbuf_errorhandler.h"
#include "tier0/dbg.h"
#include "utlsymbol.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void EngineBitBufErrorHandler( BitBufErrorType errorType, const char *pDebugName )
{
	if ( !pDebugName )
	{
		pDebugName = "(unknown)";
	}

	static CUtlSymbolTable errorNames[ BITBUFERROR_NUM_ERRORS ];

	// Only print an error a couple times.
	CUtlSymbol sym = errorNames[ errorType ].Find( pDebugName );
	if ( UTL_INVAL_SYMBOL == sym )
	{
		errorNames[ errorType ].AddString( pDebugName );
		if ( errorType == BITBUFERROR_VALUE_OUT_OF_RANGE )
		{
			Warning( "Error in bitbuf [%s]: out of range value. Debug in bitbuf_errorhandler.cpp\n", pDebugName );
		}
		else if ( errorType == BITBUFERROR_BUFFER_OVERRUN )
		{
			Warning( "Error in bitbuf [%s]: buffer overrun. Debug in bitbuf_errorhandler.cpp\n", pDebugName );
		}
	}

	AssertMsg( false, "%s: %s errorType: %d", __FUNCTION__, pDebugName, errorType );
}

void InstallBitBufErrorHandler()
{
	SetBitBufErrorHandler( EngineBitBufErrorHandler );
}

