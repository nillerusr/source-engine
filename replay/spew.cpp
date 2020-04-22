//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "spew.h"
#include "dbg.h"
#include "strtools.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

class CBlockSpewer : public ISpewer
{
	virtual void PrintBlockStart() const
	{
		Log( "\n********************************************************************************\n*\n" );
	}

	virtual void PrintBlockEnd() const
	{
		Log( "*\n********************************************************************************\n\n" );
	}

	virtual void PrintEmptyLine() const
	{
		Log( "*\n" );
	}

	virtual void PrintEventStartMsg( const char *pMsg ) const
	{
		char pDots[] = { "............................................." };
		const int nNumDots = MAX( 3, V_strlen( pDots ) - V_strlen( pMsg ) );
		pDots[ nNumDots ] = '\0';
		Log( "*      %s%s", pMsg, pDots );
	}

	virtual void PrintEventResult( bool bSuccess ) const
	{
		Log( "%s\n", bSuccess ? "OK" : "FAILED" );
	}

	virtual void PrintEventError( const char *pError ) const
	{
		Log( "*\n*\n*         ** ERROR: %s\n*\n", pError );
	}

	virtual void PrintTestHeader( const char *pHeader ) const
	{
		Log( "*\n*\n*   %s...\n*\n", pHeader );
	}

	virtual void PrintValue( const char *pWhat, const char *pValue ) const
	{
		char pSpaces[] = { "                  " };
		const int nNumSpaces = MAX( 3, V_strlen( pSpaces ) - V_strlen( pWhat ) );
		pSpaces[ nNumSpaces ] = '\0';
		Log( "*      %s: %s%s\n", pWhat, pSpaces, pValue );
	}

	virtual void PrintMsg(  const char *pMsg ) const
	{
		Log( "*   %s\n", pMsg );
	}
};

//----------------------------------------------------------------------------------------

static CBlockSpewer s_BlockSpewer;
ISpewer *g_pBlockSpewer = &s_BlockSpewer;

//----------------------------------------------------------------------------------------

class CNullSpewer : public ISpewer
{
public:
	virtual void PrintBlockStart() const {}
	virtual void PrintBlockEnd() const {}
	virtual void PrintEmptyLine() const {}
	virtual void PrintEventStartMsg( const char *pMsg ) const {}
	virtual void PrintEventResult( bool bSuccess ) const {}
	virtual void PrintTestHeader( const char *pHeader ) const {}
	virtual void PrintMsg( const char *pMsg ) const {}
	virtual void PrintValue( const char *pWhat, const char *pValue ) const {}

	virtual void PrintEventError( const char *pError ) const
	{
		Log( "\n\nERROR: %s\n\n", pError );
	}
};

//----------------------------------------------------------------------------------------

static CNullSpewer s_NullSpewer;
ISpewer *g_pNullSpewer = &s_NullSpewer;

//----------------------------------------------------------------------------------------

class CSimpleSpewer : public CNullSpewer
{
public:
	virtual void PrintMsg( const char *pMsg ) const
	{
		Log( "%s", pMsg );
	}
};

//----------------------------------------------------------------------------------------

static CSimpleSpewer s_SimpleSpewer;
ISpewer *g_pSimpleSpewer = &s_SimpleSpewer;

//----------------------------------------------------------------------------------------

ISpewer *g_pDefaultSpewer = g_pNullSpewer;

//----------------------------------------------------------------------------------------

CBaseSpewer::CBaseSpewer( ISpewer *pSpewer/*=g_pDefaultSpewer*/ )
:	m_pSpewer( pSpewer )
{
}

//----------------------------------------------------------------------------------------
