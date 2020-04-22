//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "gameeventeditdoc.h"
#include "tier1/KeyValues.h"
#include "tier1/utlbuffer.h"
#include "toolutils/enginetools_int.h"
#include "filesystem.h"
#include "toolframework/ienginetool.h"
#include "datamodel/idatamodel.h"
#include "toolutils/attributeelementchoicelist.h"
#include "vgui_controls/messagebox.h"

// FIXME: This document currently stores a whole lot of nothing.

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CGameEventEditDoc::CGameEventEditDoc()
{
	m_hRoot = NULL;
	m_pTXTFileName[0] = 0;
	m_bDirty = false;
	g_pDataModel->InstallNotificationCallback( this );
}

CGameEventEditDoc::~CGameEventEditDoc()
{
	g_pDataModel->RemoveNotificationCallback( this );
}

//-----------------------------------------------------------------------------
// Inherited from INotifyUI
//-----------------------------------------------------------------------------
void CGameEventEditDoc::NotifyDataChanged( const char *pReason, int nNotifySource, int nNotifyFlags )
{
	//OnDataChanged( pReason, nNotifySource, nNotifyFlags );
}

//-----------------------------------------------------------------------------
// Gets the file name
//-----------------------------------------------------------------------------
const char *CGameEventEditDoc::GetTXTFileName()
{
	return m_pTXTFileName;
}

void CGameEventEditDoc::SetTXTFileName( const char *pFileName )
{
	Q_strncpy( m_pTXTFileName, pFileName, sizeof( m_pTXTFileName ) );
	Q_FixSlashes( m_pTXTFileName );
	SetDirty( true );
}

//-----------------------------------------------------------------------------
// Dirty bits
//-----------------------------------------------------------------------------
void CGameEventEditDoc::SetDirty( bool bDirty )
{
	m_bDirty = bDirty;
}

bool CGameEventEditDoc::IsDirty() const
{
	return m_bDirty;
}

//-----------------------------------------------------------------------------
// Saves/loads from file
//-----------------------------------------------------------------------------
bool CGameEventEditDoc::LoadFromFile( const char *pFileName )
{
/*
	Assert( !m_hRoot.Get() );

	CAppDisableUndoScopeGuard guard( "CCommEditDoc::LoadFromFile", 0 );
	SetDirty( false );

	if ( !pFileName[0] )
		return false;

	char mapname[ 256 ];

	// Compute the map name
	const char *pMaps = Q_stristr( pFileName, "\\maps\\" );
	if ( !pMaps )
		return false;

	// Build map name
	//int nNameLen = (int)( (size_t)pComm - (size_t)pMaps ) - 5;
	Q_StripExtension( pFileName, mapname, sizeof(mapname) );
	char *pszFileName = (char*)Q_UnqualifiedFileName(mapname);

	// Set the txt file name. 
	// If we loaded an existing commentary file, keep the same filename.
	// If we loaded a .bsp, change the name & the extension.
	if ( !V_stricmp( Q_GetFileExtension( pFileName ), "bsp" ) )
	{
		const char *pCommentaryAppend = "_commentary.txt";
		Q_StripExtension( pFileName, m_pTXTFileName, sizeof(m_pTXTFileName)- strlen(pCommentaryAppend) - 1 );
		Q_strcat( m_pTXTFileName, pCommentaryAppend, sizeof( m_pTXTFileName ) );

		if ( g_pFileSystem->FileExists( m_pTXTFileName ) )
		{
			char pBuf[1024];
			Q_snprintf( pBuf, sizeof(pBuf), "File %s already exists!\n", m_pTXTFileName ); 
			m_pTXTFileName[0] = 0;
			vgui::MessageBox *pMessageBox = new vgui::MessageBox( "Unable to overwrite file!\n", pBuf, g_pCommEditTool );
			pMessageBox->DoModal( );
			return false;
		}

		DmFileId_t fileid = g_pDataModel->FindOrCreateFileId( m_pTXTFileName );

		m_hRoot = CreateElement<CDmElement>( "root", fileid );
		CDmrElementArray<> subkeys( m_hRoot->AddAttribute( "subkeys", AT_ELEMENT_ARRAY ) );
		CDmElement *pRoot2 = CreateElement<CDmElement>( "Entities", fileid );
		pRoot2->AddAttribute( "subkeys", AT_ELEMENT_ARRAY );
		subkeys.AddToTail( pRoot2 );
		g_pDataModel->SetFileRoot( fileid, m_hRoot );
	}
	else
	{
		char *pComm = Q_stristr( pszFileName, "_commentary" );
		if ( !pComm )
		{
			char pBuf[1024];
			Q_snprintf( pBuf, sizeof(pBuf), "File %s is not a commentary file!\nThe file name must end in _commentary.txt.\n", m_pTXTFileName ); 
			m_pTXTFileName[0] = 0;
			vgui::MessageBox *pMessageBox = new vgui::MessageBox( "Bad file name!\n", pBuf, g_pCommEditTool );
			pMessageBox->DoModal( );
			return false;
		}

		// Clip off the "_commentary" at the end of the filename
		*pComm = '\0';

		// This is not undoable
		CDisableUndoScopeGuard guard;

		CDmElement *pTXT = NULL;

		CElementForKeyValueCallback KeyValuesCallback;
		g_pDataModel->SetKeyValuesElementCallback( &KeyValuesCallback );
		DmFileId_t fileid = g_pDataModel->RestoreFromFile( pFileName, NULL, "keyvalues", &pTXT );
		g_pDataModel->SetKeyValuesElementCallback( NULL );

		if ( fileid == DMFILEID_INVALID )
		{
			m_pTXTFileName[0] = 0;
			return false;
		}

		SetTXTFileName( pFileName );
		m_hRoot = pTXT;
	}

	guard.Release();
	SetDirty( false );

	char cmd[ 256 ];
	Q_snprintf( cmd, sizeof( cmd ), "disconnect; map %s\n", pszFileName );
	enginetools->Command( cmd );
	enginetools->Execute( );*/

	return true;
}

void CGameEventEditDoc::SaveToFile( )
{
	if ( m_hRoot.Get() && m_pTXTFileName && m_pTXTFileName[0] )
	{
		g_pDataModel->SaveToFile( m_pTXTFileName, NULL, "keyvalues", "keyvalues", m_hRoot );
	}

	SetDirty( false );
}