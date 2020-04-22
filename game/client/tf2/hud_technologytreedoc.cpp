//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hud_technologytreedoc.h"
#include "hud.h"
#include "hud_macros.h"
#include "techtree.h"
#include "iclientmode.h"
#include "hud_commander_statuspanel.h"
#include "clientmode_commander.h"
#include "commanderoverlaypanel.h"
#include "tf_hints.h"
#include "c_tf_hintmanager.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static CTechnologyTreeDoc s_TechnologyTreeDoc;

// Hook network messages
DECLARE_MESSAGE( s_TechnologyTreeDoc, Technology )

// Create object singleton on stack
CTechnologyTreeDoc& GetTechnologyTreeDoc()
{
	return s_TechnologyTreeDoc;
}


//-----------------------------------------------------------------------------
// Purpose: Construction
//-----------------------------------------------------------------------------
CTechnologyTreeDoc::CTechnologyTreeDoc( void )
{
	m_pTree = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Destruction
//-----------------------------------------------------------------------------
CTechnologyTreeDoc::~CTechnologyTreeDoc( void )
{
	delete m_pTree;
}

//-----------------------------------------------------------------------------
// Purpose: Initialize the panel
//-----------------------------------------------------------------------------
void CTechnologyTreeDoc::Init( void )
{
	ReloadTechTree();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTechnologyTreeDoc::LevelInit( void )
{
	if ( m_pTree )
	{
		m_pTree->SetPreferredTechnology( NULL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTechnologyTreeDoc::LevelShutdown( void )
{
	if ( m_pTree )
	{
		m_pTree->SetPreferredTechnology( NULL );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTechnologyTreeDoc::ReloadTechTree( void )
{
	// FIXME, CTechnologyTreeDoc should be an entity /MO
	HOOK_HUD_MESSAGE( s_TechnologyTreeDoc, Technology );

	// Reconstruct the tech tree
	delete m_pTree;

	// FIXME: If we reactivate this, we'll need to revisit team number here...
	m_pTree = new CTechnologyTree( ::filesystem, 0);
	Assert( m_pTree );

	m_pTree->SetPreferredTechnology( NULL );
}


//-----------------------------------------------------------------------------
// Purpose: Receive hud update message from server
// Input  : *pszName - 
//			iSize - 
//			*pbuf - 
// Output : int
//-----------------------------------------------------------------------------
int CTechnologyTreeDoc::MsgFunc_Technology(bf_read &msg)
{
	int index;
	int available;
	int voters;
	float resourcelevel;
	bool preferred;

	// Which tech
	index		= msg.ReadByte();
	// Available to this team?
	available	= msg.ReadByte();
	// # of players indicating this as their preferred tech for new spending
	voters		= msg.ReadByte();

	preferred = ( voters & 0x80 ) ? true : false;
	voters &= 0x7f;

	resourcelevel = (float)msg.ReadShort();
	
	// Look it up by index
	CBaseTechnology *item = m_pTree->GetTechnology( index );
	if ( item )
	{	
		bool wasactive = item->GetActive();

		bool justactivated = !wasactive && available;

		// Set data elements
		item->SetActive( available ? true : false );
		item->SetVoters( voters );
		item->SetResourceLevel( resourcelevel );

		// If this is the tech I am voting for, clear my vote
		if ( preferred )
		{
			// Sets the flag on the item, too
			m_pTree->SetPreferredTechnology( item );
		}
		else
		{
			// Force deselection in case there is no active preference on the server any more
			item->SetPreferred( false );
		}

		if ( justactivated && item->GetLevel() > 0 && !item->GetHintsGiven( TF_HINT_NEWTECHNOLOGY ) )
		{
			// So we only give this hint once this game, even if we respawn, etc.
			item->SetHintsGiven( TF_HINT_NEWTECHNOLOGY, true );
			// Note, only show a max of three or 4 newtechnology hints at a time
			CreateGlobalHint( TF_HINT_NEWTECHNOLOGY, item->GetPrintName(), index, 3 );
		}
	}
	
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: Add a file of technologies to the technology tree
//-----------------------------------------------------------------------------
void CTechnologyTreeDoc::AddTechnologyFile( char *sFilename )
{
	// Add the technologies to the tech list
	if ( m_pTree )
	{
		// FIXME: If we reactivate this, we'll need to revisit team number here...
		m_pTree->AddTechnologyFile( ::filesystem, 0, sFilename );
	}
}
