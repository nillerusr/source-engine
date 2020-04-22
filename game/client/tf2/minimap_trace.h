//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef MINIMAP_ENTITY_H
#define MINIMAP_ENTITY_H
#ifdef _WIN32
#pragma once
#endif


#include "hud_minimap.h"
#include "TeamBitmapImage.h"
#include "vgui_bitmapimage.h"
#include <vgui_controls/Panel.h>


// FIXME: Need to put the team color somewhere...
// Get the team color...
extern int g_TeamColor[3][3];


//-----------------------------------------------------------------------------
// A base minimap panel meant to be attached to an entity.
// NOTE: paint() will never be called unless the entity is currently valid
//-----------------------------------------------------------------------------
class C_BaseEntity;

class CMinimapTracePanel : public vgui::Panel
{
	DECLARE_CLASS_GAMEROOT( CMinimapTracePanel, vgui::Panel );

public:
				CMinimapTracePanel( vgui::Panel *parent, const char *panelName );
	virtual		~CMinimapTracePanel();

	bool	Init( KeyValues* pKeyValues, MinimapInitData_t* pMinimapData );

	// Causes the minimap panel to not be visible
	void	SetTraceVisibility( bool bVisible );

	// called when we're ticked...
	virtual void	OnTick( void );

protected:
	// Can be overridden; it's called once a frame and returns true if the
	// object is visible to the minimap or not.	X and Y are the position of the
	// entity in minimap space
	virtual bool ComputeVisibility( );

	// Computes a panel alpha based on zoom level...
	float ComputePanelAlpha();

	// Called by derived classes before drawing to determine if the entity is visible.
	// Also sets the vgui surface color based on the team.
	bool	BeginRender( CMinimapPanel* pPanel, float &x, float &y, int &team ) { return false; }

	// Computes the entity position in minimap panel coordinates
	bool	GetEntityPosition( float &x, float &y );

	// Sets the position of the trace in world space
	void	SetPosition( const Vector &vecPosition );

	// Entity accessors
	C_BaseEntity* GetEntity();
	void SetEntity( C_BaseEntity* pEntity );

protected:
	// Bitmap positional offset
	int m_OffsetX;
	int m_OffsetY;

	int m_SizeW;
	int m_SizeH;

	// Indicates we should clip to the map
	bool	m_bClipToMap;

	bool	m_bCanScale;

private:
	// Indicates if we're faded out when zoomed in
	bool	m_bVisibleWhenZoomedIn;

	// Indicates we should always draw, even if we're off the map
	bool	m_bClampToMap;

	// Are we invisible because the entity wants us invisible?
	bool	m_bVisible;

	// This is the entity to which we're attached
	C_BaseEntity *m_pEntity;

	// This is the location of the panel in world space
	Vector	m_vecPosition;

	// lifetime of the minimap panel
	float	m_flDeletionTime;
};


//-----------------------------------------------------------------------------
// A standard minimap panel that displays a bitmap
//-----------------------------------------------------------------------------
class CMinimapTraceBitmapPanel : public CMinimapTracePanel
{
	DECLARE_CLASS( CMinimapTraceBitmapPanel, CMinimapTracePanel );

public:
	CMinimapTraceBitmapPanel( vgui::Panel *parent, const char *panelName )
		: BaseClass ( parent, panelName )
	{
	}

	// Sets the bitmap and size
	virtual bool Init( KeyValues* pKeyValues, MinimapInitData_t* pInitData );

	// Performs the rendering...
 	virtual void Paint();

protected:
	BitmapImage m_Image;
};


//-----------------------------------------------------------------------------
// A standard minimap panel that displays a bitmap based on entity team
//-----------------------------------------------------------------------------
class CMinimapTraceTeamBitmapPanel : public CMinimapTracePanel
{
	DECLARE_CLASS( CMinimapTraceTeamBitmapPanel, CMinimapTracePanel );

public:
	CMinimapTraceTeamBitmapPanel( vgui::Panel *parent, const char *panelName )
		: BaseClass ( parent, panelName )
	{
	}

	// Sets the bitmap and size
	virtual bool Init( KeyValues* pKeyValues, MinimapInitData_t* pInitData );

	// Performs the rendering...
 	virtual void Paint();

protected:
	CTeamBitmapImage m_TeamImage;
};


#endif // MINIMAP_ENTITY_H
