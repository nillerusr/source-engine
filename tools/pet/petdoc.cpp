//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "petdoc.h"
#include "tier1/KeyValues.h"
#include "tier1/utlbuffer.h"
#include "toolutils/enginetools_int.h"
#include "filesystem.h"
#include "pettool.h"
#include "toolframework/ienginetool.h"
#include "movieobjects/dmeparticlesystemdefinition.h"
#include "datamodel/idatamodel.h"
#include "toolutils/attributeelementchoicelist.h"
#include "particlesystemdefinitionbrowser.h"
#include "vgui_controls/messagebox.h"
#include "particles/particles.h"
#include "particlesystempropertiescontainer.h"
#include "dme_controls/particlesystempanel.h"
#include "dme_controls/dmecontrols.h"


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CPetDoc::CPetDoc( IPetDocCallback *pCallback ) : m_pCallback( pCallback )
{
	m_hRoot = NULL;
	m_pFileName[0] = 0;
	m_bDirty = false;
	g_pDataModel->InstallNotificationCallback( this );
	SetElementPropertiesChoices( this );
}

CPetDoc::~CPetDoc()
{
	if ( m_hRoot.Get() )
	{
		g_pDataModel->RemoveFileId( m_hRoot->GetFileId() );
		m_hRoot = NULL;
	}
	g_pDataModel->RemoveNotificationCallback( this );
	SetElementPropertiesChoices( NULL );
}


//-----------------------------------------------------------------------------
// Inherited from INotifyUI
//-----------------------------------------------------------------------------
void CPetDoc::NotifyDataChanged( const char *pReason, int nNotifySource, int nNotifyFlags )
{
	OnDataChanged( pReason, nNotifySource, nNotifyFlags );
}

	
bool CPetDoc::GetIntChoiceList( const char *pChoiceListType, CDmElement *pElement, 
	const char *pAttributeName, bool bArrayElement, IntChoiceList_t &list )
{
	if ( !Q_stricmp( pChoiceListType, "particlefield" ) )
	{
		for ( int i = 0; i < MAX_PARTICLE_ATTRIBUTES; ++i )
		{
			const char *pName = g_pParticleSystemMgr->GetParticleFieldName( i );
			if ( pName )
			{
				int j = list.AddToTail();
				list[j].m_nValue = i;
				list[j].m_pChoiceString = pName;
			}
		}
		return true;
	}

	if ( !Q_stricmp( pChoiceListType, "particlefield_scalar" ) )
	{
		for ( int i = 0; i < MAX_PARTICLE_ATTRIBUTES; ++i )
		{
			if ( ( ATTRIBUTES_WHICH_ARE_VEC3S_MASK & ( 1 << i ) ) != 0 )
				continue;

			const char *pName = g_pParticleSystemMgr->GetParticleFieldName( i );
			if ( pName )
			{
				int j = list.AddToTail();
				list[j].m_nValue = i;
				list[j].m_pChoiceString = pName;
			}
		}
		return true;
	}

	if ( !Q_stricmp( pChoiceListType, "particlefield_vector" ) )
	{
		for ( int i = 0; i < MAX_PARTICLE_ATTRIBUTES; ++i )
		{
			if ( ( ATTRIBUTES_WHICH_ARE_VEC3S_MASK & ( 1 << i ) ) == 0 )
				continue;

			const char *pName = g_pParticleSystemMgr->GetParticleFieldName( i );
			if ( pName )
			{
				int j = list.AddToTail();
				list[j].m_nValue = i;
				list[j].m_pChoiceString = pName;
			}
		}
		return true;
	}

	return false;
}

	
//-----------------------------------------------------------------------------
// Gets the file name
//-----------------------------------------------------------------------------
const char *CPetDoc::GetFileName()
{
	return m_pFileName;
}

void CPetDoc::SetFileName( const char *pFileName )
{
	Q_strncpy( m_pFileName, pFileName, sizeof( m_pFileName ) );
	Q_FixSlashes( m_pFileName );
	SetDirty( true );
}

//-----------------------------------------------------------------------------
// Dirty bits
//-----------------------------------------------------------------------------
void CPetDoc::SetDirty( bool bDirty )
{
	m_bDirty = bDirty;
}

bool CPetDoc::IsDirty() const
{
	return m_bDirty;
}


//-----------------------------------------------------------------------------
// Creates the root element
//-----------------------------------------------------------------------------
bool CPetDoc::CreateRootElement()
{
	Assert( !m_hRoot.Get() );

	DmFileId_t fileid = g_pDataModel->FindOrCreateFileId( GetFileName() );

	// Create the main element
	m_hRoot = g_pDataModel->CreateElement( "DmElement", GetFileName(), fileid );
	if ( m_hRoot == DMELEMENT_HANDLE_INVALID )
		return false;

	g_pDataModel->SetFileRoot( fileid, m_hRoot );

	// We need to create an element array attribute storing particle system definitions
	m_hRoot->AddAttribute( "particleSystemDefinitions", AT_ELEMENT_ARRAY );
	return true;
}


//-----------------------------------------------------------------------------
// Creates a new document
//-----------------------------------------------------------------------------
void CPetDoc::CreateNew()
{
	Assert( !m_hRoot.Get() );

	// This is not undoable
	CDisableUndoScopeGuard guard;

	Q_strncpy( m_pFileName, "untitled", sizeof( m_pFileName ) );

	// Create the main element
	if ( !CreateRootElement() )
		return;

	SetDirty( false );
}


//-----------------------------------------------------------------------------
// Saves/loads from file
//-----------------------------------------------------------------------------
bool CPetDoc::LoadFromFile( const char *pFileName )
{
	Assert( !m_hRoot.Get() );

	CAppDisableUndoScopeGuard guard( "CPetDoc::LoadFromFile", NOTIFY_CHANGE_OTHER );
	SetDirty( false );

	if ( !pFileName[0] )
		return false;

	Q_strncpy( m_pFileName, pFileName, sizeof( m_pFileName ) );

	CDmElement *pRoot = NULL;
	DmFileId_t fileid = g_pDataModel->RestoreFromFile( pFileName, NULL, NULL, &pRoot, CR_DELETE_OLD );

	if ( fileid == DMFILEID_INVALID )
	{
		m_pFileName[0] = 0;
		return false;
	}

	m_hRoot = pRoot;

	SetDirty( false );
	return true;
}

void CPetDoc::SaveToFile( )
{
	if ( m_hRoot.Get() && m_pFileName && m_pFileName[0] )
	{
		g_pDataModel->SaveToFile( m_pFileName, NULL, "binary", PET_FILE_FORMAT, m_hRoot );
	}

	SetDirty( false );
}

	
//-----------------------------------------------------------------------------
// Returns the root object
//-----------------------------------------------------------------------------
CDmElement *CPetDoc::GetRootObject()
{
	return m_hRoot;
}

	
//-----------------------------------------------------------------------------
// Returns the root object fileid
//-----------------------------------------------------------------------------
DmFileId_t CPetDoc::GetFileId()
{
	return m_hRoot.Get() ? m_hRoot->GetFileId() : DMFILEID_INVALID;
}


//-----------------------------------------------------------------------------
// Returns the particle system definition list
//-----------------------------------------------------------------------------
CDmAttribute *CPetDoc::GetParticleSystemDefinitionList()
{
	CDmrElementArray<> array( m_hRoot, "particleSystemDefinitions" );
	return array.GetAttribute();
}


void CPetDoc::AddNewParticleSystemDefinition( CDmeParticleSystemDefinition *pNew, CUndoScopeGuard &Guard )
{
	CDmrParticleSystemList particleSystemList( GetParticleSystemDefinitionList() );

	particleSystemList.AddToTail( pNew );
	Guard.Release();

	// Force a resolve to get the particle created
	g_pDmElementFramework->Operate( true );
	g_pDmElementFramework->BeginEdit();

	UpdateParticleDefinition( pNew );
}

//-----------------------------------------------------------------------------
// Adds a new particle system definition
//-----------------------------------------------------------------------------
CDmeParticleSystemDefinition* CPetDoc::AddNewParticleSystemDefinition( const char *pName )
{
	if ( !pName || !pName[0] )
	{
		pName = "New Particle System";
	}

	CDmeParticleSystemDefinition *pParticleSystem;
	{
		CAppUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, "Add Particle System", "Add Particle System" );

		pParticleSystem = CreateElement<CDmeParticleSystemDefinition>( pName, GetFileId() );
		AddNewParticleSystemDefinition( pParticleSystem, guard );
	}

	return pParticleSystem;
}


//-----------------------------------------------------------------------------
// Refresh all particle definitions
//-----------------------------------------------------------------------------
void CPetDoc::UpdateAllParticleSystems( )
{
	// Force a resolve to get the particle created
	g_pDmElementFramework->Operate( true );
	g_pDmElementFramework->BeginEdit();

	CDmrParticleSystemList particleSystemList( GetParticleSystemDefinitionList() );
	int nCount = particleSystemList.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		UpdateParticleDefinition( particleSystemList[i] );
	}
}


//-----------------------------------------------------------------------------
// Deletes a particle system definition
//-----------------------------------------------------------------------------
void CPetDoc::DeleteParticleSystemDefinition( CDmeParticleSystemDefinition *pParticleSystem )
{
	if ( !pParticleSystem )
		return;

	CDmrParticleSystemList particleSystemList( GetParticleSystemDefinitionList() );
	int nCount = particleSystemList.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( pParticleSystem == particleSystemList[i] )
		{
			CAppUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, "Delete Particle System", "Delete Particle System" );
			particleSystemList.FastRemove( i );
			break;
		}
	}

	// Find all CDmeParticleChilds referring to this function
	CUtlVector< CDmeParticleChild* > children;
	FindAncestorsReferencingElement( pParticleSystem, children );
	int nChildCount = children.Count();
	for ( int i = 0; i < nChildCount; ++i )
	{
		CDmeParticleChild *pChildReference = children[i];
		CDmeParticleSystemDefinition *pParent = FindReferringElement<CDmeParticleSystemDefinition>( pChildReference, "children" );
		if ( !pParent )
			continue;

		pParent->RemoveFunction( FUNCTION_CHILDREN, pChildReference );
		DestroyElement( pChildReference, TD_NONE );
	}

	DestroyElement( pParticleSystem, TD_DEEP );
}


CDmeParticleSystemDefinition *CPetDoc::FindParticleSystemDefinition( const char *pName )
{
	CDmrParticleSystemList particleSystemList( GetParticleSystemDefinitionList() );
	int nCount = particleSystemList.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeParticleSystemDefinition* pParticleSystem = particleSystemList[i];
		if ( !Q_stricmp( pName, pParticleSystem->GetName() ) ) 
			return pParticleSystem;
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Deletes a particle system definition
//-----------------------------------------------------------------------------
void CPetDoc::ReplaceParticleSystemDefinition( CDmeParticleSystemDefinition *pParticleSystem )
{
	if ( !pParticleSystem )
		return;

	CDmrParticleSystemList particleSystemList( GetParticleSystemDefinitionList() );
	int nCount = particleSystemList.Count();
	int nFoundIndex = -1;
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !particleSystemList[i] )
			continue;

		if ( !Q_stricmp( particleSystemList[i]->GetName(), pParticleSystem->GetName() ) ) 
		{
			nFoundIndex = i;
			break;
		}
	}

	if ( nFoundIndex < 0 )
	{
		CAppUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, "Replace Particle System", "Replace Particle System" );
		CDmrParticleSystemList particleSystemList( GetParticleSystemDefinitionList() );
		pParticleSystem->SetFileId( m_hRoot->GetFileId(), TD_ALL );
		particleSystemList.AddToTail( pParticleSystem );
		return;
	}

	CDmeParticleSystemDefinition *pOldParticleSystem = particleSystemList[nFoundIndex];

	// This can happen if we unserialized w/ replace
	if ( pOldParticleSystem == pParticleSystem )
		return;

	CAppUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, "Replace Particle System", "Replace Particle System" );

	particleSystemList.Set( nFoundIndex, pParticleSystem );
	pParticleSystem->SetFileId( m_hRoot->GetFileId(), TD_ALL );

	// Find all CDmeParticleChilds referring to this function
	CUtlVector< CDmeParticleChild* > children;
	FindAncestorsReferencingElement( pOldParticleSystem, children );
	int nChildCount = children.Count();
	for ( int i = 0; i < nChildCount; ++i )
	{
		CDmeParticleChild *pChildReference = children[i];
		pChildReference->m_Child = pParticleSystem; 
	}

	DestroyElement( pOldParticleSystem, TD_SHALLOW );
}


//-----------------------------------------------------------------------------
// Does a particle system exist already?
//-----------------------------------------------------------------------------
bool CPetDoc::IsParticleSystemDefined( const char *pName )
{
	return FindParticleSystemDefinition( pName ) != NULL;
}


//-----------------------------------------------------------------------------
// Updates a specific particle defintion
//-----------------------------------------------------------------------------
void CPetDoc::UpdateParticleDefinition( CDmeParticleSystemDefinition *pDef )
{
	if ( !pDef )
		return;

	CUtlBuffer buf;
	g_pDataModel->Serialize( buf, "binary", PET_FILE_FORMAT, pDef->GetHandle() );

	// Tell the game about the new definitions
	if ( clienttools )
	{
		clienttools->ReloadParticleDefintions( GetFileName(), buf.Base(), buf.TellMaxPut() );
	}
	if ( servertools )
	{
		servertools->ReloadParticleDefintions( GetFileName(), buf.Base(), buf.TellMaxPut() );
	}

	// Let the other tools know
	KeyValues *pMessage = new KeyValues( "ParticleSystemUpdated" );
	pMessage->SetPtr( "definitionBits", buf.Base() );
	pMessage->SetInt( "definitionSize", buf.TellMaxPut() );
	g_pPetTool->PostMessageToAllTools( pMessage );
	pMessage->deleteThis();
}


//-----------------------------------------------------------------------------
// Populate string choice lists
//-----------------------------------------------------------------------------
bool CPetDoc::GetStringChoiceList( const char *pChoiceListType, CDmElement *pElement, 
									const char *pAttributeName, bool bArrayElement, StringChoiceList_t &list )
{
	if ( !Q_stricmp( pChoiceListType, "particleSystemDefinitions" ) )
	{
		CDmrParticleSystemList particleSystemList( GetParticleSystemDefinitionList() );

		StringChoice_t sChoice;
		sChoice.m_pValue = "";
		sChoice.m_pChoiceString = "";
		list.AddToTail( sChoice );

		int nCount = particleSystemList.Count();
		for ( int i = 0; i < nCount; ++i )
		{
			CDmeParticleSystemDefinition *pParticleSystem = particleSystemList[ i ];

			StringChoice_t sChoice;
			sChoice.m_pValue = pParticleSystem->GetName();
			sChoice.m_pChoiceString = pParticleSystem->GetName();
			list.AddToTail( sChoice );
		}
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Populate element choice lists
//-----------------------------------------------------------------------------
bool CPetDoc::GetElementChoiceList( const char *pChoiceListType, CDmElement *pElement, 
									 const char *pAttributeName, bool bArrayElement, ElementChoiceList_t &list )
{
	if ( !Q_stricmp( pChoiceListType, "allelements" ) )
	{
		AddElementsRecursively( m_hRoot, list );
		return true;
	}

	if ( !Q_stricmp( pChoiceListType, "particleSystemDefinitions" ) )
	{
		CDmrParticleSystemList particleSystemList( GetParticleSystemDefinitionList() );

		int nCount = particleSystemList.Count();
		for ( int i = 0; i < nCount; ++i )
		{
			CDmeParticleSystemDefinition *pParticleSystem = particleSystemList[ i ];
			ElementChoice_t sChoice;
			sChoice.m_pValue = pParticleSystem;
			sChoice.m_pChoiceString = pParticleSystem->GetName();
			list.AddToTail( sChoice );
		}
		return ( nCount > 0 );
	}

	// by default, try to treat the choice list type as a Dme element type
	AddElementsRecursively( m_hRoot, list, pChoiceListType );

	return list.Count() > 0;
}

	
//-----------------------------------------------------------------------------
// Called when data changes
//-----------------------------------------------------------------------------
void CPetDoc::OnDataChanged( const char *pReason, int nNotifySource, int nNotifyFlags )
{
	SetDirty( nNotifyFlags & NOTIFY_SETDIRTYFLAG ? true : false );
	m_pCallback->OnDocChanged( pReason, nNotifySource, nNotifyFlags );
}


