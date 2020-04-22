//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef UNDOMANAGER_H
#define UNDOMANAGER_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlsymbol.h"
#include "tier1/utllinkedlist.h"
#include "tier1/utlstack.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IUndoElement;
struct UndoInfo_t;
class IDmNotify;


//-----------------------------------------------------------------------------
// Undo/Redo stuff:
//-----------------------------------------------------------------------------
class CUndoManager
{
public:
	CUndoManager();
	~CUndoManager();

	void			Shutdown();
	void			SetUndoDepth( int nMaxUndoDepth );
	void			AddUndoElement( IUndoElement *pElement );
	void			Undo();
	void			Redo();
	void			TraceUndo( bool state );

	void			EnableUndo();
	void			DisableUndo();
	bool			IsEnabled() const;
	bool			HasUndoData() const;
	bool			UndoDataDiscarded() const;
	bool			HasRedoData() const;

	void			WipeUndo();
	void			WipeRedo();

	const char		*UndoDesc() const;
	const char		*RedoDesc() const;

	void			PushUndo( char const *udesc, char const *rdesc, int nChainingID );
	void			PushRedo();
	void			AbortUndoableOperation();

	UtlSymId_t		GetUndoDescInternal( const char *context );
	UtlSymId_t		GetRedoDescInternal( const char *context );

	void			GetUndoInfo( CUtlVector< UndoInfo_t >& list );

	bool			InstallNotificationCallback( IDmNotify *pNotify );
	void			RemoveNotificationCallback( IDmNotify *pNotify );
	bool			IsSuppressingNotify( ) const;
	void			SetSuppressingNotify( bool bSuppress );
	void			PushNotificationScope( const char *pReason, int nNotifySource, int nNotifyFlags );
	void			PopNotificationScope( bool bAbort );

	void			NotifyState( int nNotifyFlags );

	static const char *GetUndoString( CUtlSymbol sym );

private:
	void			Trace( PRINTF_FORMAT_STRING const char *fmt, ... );

	CUtlLinkedList< IUndoElement *, int >	m_UndoList;
	CUtlStack< IUndoElement * >			m_RedoStack;
	CUtlVector< IDmNotify* >			m_Notifiers;
	int				m_nMaxUndoDepth;
	int				m_nNesting;
	int				m_nNotifyNesting;
	CUtlSymbol		m_UndoDesc;
	CUtlSymbol		m_RedoDesc;
	int				m_nNotifySource;
	int				m_nNotifyFlags;
	const char*		m_pNotifyReason;
	int				m_nItemsAddedSinceStartOfStream;
	// Signals that next undo operation is the "Start" of a stream
	bool			m_bStreamStart : 1;
	bool			m_bTrace : 1;
	bool			m_bDiscarded : 1;
	bool			m_bEnabled : 1;
	bool			m_bSuppressingNotify : 1;

	static CUtlSymbolTableMT s_UndoSymbolTable;
	int				m_nChainingID;
	int				m_PreviousChainingID;
};


//-----------------------------------------------------------------------------
// Is undo enabled
//-----------------------------------------------------------------------------
inline bool CUndoManager::IsEnabled() const
{
	return m_bEnabled;
}

inline void CUndoManager::NotifyState( int nNotifyFlags )
{
	// FIXME: Should suppress prevent notification being sent,
	// or prevent notification flags from being set in the first place?
	m_nNotifyFlags |= nNotifyFlags;
}

inline const char *CUndoManager::GetUndoString( CUtlSymbol sym )
{
	return s_UndoSymbolTable.String( sym );
}


#endif // UNDOMANAGER_H
