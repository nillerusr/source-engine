//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "vcdblockdoc.h"
#include "tier1/KeyValues.h"
#include "tier1/utlbuffer.h"
#include "toolutils/enginetools_int.h"
#include "filesystem.h"
#include "vcdblocktool.h"
#include "toolframework/ienginetool.h"
#include "dmevmfentity.h"
#include "datamodel/idatamodel.h"
#include "toolutils/attributeelementchoicelist.h"
#include "infotargetbrowserpanel.h"
#include "vgui_controls/messagebox.h"


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CVcdBlockDoc::CVcdBlockDoc( IVcdBlockDocCallback *pCallback ) : m_pCallback( pCallback )
{
	m_hVMFRoot = NULL;
	m_hEditRoot = NULL;
	m_pBSPFileName[0] = 0;
	m_pVMFFileName[0] = 0;
	m_pEditFileName[0] = 0;
	m_bDirty = false;
	g_pDataModel->InstallNotificationCallback( this );
}

CVcdBlockDoc::~CVcdBlockDoc()
{
	g_pDataModel->RemoveNotificationCallback( this );
}


//-----------------------------------------------------------------------------
// Inherited from INotifyUI
//-----------------------------------------------------------------------------
void CVcdBlockDoc::NotifyDataChanged( const char *pReason, int nNotifySource, int nNotifyFlags )
{
	OnDataChanged( pReason, nNotifySource, nNotifyFlags );
}

	
//-----------------------------------------------------------------------------
// Gets the file name
//-----------------------------------------------------------------------------
const char *CVcdBlockDoc::GetBSPFileName()
{
	return m_pBSPFileName;
}

const char *CVcdBlockDoc::GetVMFFileName()
{
	return m_pVMFFileName;
}

void CVcdBlockDoc::SetVMFFileName( const char *pFileName )
{
	Q_strncpy( m_pVMFFileName, pFileName, sizeof( m_pVMFFileName ) );
	Q_FixSlashes( m_pVMFFileName );
	SetDirty( true );
}

const char *CVcdBlockDoc::GetEditFileName()
{
	return m_pEditFileName;
}

void CVcdBlockDoc::SetEditFileName( const char *pFileName )
{
	Q_strncpy( m_pEditFileName, pFileName, sizeof( m_pEditFileName ) );
	Q_FixSlashes( m_pEditFileName );
	SetDirty( true );
}

//-----------------------------------------------------------------------------
// Dirty bits
//-----------------------------------------------------------------------------
void CVcdBlockDoc::SetDirty( bool bDirty )
{
	m_bDirty = bDirty;
}

bool CVcdBlockDoc::IsDirty() const
{
	return m_bDirty;
}


//-----------------------------------------------------------------------------
// Saves/loads from file
//-----------------------------------------------------------------------------
bool CVcdBlockDoc::LoadFromFile( const char *pFileName )
{
	Assert( !m_hVMFRoot.Get() );
	Assert( !m_hEditRoot.Get() );

	CAppDisableUndoScopeGuard guard( "CVcdBlockDoc::LoadFromFile", NOTIFY_CHANGE_OTHER );
	SetDirty( false );

	if ( !pFileName[0] )
		return false;

	// Construct VMF file name from the BSP
	const char *pGame = Q_stristr( pFileName, "\\game\\" );
	if ( !pGame )
	{
		pGame = Q_stristr( pFileName, "\\content\\" );
		if ( !pGame )
			return false;
	}

	// Compute the map name
	const char *pMaps = Q_stristr( pFileName, "\\maps\\" );
	if ( !pMaps )
		return false;

	// Build map name
	char mapname[ 256 ];
	Q_StripExtension( pFileName, mapname, sizeof(mapname) );
	char *pszFileName = (char*)Q_UnqualifiedFileName(mapname);

	int nLen = (int)( (size_t)pGame - (size_t)pFileName ) + 1;
	Q_strncpy( m_pVMFFileName, pFileName, nLen );
	Q_strncat( m_pVMFFileName, "\\content\\", sizeof(m_pVMFFileName) );
	Q_strncat( m_pVMFFileName, pGame + 6, sizeof(m_pVMFFileName) );
	Q_SetExtension( m_pVMFFileName, ".vmf", sizeof(m_pVMFFileName) );

	// Make sure new entities start with ids at 0
	CDmeVMFEntity::SetNextEntityId( 0 );

	// Build the Edit file name
	Q_StripExtension( m_pVMFFileName, m_pEditFileName, sizeof(m_pEditFileName) );
	Q_strncat( m_pEditFileName, ".vle", sizeof( m_pEditFileName ) );

	// Store the BSP file name
	Q_strncpy( m_pBSPFileName, pFileName, sizeof( m_pBSPFileName ) );

	// Set the txt file name. 
	// If we loaded a .bsp, clear out what we're doing
	// load the Edits file into memory, assign it as our "root"
	CDmElement *pEdit = NULL;
	if ( !V_stricmp( Q_GetFileExtension( pFileName ), "vle" ) )
	{
		if ( g_pDataModel->RestoreFromFile( m_pEditFileName, NULL, "vmf", &pEdit ) != DMFILEID_INVALID )
		{
			// If we successfully read the file in, ask it for the max hammer id
			//int nMaxHammerId = pVMF->GetAttributeValue<int>( "maxHammerId" );
			//CDmeVMFEntity::SetNextEntityId( nMaxHammerId + 1 );
			m_hEditRoot = pEdit;
			SetDirty( false );
		}
	}

	if (pEdit == NULL)
	{
		if ( g_pFileSystem->FileExists( m_pEditFileName ) )
		{
			char pBuf[1024];
			Q_snprintf( pBuf, sizeof(pBuf), "File %s already exists!\n", m_pEditFileName ); 
			m_pEditFileName[0] = 0;
			vgui::MessageBox *pMessageBox = new vgui::MessageBox( "Unable to overwrite file!\n", pBuf, g_pVcdBlockTool );
			pMessageBox->DoModal( );
			return false;
		}

		DmFileId_t fileid = g_pDataModel->FindOrCreateFileId( m_pEditFileName );
		m_hEditRoot = CreateElement<CDmElement>( "root", fileid );
		m_hEditRoot->AddAttribute( "entities", AT_ELEMENT_ARRAY );
		g_pDataModel->SetFileRoot( fileid, m_hEditRoot );
		SetDirty( true );
	}

	guard.Release();

	// tell the engine to actually load the map
	char cmd[ 256 ];
	Q_snprintf( cmd, sizeof( cmd ), "disconnect; map %s\n", pszFileName );
	enginetools->Command( cmd );
	enginetools->Execute( );

	return true;
}


void CVcdBlockDoc::SaveToFile( )
{
	if ( m_hEditRoot.Get() && m_pEditFileName && m_pEditFileName[0] )
	{
		g_pDataModel->SaveToFile( m_pEditFileName, NULL, "keyvalues", "vmf", m_hEditRoot );
	}

	SetDirty( false );
}

	
//-----------------------------------------------------------------------------
// Returns the root object
//-----------------------------------------------------------------------------
CDmElement *CVcdBlockDoc::GetRootObject()
{
	return m_hEditRoot;
}

	
//-----------------------------------------------------------------------------
// Returns the entity list
//-----------------------------------------------------------------------------
CDmAttribute *CVcdBlockDoc::GetEntityList()
{
	return m_hEditRoot ? m_hEditRoot->GetAttribute( "entities", AT_ELEMENT_ARRAY ) : NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVcdBlockDoc::AddNewInfoTarget( const Vector &vecOrigin, const QAngle &angAngles )
{
	CDmrElementArray<> entities = GetEntityList();

	CDmeVMFEntity *pTarget;
	{
		CAppUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, "Add Info Target", "Add Info Target" );

		pTarget = CreateElement<CDmeVMFEntity>( "", entities.GetOwner()->GetFileId() );
		pTarget->SetValue( "classname", "info_target" );
		pTarget->SetRenderOrigin( vecOrigin );
		pTarget->SetRenderAngles( angAngles );

		entities.AddToTail( pTarget );
		pTarget->MarkDirty();
		pTarget->DrawInEngine( true );
	}

	g_pVcdBlockTool->GetInfoTargetBrowser()->SelectNode( pTarget );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVcdBlockDoc::AddNewInfoTarget( void )
{
	Vector vecOrigin;
	QAngle angAngles;
	float flFov;
	clienttools->GetLocalPlayerEyePosition( vecOrigin, angAngles, flFov );
	AddNewInfoTarget( vecOrigin, vec3_angle ); 
}


//-----------------------------------------------------------------------------
// Deletes a commentary node
//-----------------------------------------------------------------------------
void CVcdBlockDoc::DeleteInfoTarget( CDmeVMFEntity *pNode )
{
	CDmrElementArray<CDmElement> entities = GetEntityList();
	int nCount = entities.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( pNode == entities[i] )
		{
			CAppUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, "Delete Info Target", "Delete Info Target" );
			CDmeVMFEntity *pNode = CastElement< CDmeVMFEntity >( entities[ i ] );
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
// Output : CDmeVMFEntity
//-----------------------------------------------------------------------------
CDmeVMFEntity *CVcdBlockDoc::GetInfoTargetForLocation( Vector &vecOrigin, QAngle &angAbsAngles )
{
	const CDmrElementArray<> entities = GetEntityList();
	int nCount = entities.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeVMFEntity *pNode = CastElement< CDmeVMFEntity >( entities[ i ] );
		Vector &vecAngles = *(Vector*)(&pNode->GetRenderAngles());
		if ( pNode->GetRenderOrigin().DistTo( vecOrigin ) < 1e-3 && vecAngles.DistTo( *(Vector*)&angAbsAngles ) < 1e-1 )
			return pNode;
	}

	return NULL;
}



//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &vecStart - 
//			&vecEnd - 
// Output : CDmeVMFEntity
//-----------------------------------------------------------------------------
CDmeVMFEntity *CVcdBlockDoc::GetInfoTargetForLocation( Vector &vecStart, Vector &vecEnd )
{
	Vector vecDelta;
	float flEndDist;

	vecDelta = vecEnd - vecStart;
	flEndDist = VectorNormalize( vecDelta );

	CDmeVMFEntity *pSelectedNode = NULL;
	float flMinDistFromLine = 1E30;

	const CDmrElementArray<CDmElement> entities = GetEntityList();
	int nCount = entities.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeVMFEntity *pNode = CastElement< CDmeVMFEntity >( entities[ i ] );
		float flDistAway = DotProduct( pNode->GetRenderOrigin() - vecStart, vecDelta );

		if (flDistAway > 0.0 && flDistAway < flEndDist)
		{
			float flDistFromLine = (pNode->GetRenderOrigin() - vecStart - vecDelta * flDistAway).Length();
			if (flDistFromLine < flMinDistFromLine)
			{
				pSelectedNode = pNode;
				flMinDistFromLine = flDistFromLine;
			}
		}
	}
	return pSelectedNode;
}


//-----------------------------------------------------------------------------
// Populate string choice lists
//-----------------------------------------------------------------------------
bool CVcdBlockDoc::GetStringChoiceList( const char *pChoiceListType, CDmElement *pElement, 
									const char *pAttributeName, bool bArrayElement, StringChoiceList_t &list )
{
	if ( !Q_stricmp( pChoiceListType, "info_targets" ) )
	{
		const CDmrElementArray<> entities = GetEntityList();

		StringChoice_t sChoice;
		sChoice.m_pValue = "";
		sChoice.m_pChoiceString = "";
		list.AddToTail( sChoice );

		int nCount = entities.Count();
		for ( int i = 0; i < nCount; ++i )
		{
			CDmeVMFEntity *pNode = CastElement< CDmeVMFEntity >( entities[ i ] );
			if ( !V_stricmp( pNode->GetClassName(), "info_target" ) )
			{
				StringChoice_t sChoice;
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
bool CVcdBlockDoc::GetElementChoiceList( const char *pChoiceListType, CDmElement *pElement, 
									 const char *pAttributeName, bool bArrayElement, ElementChoiceList_t &list )
{
	if ( !Q_stricmp( pChoiceListType, "allelements" ) )
	{
		AddElementsRecursively( m_hEditRoot, list );
		return true;
	}

	if ( !Q_stricmp( pChoiceListType, "info_targets" ) )
	{
		const CDmrElementArray<> entities = GetEntityList();

		bool bFound = false;
		int nCount = entities.Count();
		for ( int i = 0; i < nCount; ++i )
		{
			CDmeVMFEntity *pNode = CastElement< CDmeVMFEntity >( entities[ i ] );
			if ( !V_stricmp( pNode->GetClassName(), "info_target" ) )
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
	AddElementsRecursively( m_hEditRoot, list, pChoiceListType );

	return list.Count() > 0;
}

	
//-----------------------------------------------------------------------------
// Called when data changes
//-----------------------------------------------------------------------------
void CVcdBlockDoc::OnDataChanged( const char *pReason, int nNotifySource, int nNotifyFlags )
{
	SetDirty( nNotifyFlags & NOTIFY_SETDIRTYFLAG ? true : false );
	m_pCallback->OnDocChanged( pReason, nNotifySource, nNotifyFlags );
}



//-----------------------------------------------------------------------------
// List of all entity classnames to copy over from the original block
//-----------------------------------------------------------------------------
static const char *s_pUseOriginalClasses[] =
{
	"worldspawn",
	"func_occluder",
	NULL
};


//-----------------------------------------------------------------------------
// The server just loaded, populate the list with the entities is has
//-----------------------------------------------------------------------------
void CVcdBlockDoc::ServerLevelInitPostEntity( void )
{
	CDmrElementArray<> entityList = GetEntityList();

	if ( entityList.Count() )
	{
		VerifyAllEdits( entityList );
	}
	else
	{
		InitializeFromServer( entityList );
	}
}


//-----------------------------------------------------------------------------
// Create a list of entities based on what the server has
//-----------------------------------------------------------------------------
void CVcdBlockDoc::InitializeFromServer( CDmrElementArray<> &entityList )
{
	CAppUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, "Initialize From Server" );

	entityList.RemoveAll();

	// initialize list with entities on the server
	CBaseEntity *pServerEnt = servertools->FirstEntity();
	while (pServerEnt)
	{
		char classname[256];

		if (servertools->GetKeyValue( pServerEnt, "classname", classname, sizeof( classname ) ) )
		{
			if ( !Q_stricmp( classname, "info_target" ))
			{
				char hammerid[256];
				if ( servertools->GetKeyValue( pServerEnt, "hammerid", hammerid, sizeof( hammerid ) ) )
				{
					int nextId = CDmeVMFEntity::GetNextEntityId();
					CDmeVMFEntity::SetNextEntityId( atoi( hammerid ) );

					CDmeVMFEntity *pTarget = CreateElement<CDmeVMFEntity>( "", entityList.GetOwner()->GetFileId() );

					CDmeVMFEntity::SetNextEntityId( nextId );

					if ( pTarget->CopyFromServer( pServerEnt ) )
					{
						entityList.AddToTail( pTarget );
					}
				}
			}
		}
		pServerEnt = servertools->NextEntity( pServerEnt );
	}
}


//-----------------------------------------------------------------------------
// Check the list of entities on the server against the edits that are already made
//-----------------------------------------------------------------------------
void CVcdBlockDoc::VerifyAllEdits( const CDmrElementArray<> &entityList )
{
    // already filled in
	for (int i = 0; i < entityList.Count(); i++)
	{
		CDmeVMFEntity *pEntity = CastElement<CDmeVMFEntity>( entityList[i] );

		CBaseEntity *pServerEntity = servertools->FindEntityByHammerID( pEntity->GetEntityId() );

		if (pServerEntity != NULL)
		{
			if (!pEntity->IsSameOnServer( pServerEntity ))
			{
				pEntity->MarkDirty();
			}
			else
			{
				pEntity->MarkDirty(false);
			}
		}
		else
		{
			pEntity->CreateOnServer();
			pEntity->MarkDirty();
		}
	}
}


//-----------------------------------------------------------------------------
// Load the VMF file, merge in all the edits, write it back out
//-----------------------------------------------------------------------------
bool CVcdBlockDoc::CopyEditsToVMF( )
{
	const CDmrElementArray<CDmElement> entityList = GetEntityList();
	
	CDmElement *pVMF = NULL;
	DmFileId_t fileid = g_pDataModel->FindOrCreateFileId( m_pVMFFileName );
	if ( g_pDataModel->RestoreFromFile( m_pVMFFileName, NULL, "vmf", &pVMF ) == DMFILEID_INVALID )
	{
		// needs some kind of error message
		return false;
	}

	CDmrElementArray<CDmElement> vmfEntities( pVMF, "entities" );

	int nVMFCount = vmfEntities.Count();
	for (int i = 0; i < nVMFCount; i++)
	{
		CDmElement *pVMFEntity = vmfEntities[i];

		char classname[256];
		pVMFEntity->GetValueAsString( "classname", classname, sizeof( classname ) );

		if ( Q_stricmp( "info_target", classname ) )
			continue;

		int nHammerID = atoi( pVMFEntity->GetName() );

		// find a match.
		int nCount = entityList.Count();
		for (int j = 0; j < nCount; j++)
		{
			CDmeVMFEntity *pEntity = CastElement<CDmeVMFEntity>( entityList[j] );

			if ( pEntity->IsDirty() && pEntity->GetEntityId() == nHammerID)
			{
				char text[256];
				pEntity->GetValueAsString( "targetname", text, sizeof( text ) );
				pVMFEntity->SetValueFromString( "targetname", text );
				pEntity->GetValueAsString( "origin", text, sizeof( text ) );
				pVMFEntity->SetValueFromString( "origin", text );
				pEntity->GetValueAsString( "angles", text, sizeof( text ) );
				pVMFEntity->SetValueFromString( "angles", text );

				pEntity->MarkDirty(false);
			}
		}
	}

	// add the new entities
	int nCount = entityList.Count();
	for (int j = 0; j < nCount; j++)
	{
		CDmeVMFEntity *pEntity = CastElement<CDmeVMFEntity>( entityList[j] );

		if ( pEntity->IsDirty())
		{
			CDmElement *pVMFEntity = CreateElement<CDmElement>( pEntity->GetName(), fileid );

			char text[256];
			pEntity->GetValueAsString( "classname", text, sizeof( text ) );
			pVMFEntity->SetValue( "classname", text );
			pEntity->GetValueAsString( "targetname", text, sizeof( text ) );
			pVMFEntity->SetValue( "targetname", text );
			pEntity->GetValueAsString( "origin", text, sizeof( text ) );
			pVMFEntity->SetValue( "origin", text );
			pEntity->GetValueAsString( "angles", text, sizeof( text ) );
			pVMFEntity->SetValue( "angles", text );

			vmfEntities.AddToTail( pVMFEntity );

			pEntity->MarkDirty(false);
		}
	}

	// currently, don't overwrite the vmf, not sure if this is serializing correctly yet
	char tmpname[ 256 ];
	Q_StripExtension( m_pVMFFileName, tmpname, sizeof(tmpname) );
	Q_SetExtension( tmpname, ".vme", sizeof(tmpname) );

	if (!g_pDataModel->SaveToFile( tmpname, NULL, "keyvalues", "vmf", pVMF ))
	{
		// needs some kind of error message
		return false;
	}

	/*
		// If we successfully read the file in, ask it for the max hammer id
		int nMaxHammerId = pVMF->GetAttributeValue<int>( "maxHammerId" );
		CDmeVMFEntity::SetNextEntityId( nMaxHammerId + 1 );
		m_hVMFRoot = pVMF;
	*/

	return true;
}


bool CVcdBlockDoc::RememberPlayerPosition()
{
	return true;
}
