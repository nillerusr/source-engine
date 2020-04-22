//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "actbusydoc.h"
#include "datamodel/dmelement.h"
#include "actbusytool.h"


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CActBusyDoc::CActBusyDoc( IActBusyDocCallback *pCallback ) : m_pCallback( pCallback )
{
	m_hRoot = NULL;
	m_pFileName[0] = 0;
	m_bDirty = false;
	g_pDataModel->InstallNotificationCallback( this );
}

CActBusyDoc::~CActBusyDoc()
{
	g_pDataModel->RemoveNotificationCallback( this );
}


//-----------------------------------------------------------------------------
// Inherited from INotifyUI
//-----------------------------------------------------------------------------
void CActBusyDoc::NotifyDataChanged( const char *pReason, int nNotifySource, int nNotifyFlags )
{
	OnDataChanged( pReason, nNotifySource, nNotifyFlags );
}

	
//-----------------------------------------------------------------------------
// Gets the file name
//-----------------------------------------------------------------------------
const char *CActBusyDoc::GetFileName()
{
	return m_pFileName;
}

void CActBusyDoc::SetFileName( const char *pFileName )
{
	Q_strncpy( m_pFileName, pFileName, sizeof( m_pFileName ) );
	SetDirty( true );
}


//-----------------------------------------------------------------------------
// Dirty bits
//-----------------------------------------------------------------------------
void CActBusyDoc::SetDirty( bool bDirty )
{
	m_bDirty = bDirty;
}

bool CActBusyDoc::IsDirty() const
{
	return m_bDirty;
}


//-----------------------------------------------------------------------------
// Creates a new act busy
//-----------------------------------------------------------------------------
void CActBusyDoc::CreateNew()
{
	Assert( !m_hRoot.Get() );

	// This is not undoable
	CDisableUndoScopeGuard guard;

	Q_strncpy( m_pFileName, "untitled", sizeof( m_pFileName ) );
	DmFileId_t fileid = g_pDataModel->FindOrCreateFileId( m_pFileName );

	// Create the main element
	m_hRoot = g_pDataModel->CreateElement( "DmElement", "ActBusyList", fileid );
	if ( m_hRoot == DMELEMENT_HANDLE_INVALID )
		return;

	g_pDataModel->SetFileRoot( fileid, m_hRoot );

	// Each act busy list needs to have an editortype associated with it so it displays nicely in editors
	m_hRoot->SetValue( "editorType", "actBusyList" );
	m_hRoot->AddAttribute( "children", AT_ELEMENT_ARRAY );

	SetDirty( false );
}


//-----------------------------------------------------------------------------
// Saves/loads from file
//-----------------------------------------------------------------------------
bool CActBusyDoc::LoadFromFile( const char *pFileName )
{
	Assert( !m_hRoot.Get() );

	SetDirty( false );
	m_hRoot = NULL;

	Q_strncpy( m_pFileName, pFileName, sizeof( m_pFileName ) );
	if ( !m_pFileName[0] )
		return false;

	// This is not undoable
	CDisableUndoScopeGuard guard;

	CDmElement *root = NULL;
	g_pDataModel->RestoreFromFile( m_pFileName, NULL, "actbusy", &root );
	m_hRoot = root;
	OnDataChanged( "CActBusyDoc::LoadFromFile", NOTIFY_SOURCE_APPLICATION, NOTIFY_CHANGE_TOPOLOGICAL );
	SetDirty( false );
	return true;
}

void CActBusyDoc::SaveToFile( )
{
	if ( m_hRoot.Get() && m_pFileName && m_pFileName[0] )
	{
		g_pDataModel->SaveToFile( m_pFileName, NULL, "keyvalues", "actbusy", m_hRoot );
	}

	SetDirty( false );
}


//-----------------------------------------------------------------------------
// Creates a new act busy
//-----------------------------------------------------------------------------
void CActBusyDoc::CreateActBusy()
{
	CDmElement *pRoot = GetRootObject();
	if ( !pRoot )
		return;

	// This is undoable
	CAppUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, "Add ActBusy", "Add ActBusy" );

	DmFileId_t fileid = g_pDataModel->FindOrCreateFileId( m_pFileName );

	// Create the main element
	CDmeHandle<CDmElement> hActBusy = g_pDataModel->CreateElement( "DmElement", "ActBusy", fileid );
	if ( hActBusy == DMELEMENT_HANDLE_INVALID )
		return;

	hActBusy->SetValue( "editorType", "actBusy" );
	hActBusy->SetValue( "busy_anim", "" );
	hActBusy->SetValue( "entry_anim", "" );
	hActBusy->SetValue( "exit_anim", "" );
	hActBusy->SetValue( "busy_sequence", "" );
	hActBusy->SetValue( "entry_sequence", "" );
	hActBusy->SetValue( "exit_sequence", "" );
	hActBusy->SetValue( "min_time", 0.0f );
	hActBusy->SetValue( "max_time", 0.0f );
	hActBusy->SetValue( "interrupts", "BA_INT_NONE" );

	CDmrElementArray<> children( pRoot, "children" );
	children.AddToTail( hActBusy );
}


//-----------------------------------------------------------------------------
// Returns the root object
//-----------------------------------------------------------------------------
CDmElement *CActBusyDoc::GetRootObject()
{
	return m_hRoot;
}

	
//-----------------------------------------------------------------------------
// Called when data changes
//-----------------------------------------------------------------------------
void CActBusyDoc::OnDataChanged( const char *pReason, int nNotifySource, int nNotifyFlags )
{
	SetDirty( nNotifyFlags & NOTIFY_SETDIRTYFLAG ? true : false );
	m_pCallback->OnDocChanged( pReason, nNotifySource, nNotifyFlags );
}
