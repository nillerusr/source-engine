//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "commeditdoc.h"
#include "tier1/KeyValues.h"
#include "tier1/utlbuffer.h"
#include "toolutils/enginetools_int.h"
#include "filesystem.h"
#include "commedittool.h"
#include "toolframework/ienginetool.h"
#include "dmecommentarynodeentity.h"
#include "datamodel/idatamodel.h"
#include "toolutils/attributeelementchoicelist.h"
#include "commentarynodebrowserpanel.h"
#include "vgui_controls/messagebox.h"


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CCommEditDoc::CCommEditDoc( ICommEditDocCallback *pCallback ) : m_pCallback( pCallback )
{
	m_hRoot = NULL;
	m_pTXTFileName[0] = 0;
	m_bDirty = false;
	g_pDataModel->InstallNotificationCallback( this );
}

CCommEditDoc::~CCommEditDoc()
{
	g_pDataModel->RemoveNotificationCallback( this );
}


//-----------------------------------------------------------------------------
// Inherited from INotifyUI
//-----------------------------------------------------------------------------
void CCommEditDoc::NotifyDataChanged( const char *pReason, int nNotifySource, int nNotifyFlags )
{
	OnDataChanged( pReason, nNotifySource, nNotifyFlags );
}

	
//-----------------------------------------------------------------------------
// Gets the file name
//-----------------------------------------------------------------------------
const char *CCommEditDoc::GetTXTFileName()
{
	return m_pTXTFileName;
}

void CCommEditDoc::SetTXTFileName( const char *pFileName )
{
	Q_strncpy( m_pTXTFileName, pFileName, sizeof( m_pTXTFileName ) );
	Q_FixSlashes( m_pTXTFileName );
	SetDirty( true );
}

//-----------------------------------------------------------------------------
// Dirty bits
//-----------------------------------------------------------------------------
void CCommEditDoc::SetDirty( bool bDirty )
{
	m_bDirty = bDirty;
}

bool CCommEditDoc::IsDirty() const
{
	return m_bDirty;
}


//-----------------------------------------------------------------------------
// Handles creation of the right element for a keyvalue
//-----------------------------------------------------------------------------
class CElementForKeyValueCallback : public IElementForKeyValueCallback
{
public:
	const char *GetElementForKeyValue( const char *pszKeyName, int iNestingLevel )
	{
		if ( iNestingLevel == 1 && !Q_strncmp(pszKeyName, "entity", 6) )
			return "DmeCommentaryNodeEntity";

		return NULL;
	}
};

//-----------------------------------------------------------------------------
// Saves/loads from file
//-----------------------------------------------------------------------------
bool CCommEditDoc::LoadFromFile( const char *pFileName )
{
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
		CDisableUndoScopeGuard guardFile;

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
	enginetools->Execute( );

	return true;
}

void CCommEditDoc::SaveToFile( )
{
	if ( m_hRoot.Get() && m_pTXTFileName && m_pTXTFileName[0] )
	{
		g_pDataModel->SaveToFile( m_pTXTFileName, NULL, "keyvalues", "keyvalues", m_hRoot );
	}

	SetDirty( false );
}

	
//-----------------------------------------------------------------------------
// Returns the root object
//-----------------------------------------------------------------------------
CDmElement *CCommEditDoc::GetRootObject()
{
	return m_hRoot;
}

	
//-----------------------------------------------------------------------------
// Returns the entity list
//-----------------------------------------------------------------------------
CDmAttribute *CCommEditDoc::GetEntityList()
{
	CDmrElementArray<> mainKeys( m_hRoot, "subkeys" );
	if ( !mainKeys.IsValid() || mainKeys.Count() == 0 )
		return NULL;
	CDmeHandle<CDmElement> hEntityList;
	hEntityList = mainKeys[ 0 ];
	return hEntityList ? hEntityList->GetAttribute( "subkeys", AT_ELEMENT_ARRAY ) : NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCommEditDoc::AddNewInfoTarget( const Vector &vecOrigin, const QAngle &angAngles )
{
	CDmrCommentaryNodeEntityList entities( GetEntityList() );
	if ( !entities.IsValid() )
		return;

	CDmeCommentaryNodeEntity *pTarget;
	{
		CAppUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, "Add Info Target", "Add Info Target" );

		pTarget = CreateElement<CDmeCommentaryNodeEntity>( "target", entities.GetOwner()->GetFileId() );
		pTarget->SetName( "entity" );
		pTarget->SetValue( "classname", "info_target" );
		pTarget->SetRenderOrigin( vecOrigin );
		pTarget->SetRenderAngles( angAngles );

		entities.AddToTail( pTarget );
		pTarget->MarkDirty();
		pTarget->DrawInEngine( true );
	}

	g_pCommEditTool->GetCommentaryNodeBrowser()->SelectNode( pTarget );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCommEditDoc::AddNewInfoTarget( void )
{
	Vector vecOrigin;
	QAngle angAngles;
	float flFov;
	clienttools->GetLocalPlayerEyePosition( vecOrigin, angAngles, flFov );
	AddNewInfoTarget( vecOrigin, vec3_angle ); 
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCommEditDoc::AddNewCommentaryNode( const Vector &vecOrigin, const QAngle &angAngles )
{
	CDmrCommentaryNodeEntityList entities = GetEntityList();

	CDmeCommentaryNodeEntity *pNode;
	{
		CAppUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, "Add Commentary Node", "Add Commentary Node" );

		pNode = CreateElement<CDmeCommentaryNodeEntity>( "node", entities.GetOwner()->GetFileId() );
		pNode->SetName( "entity" );
		pNode->SetValue( "classname", "point_commentary_node" );
		pNode->SetRenderOrigin( vecOrigin );
		pNode->SetRenderAngles( angAngles );
		pNode->SetValue<CUtlString>( "precommands", "" );
		pNode->SetValue<CUtlString>( "postcommands", "" );
		pNode->SetValue<CUtlString>( "commentaryfile", "" );
		pNode->SetValue<CUtlString>( "viewtarget", "" );
		pNode->SetValue<CUtlString>( "viewposition", "" );
		pNode->SetValue<int>( "prevent_movement", 0 );
		pNode->SetValue<CUtlString>( "speakers", "" );
		pNode->SetValue<CUtlString>( "synopsis", "" );

		entities.AddToTail( pNode );
		pNode->MarkDirty();
		pNode->DrawInEngine( true );
	}

	g_pCommEditTool->GetCommentaryNodeBrowser()->SelectNode( pNode );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCommEditDoc::AddNewCommentaryNode( void )
{
	Vector vecOrigin;
	QAngle angAngles;
	float flFov;
	clienttools->GetLocalPlayerEyePosition( vecOrigin, angAngles, flFov );
	AddNewCommentaryNode( vecOrigin, vec3_angle );
}


//-----------------------------------------------------------------------------
// Deletes a commentary node
//-----------------------------------------------------------------------------
void CCommEditDoc::DeleteCommentaryNode( CDmElement *pRemoveNode )
{
	CDmrCommentaryNodeEntityList entities = GetEntityList();
	int nCount = entities.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( pRemoveNode == entities[i] )
		{
			CAppUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, "Delete Commentary Node", "Delete Commentary Node" );
			CDmeCommentaryNodeEntity *pNode = entities[ i ];
			pNode->DrawInEngine( false );
			entities.FastRemove( i );
			return;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &vecOrigin - 
//			&angAbsAngles - 
// Output : CDmeCommentaryNodeEntity
//-----------------------------------------------------------------------------
CDmeCommentaryNodeEntity *CCommEditDoc::GetCommentaryNodeForLocation( Vector &vecOrigin, QAngle &angAbsAngles )
{
	CDmrCommentaryNodeEntityList entities = GetEntityList();
	int nCount = entities.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeCommentaryNodeEntity *pNode = entities[ i ];
		if ( !pNode )
			continue;

		Vector &vecAngles = *(Vector*)(&pNode->GetRenderAngles());
		if ( pNode->GetRenderOrigin().DistTo( vecOrigin ) < 1e-3 && vecAngles.DistTo( *(Vector*)&angAbsAngles ) < 1e-1 )
			return pNode;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Populate string choice lists
//-----------------------------------------------------------------------------
bool CCommEditDoc::GetStringChoiceList( const char *pChoiceListType, CDmElement *pElement, 
									const char *pAttributeName, bool bArrayElement, StringChoiceList_t &list )
{
	if ( !Q_stricmp( pChoiceListType, "info_targets" ) )
	{
		CDmrCommentaryNodeEntityList entities = GetEntityList();

		StringChoice_t sChoice;
		sChoice.m_pValue = "";
		sChoice.m_pChoiceString = "";
		list.AddToTail( sChoice );

		int nCount = entities.Count();
		for ( int i = 0; i < nCount; ++i )
		{
			CDmeCommentaryNodeEntity *pNode = entities[ i ];
			if ( !pNode )
				continue;

			if ( !V_stricmp( pNode->GetClassName(), "info_target" ) )
			{
				sChoice.m_pValue = pNode->GetTargetName();
				sChoice.m_pChoiceString = pNode->GetTargetName();
				list.AddToTail( sChoice );
			}
		}
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Populate element choice lists
//-----------------------------------------------------------------------------
bool CCommEditDoc::GetElementChoiceList( const char *pChoiceListType, CDmElement *pElement, 
									 const char *pAttributeName, bool bArrayElement, ElementChoiceList_t &list )
{
	if ( !Q_stricmp( pChoiceListType, "allelements" ) )
	{
		AddElementsRecursively( m_hRoot, list );
		return true;
	}

	if ( !Q_stricmp( pChoiceListType, "info_targets" ) )
	{
		CDmrCommentaryNodeEntityList entities = GetEntityList();

		bool bFound = false;
		int nCount = entities.Count();
		for ( int i = 0; i < nCount; ++i )
		{
			CDmeCommentaryNodeEntity *pNode = entities[ i ];
			if ( pNode && !V_stricmp( pNode->GetClassName(), "info_target" ) )
			{
				bFound = true;
				ElementChoice_t sChoice;
				sChoice.m_pValue = pNode;
				sChoice.m_pChoiceString = pNode->GetTargetName();
				list.AddToTail( sChoice );
			}
		}
		return bFound;
	}

	// by default, try to treat the choice list type as a Dme element type
	AddElementsRecursively( m_hRoot, list, pChoiceListType );

	return list.Count() > 0;
}

	
//-----------------------------------------------------------------------------
// Called when data changes
//-----------------------------------------------------------------------------
void CCommEditDoc::OnDataChanged( const char *pReason, int nNotifySource, int nNotifyFlags )
{
	SetDirty( nNotifyFlags & NOTIFY_SETDIRTYFLAG ? true : false );
	m_pCallback->OnDocChanged( pReason, nNotifySource, nNotifyFlags );
}


