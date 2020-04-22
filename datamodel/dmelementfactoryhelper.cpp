//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "datamodel/dmelementfactoryhelper.h"
#include "tier0/dbg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CDmElementFactoryHelper *CDmElementFactoryHelper::s_pHelpers[2] = { NULL, NULL };


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CDmElementFactoryHelper::CDmElementFactoryHelper( const char *classname, IDmElementFactoryInternal *pFactory, bool bIsStandardFactory )
{
	m_pNext = s_pHelpers[bIsStandardFactory];
	s_pHelpers[bIsStandardFactory] = this;

	// Set attributes
	Assert( pFactory );
	m_pFactory = pFactory;
	Assert( classname );
	m_pszClassname	= classname;
}

//-----------------------------------------------------------------------------
// Purpose: Returns next object in list
// Output : CDmElementFactoryHelper
//-----------------------------------------------------------------------------
CDmElementFactoryHelper *CDmElementFactoryHelper::GetNext( void )
{ 
	return m_pNext;
}


//-----------------------------------------------------------------------------
// Installs all factories into the datamodel system
//-----------------------------------------------------------------------------
// NOTE: The name of this extern is defined by the macro IMPLEMENT_ELEMENT_FACTORY 
extern CDmElementFactoryHelper g_CDmElement_Helper;

void CDmElementFactoryHelper::InstallFactories( )
{
	// Just set up the type symbols of the other factories
	CDmElementFactoryHelper *p = s_pHelpers[0];
	while ( p )
	{
		// Add factories to database
		if ( !p->GetFactory()->IsAbstract() )
		{
			g_pDataModel->AddElementFactory( p->GetClassname(), p->GetFactory() );
		}

		// Set up the type symbol. Note this can't be done at
		// constructor time since we don't have a DataModel pointer then
		p->GetFactory()->SetElementTypeSymbol( g_pDataModel->GetSymbol( p->GetClassname() ) );
		p = p->GetNext();
	}

	p = s_pHelpers[1];
	while ( p )
	{
		// Add factories to database, but not if they've been overridden
		if ( !g_pDataModel->HasElementFactory( p->GetClassname() ) )
		{
			if ( !p->GetFactory()->IsAbstract() )
			{
				g_pDataModel->AddElementFactory( p->GetClassname(), p->GetFactory() );
			}

			// Set up the type symbol. Note this can't be done at
			// constructor time since we don't have a DataModel pointer then

			// Backward compat--don't let the type symbol be 'DmeElement'
			if ( Q_stricmp( p->GetClassname(), "DmeElement" ) )
			{
				p->GetFactory()->SetElementTypeSymbol( g_pDataModel->GetSymbol( p->GetClassname() ) );
			}
		}

		p = p->GetNext();
	}

	// Also install the DmElement factory as the default factory
	g_pDataModel->SetDefaultElementFactory( g_CDmElement_Helper.GetFactory() );
}


//-----------------------------------------------------------------------------
// Installs all DmElement factories
//-----------------------------------------------------------------------------
void InstallDmElementFactories( )
{
	CDmElementFactoryHelper::InstallFactories( );
}
