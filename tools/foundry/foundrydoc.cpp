//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "foundrydoc.h"
#include "tier1/KeyValues.h"
#include "tier1/utlbuffer.h"
#include "datamodel/dmelement.h"
#include "toolutils/enginetools_int.h"
#include "filesystem.h"
#include "foundrytool.h"
#include "toolframework/ienginetool.h"
#include "dmevmfentity.h"


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CFoundryDoc::CFoundryDoc( IFoundryDocCallback *pCallback ) : m_pCallback( pCallback )
{
	m_hRoot = NULL;
	m_pBSPFileName[0] = 0;
	m_pVMFFileName[0] = 0;
	m_bDirty = false;
	g_pDataModel->InstallNotificationCallback( this );
}

CFoundryDoc::~CFoundryDoc()
{
	g_pDataModel->RemoveNotificationCallback( this );
}


//-----------------------------------------------------------------------------
// Inherited from INotifyUI
//-----------------------------------------------------------------------------
void CFoundryDoc::NotifyDataChanged( const char *pReason, int nNotifySource, int nNotifyFlags )
{
	OnDataChanged( pReason, nNotifySource, nNotifyFlags );
}

	
//-----------------------------------------------------------------------------
// Gets the file name
//-----------------------------------------------------------------------------
const char *CFoundryDoc::GetBSPFileName()
{
	return m_pBSPFileName;
}

const char *CFoundryDoc::GetVMFFileName()
{
	return m_pVMFFileName;
}

void CFoundryDoc::SetVMFFileName( const char *pFileName )
{
	Q_strncpy( m_pVMFFileName, pFileName, sizeof( m_pVMFFileName ) );
	Q_FixSlashes( m_pVMFFileName );
	SetDirty( true );
}


//-----------------------------------------------------------------------------
// Dirty bits
//-----------------------------------------------------------------------------
void CFoundryDoc::SetDirty( bool bDirty )
{
	m_bDirty = bDirty;
}

bool CFoundryDoc::IsDirty() const
{
	return m_bDirty;
}


//-----------------------------------------------------------------------------
// Saves/loads from file
//-----------------------------------------------------------------------------
bool CFoundryDoc::LoadFromFile( const char *pFileName )
{
	Assert( !m_hRoot.Get() );

	// This is not undoable
	CAppDisableUndoScopeGuard guard( "CFoundryDoc::LoadFromFile", 0 );
	SetDirty( false );

	if ( !pFileName[0] )
		return false;

	// Store the BSP file name
	Q_strncpy( m_pBSPFileName, pFileName, sizeof( m_pBSPFileName ) );

	// Construct VMF file name from the BSP
	const char *pGame = Q_stristr( pFileName, "\\game\\" );
	if ( !pGame )
		return false;

	// Compute the map name
	char mapname[ 256 ];
	const char *pMaps = Q_stristr( pFileName, "\\maps\\" );
	if ( !pMaps )
		return false;

	Q_strncpy( mapname, pMaps + 6, sizeof( mapname ) );

	int nLen = (int)( (size_t)pGame - (size_t)pFileName ) + 1;
	Q_strncpy( m_pVMFFileName, pFileName, nLen );
	Q_strncat( m_pVMFFileName, "\\content\\", sizeof(m_pVMFFileName) );
	Q_strncat( m_pVMFFileName, pGame + 6, sizeof(m_pVMFFileName) );
	Q_SetExtension( m_pVMFFileName, ".vmf", sizeof(m_pVMFFileName) );

	CDmElement *pVMF = NULL;
	if ( g_pDataModel->RestoreFromFile( m_pVMFFileName, NULL, "vmf", &pVMF ) == DMFILEID_INVALID )
	{
		m_pBSPFileName[0] = 0;
		m_pVMFFileName[0] = 0;
		return false;
	}

	m_hRoot = pVMF;

	guard.Release();
	SetDirty( false );

	char cmd[ 256 ];
	Q_snprintf( cmd, sizeof( cmd ), "disconnect; map %s\n", mapname );
	enginetools->Command( cmd );
	enginetools->Execute( );

	return true;
}

void CFoundryDoc::SaveToFile( )
{
	if ( m_hRoot.Get() && m_pVMFFileName && m_pVMFFileName[0] )
	{
		g_pDataModel->SaveToFile( m_pVMFFileName, NULL, "keyvalues", "vmf", m_hRoot );
	}

	SetDirty( false );
}

	
//-----------------------------------------------------------------------------
// Returns the root object
//-----------------------------------------------------------------------------
CDmElement *CFoundryDoc::GetRootObject()
{
	return m_hRoot;
}

	
//-----------------------------------------------------------------------------
// Returns the entity list
//-----------------------------------------------------------------------------
CDmAttribute *CFoundryDoc::GetEntityList()
{
	return m_hRoot ? m_hRoot->GetAttribute( "entities", AT_ELEMENT_ARRAY ) : NULL;
}


//-----------------------------------------------------------------------------
// Deletes an entity
//-----------------------------------------------------------------------------
void CFoundryDoc::DeleteEntity( CDmeVMFEntity *pEntity )
{
	CDmrElementArray<> entities( GetEntityList() );
	if ( !entities.IsValid() )
		return;

	int nCount = entities.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( pEntity == CastElement< CDmeVMFEntity >( entities[i] ) )
		{
			entities.FastRemove( i );
			return;
		}
	}
}

	
//-----------------------------------------------------------------------------
// Called when data changes
//-----------------------------------------------------------------------------
void CFoundryDoc::OnDataChanged( const char *pReason, int nNotifySource, int nNotifyFlags )
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
// Always copy the worldspawn and other entities that had data built into them by VBSP out
//-----------------------------------------------------------------------------
void CFoundryDoc::AddOriginalEntities( CUtlBuffer &entityBuf, const char *pActualEntityData )
{
	while ( *pActualEntityData )
	{
		pActualEntityData = strchr( pActualEntityData, '{' );
		if ( !pActualEntityData )
			break;

		const char *pBlockStart = pActualEntityData;

		pActualEntityData = strstr( pActualEntityData, "\"classname\"" );
		if ( !pActualEntityData )
			break;

		// Skip "classname"
		pActualEntityData += 11;

		pActualEntityData = strchr( pActualEntityData, '\"' );
		if ( !pActualEntityData )
			break;

		// Skip "
		++pActualEntityData;

		char pClassName[512];
		int j = 0;
		while (*pActualEntityData != 0 && *pActualEntityData != '\"' )
		{
			pClassName[j++] = *pActualEntityData++;	
		}
		pClassName[j] = 0;

		pActualEntityData = strchr( pActualEntityData, '}' );
		if ( !pActualEntityData )
			break;
		
		// Skip }
		++pActualEntityData;

		for ( int i = 0; s_pUseOriginalClasses[i]; ++i )
		{
			if ( !Q_stricmp( pClassName, s_pUseOriginalClasses[i] ) )
			{
				// Found one we need to keep, add it to the buffer
				int nBytes = (int)( (size_t)pActualEntityData - (size_t)pBlockStart );
				entityBuf.Put( pBlockStart, nBytes );
				entityBuf.PutChar( '\n' );
				break;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Copy in other entities from the editable VMF
//-----------------------------------------------------------------------------
void CFoundryDoc::AddVMFEntities( CUtlBuffer &entityBuf, const char *pActualEntityData )
{
	const CDmrElementArray<CDmElement> entityArray( m_hRoot, "entities" );
	if ( !entityArray.IsValid() )
		return;

	int nCount = entityArray.Count();
	for ( int iEntity = 0; iEntity < nCount; ++iEntity )
	{
		CDmElement *pEntity = entityArray[iEntity];
		const char *pClassName = pEntity->GetValueString( "classname" );
		if ( !pClassName || !pClassName[0] )
			continue;

		// Don't spawn those classes we grab from the actual compiled map
		bool bDontUse = false;
		for ( int i = 0; s_pUseOriginalClasses[i]; ++i )
		{
			if ( !Q_stricmp( pClassName, s_pUseOriginalClasses[i] ) )
			{
				bDontUse = true;
				break;
			}
		}

		if ( bDontUse )
			continue;

		entityBuf.PutString( "{\n" );
		entityBuf.Printf( "\"id\" \"%d\"\n", atol( pEntity->GetName() ) );

		for( CDmAttribute *pAttribute = pEntity->FirstAttribute(); pAttribute; pAttribute = pAttribute->NextAttribute() )
		{
			if ( pAttribute->IsFlagSet( FATTRIB_STANDARD ) )
				continue;

			if ( IsArrayType( pAttribute->GetType() ) )
				continue;

			if ( !Q_stricmp( pAttribute->GetName(), "editorType" ) || !Q_stricmp( pAttribute->GetName(), "editor" ) )
				continue;

			entityBuf.Printf( "\"%s\" ", pAttribute->GetName() );

			// FIXME: Set up standard delimiters
			entityBuf.PutChar( '\"' );
			pAttribute->Serialize( entityBuf );
			entityBuf.PutString( "\"\n" );
		}

		entityBuf.PutString( "}\n" );
	}
}


//-----------------------------------------------------------------------------
// Create a text block the engine can parse containing the entity data to spawn
//-----------------------------------------------------------------------------
const char* CFoundryDoc::GenerateEntityData( const char *pActualEntityData )
{
	if ( !m_hRoot.Get() )
		return pActualEntityData;

	// Contains the text block the engine can parse containing the entity data to spawn
	static CUtlBuffer entityBuf( 2048, 2048, CUtlBuffer::TEXT_BUFFER );
	entityBuf.Clear();

	// Always copy the worldspawn and other entities that had data built into them by VBSP out
	AddOriginalEntities( entityBuf, pActualEntityData );

	// Copy in other entities from the editable VMF
	AddVMFEntities( entityBuf, pActualEntityData );

	return (const char*)entityBuf.Base();
}

