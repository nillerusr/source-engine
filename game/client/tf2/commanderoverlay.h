//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $NoKeywords: $
//=============================================================================//

#if !defined( COMMANDEROVERLAY_H )
#define COMMANDEROVERLAY_H

#ifdef _WIN32
#pragma once
#endif

#include "panelmetaclassmgr.h"


//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
class C_BaseEntity;
class KeyValues;


//-----------------------------------------------------------------------------
// Overlay handle
//-----------------------------------------------------------------------------

typedef unsigned short OverlayHandle_t;
enum
{
	OVERLAY_HANDLE_INVALID = (OverlayHandle_t)~0
};

//-----------------------------------------------------------------------------
// Purpose: Singleton class responsible for managing overlay elements 
//-----------------------------------------------------------------------------
class IHudCommanderOverlayMgr
{
public:
//	// Call this when the game starts up + shuts down
	virtual void GameInit() = 0;
	virtual void GameShutdown() = 0;

	// Call this when the level starts up + shuts down
	virtual void LevelInit( ) = 0;
	virtual void LevelShutdown() = 0;

	// add an overlay element to the commander mode, returns a handle to it
	virtual OverlayHandle_t AddOverlay( char const* pOverlayName, C_BaseEntity* pEntity, vgui::Panel *pParent = NULL ) = 0;

	// removes a particular overlay
	virtual void RemoveOverlay( OverlayHandle_t handle ) = 0;

	// Call this once a frame...
	virtual void Tick( ) = 0;

	// Call this when commander mode is enabled or disabled
	virtual void Enable( bool enable ) = 0;

protected:
	// Don't delete me!
	virtual ~IHudCommanderOverlayMgr() {}
};


//-----------------------------------------------------------------------------
// Returns the singleton commander overlay interface
//-----------------------------------------------------------------------------
IHudCommanderOverlayMgr* HudCommanderOverlayMgr();


//-----------------------------------------------------------------------------
// Helper class for entities to join the list of entities to render on screen
//-----------------------------------------------------------------------------
class CPanelRegistration
{
public:
	CPanelRegistration( ) : m_Overlay(OVERLAY_HANDLE_INVALID) {}

	~CPanelRegistration()
	{
		HudCommanderOverlayMgr()->RemoveOverlay( m_Overlay );
	}

	void Activate( C_BaseEntity* pEntity, char const* pOverlayName, bool active )
	{
		if( active )
		{
			AddOverlay( pEntity, pOverlayName );
		}
		else
		{
			RemoveOverlay();
		}
	}

	void Activate( C_BaseEntity* pEntity, char const* pOverlayName, vgui::Panel *pParent, int sortOrder, bool active )
	{
		if( active )
		{
			AddOverlay( pEntity, pOverlayName, pParent );
		}
		else
		{
			RemoveOverlay();
		}
	}


	void AddOverlay( C_BaseEntity *pEntity, const char *pOverlayName, vgui::Panel *pParent = NULL )
	{
		RemoveOverlay();
		m_Overlay = HudCommanderOverlayMgr()->AddOverlay( pOverlayName, pEntity, pParent );
	}

	void RemoveOverlay()
	{
		if( m_Overlay != OVERLAY_HANDLE_INVALID )
		{
			HudCommanderOverlayMgr()->RemoveOverlay( m_Overlay );
			m_Overlay = OVERLAY_HANDLE_INVALID;
		}
	}


private:
	OverlayHandle_t m_Overlay;
};


//-----------------------------------------------------------------------------
// Macros for help with simple registration of panels
// Put DECLARE_ENTITY_PANEL() in your class definition
// and ENTITY_PANEL_ACTIVATE( "name" ) in the entity's SetDormant call
//-----------------------------------------------------------------------------
#define DECLARE_ENTITY_PANEL()							CPanelRegistration m_OverlayPanel
#define ENTITY_PANEL_ACTIVATE( _pOverlayName, _active )	m_OverlayPanel.Activate( this, _pOverlayName, _active )


//-----------------------------------------------------------------------------
// Helper macro to make overlay factories one line of code. Use like this:
//	DECLARE_OVERLAY_FACTORY( CEntityImagePanel, "image" );
//-----------------------------------------------------------------------------
#define DECLARE_OVERLAY_FACTORY( _PanelClass, _nameString )	\
	DECLARE_PANEL_FACTORY( _PanelClass, C_BaseEntity, _nameString )

#define DECLARE_OVERLAY_POINT_FACTORY( _PanelClass, _nameString )	\
	DECLARE_PANEL_FACTORY( _PanelClass, void, _nameString )


#endif // COMMANDEROVERLAY_H