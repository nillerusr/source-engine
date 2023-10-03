//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// work in progress
//
//===============================================================================

#include "cbase.h"
#include "global_event_log.h"
#include "filesystem.h"
#include "utlbuffer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


CGlobalEventLog	GlobalEventLog;

static	CUtlSymbolTable	EventSymbols;
static	ConVar			global_event_log_enabled( "global_event_log_enabled", "0", FCVAR_CHEAT, "Enables the global event log system" );

class CGlobalEventLine
{
public:
	CGlobalEventLine( );
	~CGlobalEventLine( );

	void	Clear( );
	bool	SetStaticText( const char *pszValue );
	bool	SetVaryingText( const char *pszValue );

	bool	IsDirty( ) { return m_bDirty; }

	void	Write( CUtlBuffer *pBuffer );
	void	ClearDirty( );		

private:
	CUtlSymbol	m_ValueSymbol;
	char		*m_pszValue;
	bool		m_bDirty;
};

CGlobalEventLine::CGlobalEventLine( )
{
	m_ValueSymbol = UTL_INVAL_SYMBOL;
	m_pszValue = NULL;
	m_bDirty = true;
}

CGlobalEventLine::~CGlobalEventLine( )
{
	Clear();
}

void CGlobalEventLine::Clear( )
{
	m_ValueSymbol = UTL_INVAL_SYMBOL;
	if ( m_pszValue != NULL )
	{
		delete m_pszValue;
		m_pszValue = NULL;
	}
	m_bDirty = true;
}

bool CGlobalEventLine::SetStaticText( const char *pszValue )
{
	if ( m_ValueSymbol != UTL_INVAL_SYMBOL && m_ValueSymbol == EventSymbols.Find( pszValue ) )
	{
		return false;
	}

	Clear();

	m_ValueSymbol = EventSymbols.AddString( pszValue );

	return true;
}

bool CGlobalEventLine::SetVaryingText( const char *pszValue )
{
	if ( m_pszValue && strcmpi( m_pszValue, pszValue ) == 0 )
	{	// no change
		return false;
	}

	Clear();

	m_pszValue = ( char * )malloc( strlen( pszValue ) + 1 );
	strcpy( m_pszValue, pszValue );

	return true;
}

void CGlobalEventLine::Write( CUtlBuffer *pBuffer )
{
	const char *pszValue;

	if ( m_pszValue != NULL )
	{
		pszValue = m_pszValue;
	}
	else
	{
		pszValue = EventSymbols.String( m_ValueSymbol );
	}

	if ( strchr( pszValue, ' ' ) != NULL )
	{
		pBuffer->Printf( "\"%s\"\n", pszValue );
	}
	else
	{
		pBuffer->Printf( "%s\n", pszValue );
	}
}

void CGlobalEventLine::ClearDirty( )
{
	m_bDirty = false;
}



class CGlobalEvent
{
public:
	CGlobalEvent( const char *pszName, unsigned int nID, bool bIsHighLevel, CGlobalEvent *pParent = NULL );

	bool			AddValue( bool bVarying, const char *pszKey, const char *pszValue );

	unsigned int	GetID( ) { return m_nID; }
	bool			IsDirty( ) { return m_bDirty; }

	void			Write( CUtlBuffer *pBuffer );
	void			ClearDirty( );		

private:
	unsigned int								m_nID;
	CUtlSymbol									m_Name;
	float										m_flTime;
	bool										m_bIsHighLevel;
	bool										m_bFullUpdate;
	bool										m_bDirty;
	CGlobalEvent								*m_pParent;
	CUtlMap< CUtlSymbol, CGlobalEventLine * >	m_EventLines;
};


CGlobalEvent::CGlobalEvent( const char *pszName, unsigned int nID, bool bIsHighLevel, CGlobalEvent *pParent ) :
	m_EventLines( DefLessFunc( const CUtlSymbol ) )
{
	m_Name = EventSymbols.AddString( pszName );
	m_nID = nID;
	m_bIsHighLevel = bIsHighLevel;
	m_pParent = pParent;
	m_bFullUpdate = true;
	m_flTime = gpGlobals->curtime;
}

bool CGlobalEvent::AddValue( bool bVarying, const char *pszKey, const char *pszValue )
{
	CUtlSymbol			KeyID;
	CGlobalEventLine	*pEvent;
	int					nIndex;
	bool				bResult;

	KeyID = EventSymbols.AddString( pszKey );
	nIndex = m_EventLines.Find( KeyID );
	if ( nIndex == m_EventLines.InvalidIndex() )
	{
		pEvent = new CGlobalEventLine();
		m_EventLines.Insert( KeyID, pEvent );
	}
	else
	{
		pEvent = m_EventLines.Element( nIndex );
	}

	if ( bVarying )
	{
		bResult = pEvent->SetVaryingText( pszValue );
	}
	else
	{
		bResult = pEvent->SetStaticText( pszValue );
	}

	if ( bResult == true )
	{
		m_bDirty = true;
		m_flTime = gpGlobals->curtime;
	}

	return bResult;
}

void CGlobalEvent::Write( CUtlBuffer *pBuffer )
{
	pBuffer->Printf( "event %u\n", m_nID );
	pBuffer->Printf( "{\n" );

	if ( m_bFullUpdate )
	{
		const char *pszName = EventSymbols.String( m_Name );

		if ( strchr( pszName, ' ' ) != NULL )
		{
			pBuffer->Printf( "\tName\t\"%s\"\n", pszName );
		}
		else
		{
			pBuffer->Printf( "\tName\t%s\n", pszName );
		}
		if ( m_pParent != NULL )
		{
			pBuffer->Printf( "\tParent_ID\t%u\n", m_pParent->GetID() );
		}
		if ( m_bIsHighLevel == true )
		{
			pBuffer->Printf( "\tHighLevel\t1\n" );
		}
	}

	pBuffer->Printf( "\tTime\t%g\n", m_flTime );

	for( unsigned i = 0; i < m_EventLines.Count(); i++ )
	{
		if ( m_EventLines.Element( i )->IsDirty() )
		{
			const char *pszKey = EventSymbols.String( m_EventLines.Key( i ) );

			if ( strchr( pszKey, ' ' ) != NULL )
			{
				pBuffer->Printf( "\t\"%s\"\t", pszKey );
			}
			else
			{
				pBuffer->Printf( "\t%s\t", pszKey );
			}
			m_EventLines.Element( i )->Write( pBuffer );
		}
	}
	pBuffer->Printf( "}\n" );
}

void CGlobalEvent::ClearDirty( )
{
	m_bFullUpdate = false;
	m_bDirty = false;
	for( unsigned i = 0; i < m_EventLines.Count(); i++ )
	{
		m_EventLines.Element( i )->ClearDirty();
	}
}


CGlobalEventLog::CGlobalEventLog( )
{
	m_nNextID = 1;
}

CGlobalEvent *CGlobalEventLog::GetGlobalEvent( EGlobalEvent GlobalEvent )
{
	return m_pGlobalEvents[ GlobalEvent ];
}

CGlobalEvent *CGlobalEventLog::CreateEvent( const char *pszName, bool bIsHighLevel, CGlobalEvent *pParent )
{
	CGlobalEvent	*pEvent = new CGlobalEvent( pszName, m_nNextID, bIsHighLevel, pParent );

	m_Events.AddToTail( pEvent );
	m_DirtyEvents.AddToTail( pEvent );
	m_nNextID++;

	return pEvent;
}

CGlobalEvent *CGlobalEventLog::CreateTempEvent( const char *pszName, CGlobalEvent *pParent )
{
	CGlobalEvent	*pEvent = new CGlobalEvent( pszName, m_nNextID, false, pParent );

	m_TempEvents.AddToTail( pEvent );
	m_DirtyEvents.AddToTail( pEvent );
	m_nNextID++;

	return pEvent;
}

void CGlobalEventLog::RemoveEvent( CGlobalEvent *pEvent )
{
	if ( m_Events.FindAndRemove( pEvent ) == true && m_TempEvents.Find( pEvent ) == -1 )
	{
		m_TempEvents.AddToTail( pEvent );
	}
}

void CGlobalEventLog::AddKeyValue( CGlobalEvent *pEvent, bool bVarying, const char *pszKey, const char *pszValueFormat, ... )
{
	va_list		Args;
	int			nLen;
	char		*pszBuffer;
	CUtlSymbol	KeyID;
	bool		bResult;

	va_start( Args, pszValueFormat );
	nLen = _vscprintf( pszValueFormat, Args ) + 1;
	pszBuffer = ( char * )stackalloc( nLen * sizeof( char ) );
	vsprintf_s( pszBuffer, nLen, pszValueFormat, Args );

	bResult = pEvent->AddValue( bVarying, pszKey, pszBuffer );

	if ( bResult == true && m_DirtyEvents.Find( pEvent ) == -1 )
	{
		m_DirtyEvents.AddToTail( pEvent );
	}
}

void CGlobalEventLog::SendUpdate( )
{
	if ( m_DirtyEvents.Count() == 0 )
	{
		return;
	}

	if ( global_event_log_enabled.GetBool() == true )
	{
		FileHandle_t fh = g_pFullFileSystem->Open( "c:\\o.events", "a" );
		CUtlBuffer	*pBuffer = new CUtlBuffer( 0, 0, CUtlBuffer::TEXT_BUFFER );

		pBuffer->Clear();

		for( int i = 0; i < m_DirtyEvents.Count(); i++ )
		{
			m_DirtyEvents[ i ]->Write( pBuffer );

			g_pFullFileSystem->Write( pBuffer->Base(), pBuffer->TellPut(), fh );
			pBuffer->Clear();
		}

		g_pFullFileSystem->Close( fh );
	}

	for( int i = 0; i < m_DirtyEvents.Count(); i++ )
	{
		m_DirtyEvents[ i ]->ClearDirty();
	}
	m_DirtyEvents.Purge();

	for( int i = 0; i < m_TempEvents.Count(); i++ )
	{
		delete m_TempEvents[ i ];
	}
	m_TempEvents.Purge();
}

void CGlobalEventLog::PostInit( )
{
	m_pGlobalEvents[ GLOBAL_EVENT_NPCS ] = CreateEvent( "NPCs", true );
}

void CGlobalEventLog::FrameUpdatePostEntityThink( )
{
	SendUpdate();
}
