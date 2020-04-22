//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "undomanager.h"
#include "datamodel.h"

extern CDataModel *g_pDataModelImp;

CUtlSymbolTableMT CUndoManager::s_UndoSymbolTable;


CUndoManager::CUndoManager( ) :
	m_bEnabled( true ),
	m_bDiscarded( false ),
	m_nMaxUndoDepth( 4096 ),
	m_nNesting( 0 ),
	m_nNotifyNesting( 0 ),
	m_bStreamStart( false ),
	m_bTrace( false ),
	m_bSuppressingNotify( false ),
	m_nItemsAddedSinceStartOfStream( 0 ),
	m_nNotifySource( 0 ),
	m_nNotifyFlags( 0 ),
	m_nChainingID( 0 ),
	m_PreviousChainingID( 0 )
{
}

CUndoManager::~CUndoManager()
{
}

void CUndoManager::Shutdown()
{
	WipeUndo();
	WipeRedo();
}

bool CUndoManager::InstallNotificationCallback( IDmNotify *pNotify )
{
	if ( m_Notifiers.Find( pNotify ) >= 0 )
		return false;
	m_Notifiers.AddToTail( pNotify );
	return true;
}

void CUndoManager::RemoveNotificationCallback( IDmNotify *pNotify )
{
	m_Notifiers.FindAndRemove( pNotify );
}

bool CUndoManager::IsSuppressingNotify( ) const
{
	return m_bSuppressingNotify;
}

void CUndoManager::SetSuppressingNotify( bool bSuppress )
{
	m_bSuppressingNotify = bSuppress;
}

void CUndoManager::Trace( const char *fmt, ... )
{
	if ( !m_bTrace )
		return;

	char str[ 2048 ];
	va_list argptr;
	va_start( argptr, fmt );
	_vsnprintf( str, sizeof( str ) - 1, fmt, argptr );
	va_end( argptr );
	str[ sizeof( str ) - 1 ] = 0;

	char spaces[ 128 ];
	Q_memset( spaces, 0, sizeof( spaces ) );
	for ( int i = 0; i < ( m_nNesting * 3 ); ++i )
	{
		if ( i >= 127 )
			break;
		spaces[ i ] = ' ';
	}

	Msg( "%s%s", spaces, str );
}

void CUndoManager::SetUndoDepth( int nMaxUndoDepth )
{
	Assert( !HasUndoData() );
	m_nMaxUndoDepth = nMaxUndoDepth;
}

void CUndoManager::EnableUndo()
{
	m_bEnabled = true;
}

void CUndoManager::DisableUndo()
{
	m_bEnabled = false;
}

bool CUndoManager::HasUndoData() const
{
	return m_UndoList.Count() != 0;
}

bool CUndoManager::UndoDataDiscarded() const
{
	return m_bDiscarded;
}

bool CUndoManager::HasRedoData() const
{
	return m_RedoStack.Count() > 0;
}

void CUndoManager::PushNotificationScope( const char *pReason, int nNotifySource, int nNotifyFlags )
{
	if ( m_nNotifyNesting++ == 0 )
	{
		m_pNotifyReason = pReason;
		m_nNotifySource = nNotifySource;
		m_nNotifyFlags = nNotifyFlags;
	}
}

void CUndoManager::PopNotificationScope( bool bAbort )
{
	--m_nNotifyNesting;
	Assert( m_nNotifyNesting >= 0 );
	if ( m_nNotifyNesting == 0 )
	{
		if ( !m_bSuppressingNotify && ( ( m_nNotifyFlags & NOTIFY_CHANGE_MASK ) != 0 ) )
		{
			int nNotifyCount = m_Notifiers.Count();
			for( int i = 0; i < nNotifyCount; ++i )
			{
				m_Notifiers[i]->NotifyDataChanged( m_pNotifyReason, m_nNotifySource, m_nNotifyFlags );
			}
		}
		m_nNotifySource = 0;
		m_nNotifyFlags = 0;
	}
}



void CUndoManager::PushUndo( const char *udesc, const char *rdesc, int nChainingID )
{
	if ( !IsEnabled() )
		return;

	Trace( "[%d] Pushing undo '%s'\n", m_nNesting + 1, udesc );

	if ( m_nNesting++ == 0 )
	{
		m_PreviousChainingID = m_nChainingID;
		m_nChainingID = nChainingID;
		m_UndoDesc = s_UndoSymbolTable.AddString( udesc );
		m_RedoDesc = ( udesc == rdesc ) ? m_UndoDesc : s_UndoSymbolTable.AddString( rdesc );
		m_bStreamStart = true;
		m_nItemsAddedSinceStartOfStream = 0;
	}
}

void CUndoManager::PushRedo()
{
	if ( !IsEnabled() )
		return;

	Trace( "[%d] Popping undo '%s'\n", m_nNesting, s_UndoSymbolTable.String( m_UndoDesc ) );

	--m_nNesting;
	Assert( m_nNesting >= 0 );
	if ( m_nNesting == 0 )
	{
		if ( m_nItemsAddedSinceStartOfStream > 0 )
		{
			WipeRedo();

			// Accumulate this operation into the previous "undo" operation if there is one
			if ( m_nChainingID != 0 &&
				m_PreviousChainingID == m_nChainingID )
			{
				// Walk undo list backward looking for previous end of stream and unmark that indicator
				int i = m_UndoList.Tail();
				while ( i != m_UndoList.InvalidIndex() )
				{
					IUndoElement *e = m_UndoList[ i ];
					if ( e && e->IsEndOfStream() )
					{
						e->SetEndOfStream( false );
						break;
					}
					i = m_UndoList.Previous( i );
				}
			}
		}

		m_nItemsAddedSinceStartOfStream = 0;
	}
}

void CUndoManager::AbortUndoableOperation()
{
	if ( !IsEnabled() )
		return;

	bool hasItems = m_nItemsAddedSinceStartOfStream > 0 ? true : false;
	
	Trace( "[%d] Aborting undo '%s'\n", m_nNesting, s_UndoSymbolTable.String( m_UndoDesc )  );

	// Close off context
	PushRedo();

	if ( m_nNesting == 0 && hasItems )
	{
		Undo();
		WipeRedo();
	}
}

void CUndoManager::WipeUndo()
{
	CDisableUndoScopeGuard sg;

	FOR_EACH_LL( m_UndoList, elem )
	{
		Trace( "WipeUndo '%s'\n", m_UndoList[ elem ]->GetDesc() );

		m_UndoList[ elem ]->Release();
	}
	m_UndoList.RemoveAll();
	m_PreviousChainingID = 0;
}

void CUndoManager::WipeRedo()
{
	int c = m_RedoStack.Count();
	if ( c == 0 )
		return;

	CUtlVector< DmElementHandle_t > handles;
	g_pDataModelImp->GetInvalidHandles( handles );
	g_pDataModelImp->MarkHandlesValid( handles );

	CDisableUndoScopeGuard sg;

	for ( int i = 0; i < c ; ++i )
	{
		IUndoElement *elem;
		elem = m_RedoStack[ i ];
		
		Trace( "WipeRedo '%s'\n", elem->GetDesc() );

		elem->Release();
	}

	m_RedoStack.Clear();

	g_pDataModelImp->MarkHandlesInvalid( handles );
}

void CUndoManager::AddUndoElement( IUndoElement *pElement )
{
	Assert( IsEnabled() );

	if ( !pElement )
		return;

	++m_nItemsAddedSinceStartOfStream;

	WipeRedo();

	/*
	// For later
	if ( m_UndoList.Count() >= m_nMaxUndos )
	{
		m_bDiscarded = true;
	}
	*/
	
	Trace( "AddUndoElement '%s'\n", pElement->GetDesc() );

	m_UndoList.AddToTail( pElement );

	if ( m_bStreamStart )
	{
		pElement->SetEndOfStream( true );
		m_bStreamStart = false;
	}
}

void CUndoManager::Undo()
{
	CNotifyScopeGuard notify( "CUndoManager::Undo", NOTIFY_SOURCE_UNDO, NOTIFY_SETDIRTYFLAG );

	Trace( "Undo\n======\n" );

	bool saveEnabled = m_bEnabled;
	m_bEnabled = false;
	bool bEndOfStream = false;
	while ( !bEndOfStream && m_UndoList.Count() > 0 )
	{
		int i = m_UndoList.Tail();
		IUndoElement *action = m_UndoList[ i ];
		Assert( action );

		Trace( "  %s\n", action->GetDesc() );

		action->Undo();
		m_RedoStack.Push( action );
		bEndOfStream = action->IsEndOfStream();
		m_UndoList.Remove( i );
	}

	Trace( "======\n\n" );

	m_bEnabled = saveEnabled;
	m_PreviousChainingID = 0;
}

void CUndoManager::Redo()
{
	CNotifyScopeGuard notify( "CUndoManager::Redo", NOTIFY_SOURCE_UNDO, NOTIFY_SETDIRTYFLAG );

	Trace( "Redo\n======\n" );

	bool saveEnabled = m_bEnabled;
	m_bEnabled = false;
	bool bEndOfStream = false;
	while ( !bEndOfStream && m_RedoStack.Count() > 0 )
	{
		IUndoElement *action = NULL;
		m_RedoStack.Pop( action );
		Assert( action );

		Trace( "  %s\n", action->GetDesc() );

		action->Redo();
		m_UndoList.AddToTail( action );
		if ( m_RedoStack.Count() > 0 )
		{
			action = m_RedoStack.Top();
			bEndOfStream = action->IsEndOfStream();
		}
	}

	Trace( "======\n\n" );

	m_bEnabled = saveEnabled;
	m_PreviousChainingID = 0;
}

const char *CUndoManager::UndoDesc() const
{
	if ( m_UndoList.Count() <= 0 )
		return "";

	int i = m_UndoList.Tail();
	IUndoElement *action = m_UndoList[ i ];
	return action->UndoDesc();
}

const char *CUndoManager::RedoDesc() const
{
	if ( m_RedoStack.Count() <= 0 )
	{
		return "";
	}

	IUndoElement *action = m_RedoStack.Top();
	return action->RedoDesc();
}

UtlSymId_t CUndoManager::GetUndoDescInternal( const char *context )
{
	if ( m_nNesting <= 0 )
	{
		static CUtlSymbolTable s_DescErrorsTable;
		static CUtlVector< CUtlSymbol >	s_DescErrors;
		CUtlSymbol sym = s_DescErrorsTable.AddString( context );
		if ( s_DescErrors.Find( sym ) == s_DescErrors.InvalidIndex() )
		{
			Warning( "CUndoManager::GetUndoDescInternal:  undoable operation missing CUndoScopeGuard in application\nContext( %s )\n", context );
			s_DescErrors.AddToTail( sym );
		}
		return s_UndoSymbolTable.AddString( context );
	}
	return m_UndoDesc;
}

UtlSymId_t CUndoManager::GetRedoDescInternal( const char *context )
{
	if ( m_nNesting <= 0 )
	{
		// Warning( "CUndoManager::GetRedoDescInternal:  undoable operation missing CUndoScopeGuard in application\nContext( %s )", context );
		return s_UndoSymbolTable.AddString( context );
	}
	return m_RedoDesc;
}

void CUndoManager::GetUndoInfo( CUtlVector< UndoInfo_t >& list )
{
	// Needs to persist after function returns...
	static CUtlSymbolTable table;

	int ops = 0;
	for ( int i = m_UndoList.Tail(); i != m_UndoList.InvalidIndex(); i = m_UndoList.Previous( i ) )
	{
		++ops;
		IUndoElement *action = m_UndoList[ i ];
		Assert( action );
		bool bEndOfStream = action->IsEndOfStream();

		UndoInfo_t info;
		info.undo = action->UndoDesc();
		info.redo = action->RedoDesc();

		// This is a hack because GetDesc() returns a static char buf[] and so the last one will clobber them all
		// So we have the requester pass in a temporary string table so we can get a ptr to a CUtlSymbol in the table
		// and use that.  Sigh.
		const char *desc = action->GetDesc();
		CUtlSymbol sym = table.AddString( desc );
		info.desc = table.String( sym );
		info.terminator = bEndOfStream;
		info.numoperations = bEndOfStream ? ops : 1;

		list.AddToTail( info );

		if ( bEndOfStream )
		{
			ops = 0;
		}
	}
}

void CUndoManager::TraceUndo( bool state )
{
	m_bTrace = state;
}
