//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "CommanderOverlay.h"
#include "utlsymbol.h"

//-----------------------------------------------------------------------------
// Purpose: Singleton class responsible for managing overlay elements 
//-----------------------------------------------------------------------------
					  
class CHudCommanderOverlayMgrImp : public IHudCommanderOverlayMgr
{
public:
	// constructor, destructor
	CHudCommanderOverlayMgrImp();
	virtual ~CHudCommanderOverlayMgrImp();

	// Call this when the game starts up + shuts down
	virtual void GameInit();
	virtual void GameShutdown();

	// Call this when the level starts up + shuts down
	virtual void LevelInit( );
	virtual void LevelShutdown();

	// add an overlay element to the commander mode
	virtual OverlayHandle_t AddOverlay( char const* pOverlayName, C_BaseEntity* pEntity, vgui::Panel *pParent );

	// removes a particular overlay
	virtual void RemoveOverlay( OverlayHandle_t handle );

	// Call this once a frame...
	virtual void Tick( );

	// Call this when commander mode is enabled or disabled
	virtual void Enable( bool enable );

private:
	struct OverlayPanel_t
	{
		unsigned short	m_Team;
		CUtlSymbol		m_Name;
		vgui::Panel		*m_pPanel;
		vgui::Panel		*m_pParentPanel;
		EHANDLE			m_hEntity;
	};

	// Create, destroy panels
	void CreatePanel( int overlay );
	void DestroyPanel( int overlay );

	// No copy constructor
	CHudCommanderOverlayMgrImp( const CHudCommanderOverlayMgrImp& );

	// List of actual overlays currently in service
	// Linked list used so handle ids don't change
	CUtlLinkedList< OverlayPanel_t, unsigned short > m_Overlays;
};


//-----------------------------------------------------------------------------
// Returns the singleton commander overlay interface
//-----------------------------------------------------------------------------
IHudCommanderOverlayMgr* HudCommanderOverlayMgr()
{
	static CHudCommanderOverlayMgrImp s_OverlayImp;
	return &s_OverlayImp;
}


//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CHudCommanderOverlayMgrImp::CHudCommanderOverlayMgrImp()
{
}

CHudCommanderOverlayMgrImp::~CHudCommanderOverlayMgrImp()
{
}


//-----------------------------------------------------------------------------
// Call this when the game starts up or shuts down...
//-----------------------------------------------------------------------------
#define OVERLAY_FILE	"scripts/commander_overlays.txt"

void CHudCommanderOverlayMgrImp::GameInit()
{
	// Read in all overlay metaclasses...
	PanelMetaClassMgr()->LoadMetaClassDefinitionFile( OVERLAY_FILE );
}

void CHudCommanderOverlayMgrImp::GameShutdown()
{
}


//-----------------------------------------------------------------------------
// Call this when the level starts up + shuts down
//-----------------------------------------------------------------------------
void CHudCommanderOverlayMgrImp::LevelInit( )
{
}


void CHudCommanderOverlayMgrImp::LevelShutdown()
{
	// Remove all panels....
	for( unsigned short i = m_Overlays.Head(); i != m_Overlays.InvalidIndex(); i = m_Overlays.Next(i) )
	{
		DestroyPanel(i);
	}
	m_Overlays.RemoveAll();
}


//-----------------------------------------------------------------------------
// Create, destroy panel...
//-----------------------------------------------------------------------------
void CHudCommanderOverlayMgrImp::CreatePanel( int overlay )
{
	Assert( m_Overlays.IsValidIndex(overlay) );
	OverlayPanel_t& panel = m_Overlays[overlay];

	// Chain keyvalues with name Team#
	char pTeamName[6] = "Team ";
	pTeamName[4] = '0' + panel.m_Team;

	vgui::Panel *pPanel = PanelMetaClassMgr()->CreatePanelMetaClass( 
		panel.m_Name.String(), 0, panel.m_hEntity.Get(), panel.m_pParentPanel, pTeamName );
	m_Overlays[overlay].m_pPanel = pPanel;
}

void CHudCommanderOverlayMgrImp::DestroyPanel( int overlay )
{
	Assert( m_Overlays.IsValidIndex(overlay) );
	OverlayPanel_t& panel = m_Overlays[overlay];
	if (panel.m_pPanel)
	{
		PanelMetaClassMgr()->DestroyPanelMetaClass(	panel.m_pPanel );
		m_Overlays[overlay].m_pPanel = NULL;
	}
}


//-----------------------------------------------------------------------------
// add an overlay element to the commander mode
//-----------------------------------------------------------------------------
OverlayHandle_t CHudCommanderOverlayMgrImp::AddOverlay( 
	char const* pOverlayInstance, C_BaseEntity* pEntity, vgui::Panel *pParent )
{
	OverlayPanel_t newPanel;
	newPanel.m_Name = pOverlayInstance;
	newPanel.m_pPanel = NULL;
	newPanel.m_Team = (unsigned short)~0;
	newPanel.m_hEntity = pEntity;
	newPanel.m_pParentPanel = pParent;
	return m_Overlays.AddToTail( newPanel );
}


//-----------------------------------------------------------------------------
// removes a particular overlay
//-----------------------------------------------------------------------------
void CHudCommanderOverlayMgrImp::RemoveOverlay( OverlayHandle_t handle )
{
	if ((handle == OVERLAY_HANDLE_INVALID) || (!m_Overlays.IsValidIndex(handle)) )
		return;

	// Deallocate the panel
	DestroyPanel( handle );

	// Remove it from our list
	m_Overlays.Remove( handle );
}


//-----------------------------------------------------------------------------
// make our sprites visible when we are enabled
//-----------------------------------------------------------------------------
void CHudCommanderOverlayMgrImp::Enable( bool enable )
{
}


//-----------------------------------------------------------------------------
// Call this once a frame...
//-----------------------------------------------------------------------------
void CHudCommanderOverlayMgrImp::Tick( )
{
	if ( !engine->IsInGame() )
		return;

	// Iterate through all overlays
	for( unsigned short i = m_Overlays.Head(); i != m_Overlays.InvalidIndex(); i = m_Overlays.Next(i) )
	{
		// Kill panels attached to dead entities...
		if (!m_Overlays[i].m_hEntity)
		{
			DestroyPanel( i );
			continue;
		}

		// make sure the panels are up-to-date based on current entity team
		if (m_Overlays[i].m_Team != m_Overlays[i].m_hEntity->GetTeamNumber())
		{
			// Destroy the old team panel, create the new team panel
			DestroyPanel( i );
			m_Overlays[i].m_Team = m_Overlays[i].m_hEntity->GetTeamNumber();
			CreatePanel( i );
		}
	}
}

