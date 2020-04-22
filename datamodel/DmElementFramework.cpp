//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "DmElementFramework.h"
#include "datamodel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CDmElementFramework g_DmElementFramework;
CDmElementFramework *g_pDmElementFrameworkImp = &g_DmElementFramework;
IDmElementFramework *g_pDmElementFramework = &g_DmElementFramework;


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CDmElementFramework::CDmElementFramework() : m_phase( PH_EDIT ), m_dirtyElements( 128, 256 ) 
{
}

	
//-----------------------------------------------------------------------------
// Methods of IAppSystem
//-----------------------------------------------------------------------------
bool CDmElementFramework::Connect( CreateInterfaceFn factory )
{
	return true;
}

void CDmElementFramework::Disconnect()
{
}

void *CDmElementFramework::QueryInterface( const char *pInterfaceName )
{
	if ( !V_strcmp( pInterfaceName, VDMELEMENTFRAMEWORK_VERSION ) )
		return (IDmElementFramework*)this;

	return NULL;
}

InitReturnVal_t CDmElementFramework::Init( )
{
	return INIT_OK;
}

void CDmElementFramework::Shutdown()
{
	m_dependencyGraph.Cleanup();
}


//-----------------------------------------------------------------------------
// element framework phase transition methods
//-----------------------------------------------------------------------------
void CDmElementFramework::EditApply()
{
	g_pDataModelImp->RemoveUnreferencedElements();
}

void CDmElementFramework::Resolve( bool clearDirtyFlags )
{
	int nCount = m_dirtyElements.Count();
	for ( int ei = 0; ei < nCount; ++ei )
	{
		DmElementHandle_t h = m_dirtyElements[ ei ];
		CDmElement *pElement = g_pDataModel->GetElement( h );
		if ( !pElement )
			continue;

		pElement->Resolve();

		if ( clearDirtyFlags )
		{
			CDmeElementAccessor::MarkDirty( pElement, false ); // marks element clean
			CDmeElementAccessor::MarkAttributesClean( pElement ); // marks all attributes clean
		}
	}

	if ( clearDirtyFlags )
	{
		m_dirtyElements.RemoveAll();
	}
}


//-----------------------------------------------------------------------------
// Returns the current phase
//-----------------------------------------------------------------------------
DmPhase_t CDmElementFramework::GetPhase()
{
	return FastGetPhase();
}

void CDmElementFramework::SetOperators( const CUtlVector< IDmeOperator* > &operators )
{
	VPROF( "CDmElementFramework::SetOperators()" );
	m_dependencyGraph.Reset( operators );
}

void CDmElementFramework::BeginEdit()
{
	Assert( m_phase == PH_EDIT || m_phase == PH_OUTPUT );

	if ( m_phase == PH_EDIT )
	{
		m_phase = PH_EDIT_APPLY;
		EditApply();

		m_phase = PH_EDIT_RESOLVE;
		Resolve( false );
	}

	m_phase = PH_EDIT;
}

void CDmElementFramework::Operate( bool bResolve )
{
	VPROF( "CDmElementFramework::Operate" );

	Assert( m_phase == PH_EDIT || m_phase == PH_OUTPUT );

	if ( m_phase == PH_EDIT )
	{
		{
			VPROF( "CDmElementFramework::PH_EDIT_APPLY" );
			m_phase = PH_EDIT_APPLY;
			EditApply();
		}

		{
			VPROF( "CDmElementFramework::PH_EDIT_RESOLVE" );
			m_phase = PH_EDIT_RESOLVE;
			Resolve( false );
		}
	}

	{
		VPROF( "CDmElementFramework::PH_DEPENDENCY" );
		m_phase = PH_DEPENDENCY;
		bool cycle = m_dependencyGraph.CullAndSortOperators();
		if ( cycle )
		{
			Warning( "Operator cycle found during dependency graph traversal!\n" );
		}
	}

	{
		VPROF( "CDmElementFramework::PH_OPERATE" );
		m_phase = PH_OPERATE;
		const CUtlVector< IDmeOperator* > &operatorsToRun = m_dependencyGraph.GetSortedOperators();
		uint on = operatorsToRun.Count();
		for ( uint oi = 0; oi < on; ++oi )
		{
			operatorsToRun[ oi ]->Operate();
		}
	}

	if ( bResolve )
	{
		VPROF( "CDmElementFramework::PH_OPERATE_RESOLVE" );
		m_phase = PH_OPERATE_RESOLVE;
		Resolve( true );

		m_phase = PH_OUTPUT;
	}
}

void CDmElementFramework::Resolve()
{
	VPROF( "CDmElementFramework::Resolve" );

	Assert( m_phase == PH_OPERATE );

	m_phase = PH_OPERATE_RESOLVE;
	Resolve( true );

	m_phase = PH_OUTPUT;
}

void CDmElementFramework::AddElementToDirtyList( DmElementHandle_t hElement )
{
	m_dirtyElements.AddToTail( hElement );
}

void CDmElementFramework::RemoveCleanElementsFromDirtyList()
{
	int nCount = m_dirtyElements.Count();
	while ( --nCount >= 0 )
	{
		DmElementHandle_t h = m_dirtyElements[ nCount ];
		CDmElement *pElement = g_pDataModel->GetElement( h );
		if ( !pElement )
			continue;

		if ( !CDmeElementAccessor::IsDirty( pElement ) )
		{
			m_dirtyElements.FastRemove( nCount );
		}
	}
}
