//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

//-----------------------------------------------------------------------------
// The panel responsible for rendering the 3D view in orthographic mode
//-----------------------------------------------------------------------------
#include "cbase.h"
#include <vgui/VGUI.h>
#include <vgui_controls/Controls.h>
#include <vgui/IVGui.h>
#include <vgui/IInput.h>
#include <vgui/ISurface.h>
#include "clientmode_commander.h"
#include "vgui_int.h"
#include "iinput.h"
#include "kbutton.h"
#include "hud_minimap.h"
#include "usercmd.h"
#include "mapdata.h"
#include "c_basetfplayer.h"
#include "view.h"
#include "view_shared.h"
#include "CommanderOverlay.h"
#include "C_TfTeam.h"
#include <vgui/MouseCode.h>
#include <vgui/KeyCode.h>
#include <vgui/IPanel.h>
#include "commanderoverlaypanel.h"
#include "PixelWriter.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"
#include "vtf/vtf.h"
#include "engine/ivdebugoverlay.h"


static inline int AlphaMapIndex(int x, int y)
{
	return y * FOG_ALPHAMAP_SIZE + x;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *commander - 
//-----------------------------------------------------------------------------
void CCommanderOverlayPanel::SetCommanderView( CClientModeCommander *commander )
{
	m_pCommanderView = commander;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCommanderOverlayPanel::CCommanderOverlayPanel( void ) :
	vgui::Panel( NULL, "CommanderOverlayPanel" ),
	m_CursorCommander(vgui::dc_arrow),
	m_CursorRightMouseMove(vgui::dc_hand)
{
	MakePopup();

	SetPaintBackgroundEnabled( false );
	m_left.m_bMouseDown = false;
	m_left.m_nXStart = 0;
	m_left.m_nYStart = 0;

	m_right.m_bMouseDown = false;
	m_right.m_nXStart = 0;
	m_right.m_nYStart = 0;

	m_fZoom = 0;

	m_flPreviousMaxWorldWidth = 0.0f;
	SetVisible( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCommanderOverlayPanel::~CCommanderOverlayPanel( void )
{
}


//-----------------------------------------------------------------------------
// Initialize rendering origin 
//-----------------------------------------------------------------------------
void CCommanderOverlayPanel::InitializeRenderOrigin()
{
	// Initializes the rendering origin
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( player )
	{
		m_vecTacticalOrigin = player->GetAbsOrigin();
	}
	else
	{
		MapData().GetMapOrigin( m_vecTacticalOrigin );
	}

	Vector mins, maxs;
	MapData().GetMapBounds( mins, maxs );
	m_vecTacticalOrigin.z = maxs.z + TACTICAL_ZOFFSET;
	m_vecTacticalAngles.Init( 90, 90, 0 );
	BoundOrigin( m_vecTacticalOrigin );
}


//-----------------------------------------------------------------------------
// Computes the view range: 
//-----------------------------------------------------------------------------
void CCommanderOverlayPanel::ComputeZoomRange()
{
	// This is 
	m_MinWorldWidth = TACTICAL_MIN_VIEWABLE_SIZE;

	// Get the world size
	Vector mins, maxs, size;
	MapData().GetMapBounds( mins, maxs );
	VectorSubtract( maxs, mins, size );
	float worldAspect = size[0] / size[1];

//	size[ 0 ] *= 1.05f;
//	size[ 1 ] *= 1.05f;

	// Find out the panel aspect ratio
	int w, h;
	GetVisibleSize( w, h );

	float panelAspect = (float)w / (float)h;

	// Store off old zoom
	m_flPreviousMaxWorldWidth = m_MaxWorldWidth;

	if (panelAspect > worldAspect)
	{
		// In this case, to see the whole map, 
		// we'll have black areas on the left + right sides
		// Make sure we choose a width big enough to display the entire height
		m_MaxWorldWidth = size[1] * panelAspect;
	}
	else
	{
		// In this case, to see the whole map, 
		// we'll have black areas on the top + bottom
		// Make sure we choose a width big enough to display the entire height
		m_MaxWorldWidth = size[0];
	}

	// if flipping open/close the tech tree, preserver relative zoom amount
	if ( m_flPreviousMaxWorldWidth )
	{
		float ratio = m_MaxWorldWidth / m_flPreviousMaxWorldWidth;

		m_fZoom *= ratio;
	}
}	

//-----------------------------------------------------------------------------
// Call when the map changes
//-----------------------------------------------------------------------------

void CCommanderOverlayPanel::LevelInit( char const* pMapName )
{
	// Always look at the entire map to start with
	m_fZoom = 0;
	m_flPreviousMaxWorldWidth = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCommanderOverlayPanel::LevelShutdown( void )
{
}

//-----------------------------------------------------------------------------
// call these when commander view is enabled/disabled
//-----------------------------------------------------------------------------
void CCommanderOverlayPanel::Enable()
{
	vgui::VPANEL pRoot = VGui_GetClientDLLRootPanel();

	SetCursor(m_CursorCommander);
	SetVisible( true );

	// Make the viewport fill the root panel.
	if( pRoot)
	{
		int wide, tall;
		vgui::ipanel()->GetSize(pRoot, wide, tall);

		// subtract out the technology UI size
		tall -= 0;// xxx200

		SetBounds(0, 0, wide, tall);
	}

	// Cache the orthographic view size range
	ComputeZoomRange();

	// Start out looking at the whole world
	if (m_fZoom == 0)
		m_fZoom = m_MaxWorldWidth;

	// clamp
	if (m_fZoom < m_MinWorldWidth)
		m_fZoom = m_MinWorldWidth;
	else if (m_fZoom > m_MaxWorldWidth)
		m_fZoom = m_MaxWorldWidth;

	// Figure out where the camera is
	InitializeRenderOrigin();

	vgui::ivgui()->AddTickSignal( GetVPanel() );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCommanderOverlayPanel::Disable()
{
	SetVisible( false );
	// FIXME: Need a removeTickSignal!
//	vgui::ivgui()->removeTickSignal( GetVPanel() );
}


//-----------------------------------------------------------------------------
// called when we're ticked...
//-----------------------------------------------------------------------------
void CCommanderOverlayPanel::OnTick()
{
	if (!IsLocalPlayerInTactical() || !engine->IsInGame())
		return;
}


//-----------------------------------------------------------------------------
// Purpose: Returns up to 32 players encoded as a bit per player in a 32 bit unsigned
// Output : unsigned int
//-----------------------------------------------------------------------------
void CCommanderOverlayPanel::GetSelectedPlayerBitField( CBitVec< MAX_PLAYERS >& bits )
{
	CMapPlayers *pPlayer;

	bits.ClearAll();

	// draw all the visible players
	for ( int playerNum = 0; playerNum < MAX_PLAYERS; playerNum++ )
	{
		pPlayer = &MapData().m_Players[ playerNum ];
		if ( pPlayer->m_bSelected )
		{
			bits.Set( playerNum );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Counts how many players are selected
// Output : int
//-----------------------------------------------------------------------------
int CCommanderOverlayPanel::CountSelectedPlayers( void )
{
	CMapPlayers *pPlayer;

	// bitfield variable
	int count = 0;

	// draw all the visible players
	for ( int playerNum = 0; playerNum < MAX_PLAYERS; playerNum++ )
	{
		pPlayer = &MapData().m_Players[ playerNum ];
		if ( pPlayer->m_bSelected )
		{
			count++;
		}
	}

	return count;
}

//-----------------------------------------------------------------------------
// Purpose: Called when a key is pressed
//-----------------------------------------------------------------------------
void CCommanderOverlayPanel::OnKeyPressed(vgui::KeyCode code)
{
	switch(code)
	{
	case vgui::KEY_SPACE:
		if (m_fZoom != m_MaxWorldWidth)
		{
			ChangeZoomLevel(m_MaxWorldWidth);
		}
		else
		{
			Vector mouseWorldPos;
			CenterOnMouse( mouseWorldPos );

			ChangeZoomLevel(TACTICAL_SPACEBAR_VIEWABLE_SIZE);

			// Place mouse over me
			m_pCommanderView->MoveMouse( mouseWorldPos );
		}
		break;

	default:
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: when space is hit, show the entire view
//-----------------------------------------------------------------------------
void CCommanderOverlayPanel::ChangeZoomLevel( float newZoom )
{
	// Make sure the mouse remains over the thing it started over

	// Figure out worldspace movement based on picking ray
	Vector rayOrigin, rayForward;

	int mx, my;
	GetMousePos( mx, my );

	// Need to convert from screen space back to a worldspace ray
	CreatePickingRay( 
		mx, my, 
		ScreenWidth(), ScreenHeight(),
		CurrentViewOrigin(), 
		CurrentViewAngles(),
		rayOrigin,
		rayForward );

	m_fZoom = newZoom;

	BoundOrigin( m_vecTacticalOrigin );

	// move mouse to center and zero out any delta
	m_pCommanderView->MoveMouse( rayOrigin );
//	RequestFocus();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CCommanderOverlayPanel::IsRightMouseMapMoving( void )
{
	return m_right.m_bMouseDown;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCommanderOverlayPanel::RightMouseMapMove( void )
{
	/*
	// Figure out worldspace movement based on picking ray
	Vector rayForward;
	Vector vecEndPosCurrent, vecEndPosPrevious;

	// Need to convert from screen space back to a worldspace ray
	CreatePickingRay( 
		m_right.m_nXCurrent, m_right.m_nYCurrent, 
		ScreenWidth(), ScreenHeight(),
		g_vecRenderOrigin, 
		g_vecRenderAngles,
		vecEndPosCurrent,
		rayForward );

	engine->Con_NPrintf( 8, "x %i y %i hit %.3f %.3f",
		m_right.m_nXCurrent, m_right.m_nYCurrent, vecEndPosCurrent.x, vecEndPosCurrent.y );
	*/

	if ( !m_right.m_bMouseDown )
		return;

	/*
	// See if there is a delta in the current and previous mouse positions
	//
	int dx, dy;

	dx = m_right.m_nXCurrent - m_right.m_nXPrev;
	dy = m_right.m_nYCurrent - m_right.m_nYPrev;

	// No relative move
	if ( !dx && !dy )
		return;

	// Figure out worldspace movement based on picking ray
	Vector rayForward;
	Vector vecEndPosCurrent, vecEndPosPrevious;

	// Need to convert from screen space back to a worldspace ray
	CreatePickingRay( 
		m_right.m_nXCurrent, m_right.m_nYCurrent, 
		ScreenWidth(), ScreenHeight(),
		g_vecRenderOrigin, 
		g_vecRenderAngles,
		vecEndPosCurrent,
		rayForward );

	// Now create ray from old position
	// Need to convert from screen space back to a worldspace ray
	CreatePickingRay( 
		m_right.m_nXPrev, m_right.m_nYPrev, 
		ScreenWidth(), ScreenHeight(),
		g_vecRenderOrigin, 
		g_vecRenderAngles,
		vecEndPosPrevious,
		rayForward );

	// Remove z component
	vecEndPosPrevious.z = 0.0;
	vecEndPosCurrent.z = 0.0;

	// Compute offset
	Vector viewDelta;
	VectorSubtract( vecEndPosCurrent, vecEndPosPrevious, viewDelta );

	viewDelta.z = 0.0f;

	VectorAdd( m_vecTacticalOrigin, viewDelta, m_vecTacticalOrigin );

	m_pCommanderView->BoundOrigin( m_vecTacticalOrigin );
	*/
}


//-----------------------------------------------------------------------------
// Purpose: Right mouse button down
//-----------------------------------------------------------------------------
void CCommanderOverlayPanel::RightMousePressed( void )
{
	if ( m_left.m_bMouseDown || m_right.m_bMouseDown )
		return;

	SetCursor(m_CursorRightMouseMove);

	StartMovingMouse( m_right );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCommanderOverlayPanel::RightMouseReleased( void )
{
	if ( !m_right.m_bMouseDown )
		return;

	m_right.m_bMouseDown = false;
	int mx, my;
	GetMousePos( mx, my );
	UpdateMousePos( mx, my, m_right );

	// Final move
	RightMouseMapMove();

	vgui::input()->SetMouseCapture( NULL );

	SetCursor(m_CursorCommander);

	// If the player's dead, and doesn't have the special sniper upgrade
	// then check for respawn stations....
 	C_BaseTFPlayer *pPlayer = C_BaseTFPlayer::GetLocalPlayer();
	if (!pPlayer)
		return;

	// If the player's dead, abort
	if ( pPlayer->PlayerClass() == TFCLASS_SNIPER )
	{
		// Sniper's can request a respawn point when dead
		//
		Vector rayOrigin, rayForward;
		Vector vecEndPos;

		// Need to convert from screen space back to a worldspace ray
		CreatePickingRay( 
			m_right.m_nXCurrent, m_right.m_nYCurrent, 
			ScreenWidth(), ScreenHeight(),
			CurrentViewOrigin(), 
			CurrentViewAngles(),
			rayOrigin,
			rayForward );

		// Now do the trace
		trace_t tr;

		vecEndPos = rayOrigin + rayForward * MAX_TRACE_LENGTH;

		UTIL_TraceLine( rayOrigin, vecEndPos, MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &tr );

		// Didn't hit anything
		if ( tr.fraction == 1.0 )
			return;

		char cmd[ 128 ];
		Q_snprintf( cmd, sizeof( cmd ), "respawnpoint %i %i %i\n",
			(int)tr.endpos.x,
			(int)tr.endpos.y,
			(int)tr.endpos.z );

		engine->ServerCmd( cmd );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : x - 
//			y - 
//			mouse - 
//-----------------------------------------------------------------------------
void CCommanderOverlayPanel::UpdateMousePos( int x, int y, MouseDown_t& mouse )
{
	// store previous
	mouse.m_nXPrev = mouse.m_nXCurrent;
	mouse.m_nYPrev = mouse.m_nYCurrent;
	// update current
	mouse.m_nXCurrent = x;
	mouse.m_nYCurrent = y;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : x - 
//			y - 
//-----------------------------------------------------------------------------
void CCommanderOverlayPanel::OnCursorMoved(int x, int y)
{
	UpdateMousePos( x, y, m_left );
	UpdateMousePos( x, y, m_right );

	RightMouseMapMove();
//	RequestFocus();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : x - 
//			y - 
//-----------------------------------------------------------------------------
void CCommanderOverlayPanel::GetMousePos( int& x, int& y )
{
	vgui::input()->GetCursorPos( x, y );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mouse - 
//-----------------------------------------------------------------------------
void CCommanderOverlayPanel::StartMovingMouse( MouseDown_t& mouse )
{
	mouse.m_bMouseDown = true;
	GetMousePos( mouse.m_nXStart, mouse.m_nYStart );

	mouse.m_nXCurrent = mouse.m_nXPrev = mouse.m_nXStart;
	mouse.m_nYCurrent = mouse.m_nYPrev = mouse.m_nYStart;

	vgui::input()->SetMouseCapture( GetVPanel() );
}

//-----------------------------------------------------------------------------
// Purpose: Left mouse button down
//-----------------------------------------------------------------------------
void CCommanderOverlayPanel::LeftMousePressed( void )
{
	if ( m_left.m_bMouseDown || m_right.m_bMouseDown )
		return;

	StartMovingMouse( m_left );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : code - 
//-----------------------------------------------------------------------------
void CCommanderOverlayPanel::OnMousePressed(vgui::MouseCode code)
{
	switch ( code )
	{
	case vgui::MOUSE_LEFT:
		LeftMousePressed();
		break;
	case vgui::MOUSE_RIGHT:
		RightMousePressed();
		break;
	default:
		BaseClass::OnMousePressed( code );
		return;
	}

//	RequestFocus();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : x0 - 
//			y0 - 
//			x1 - 
//			y1 - 
//			true - 
//-----------------------------------------------------------------------------
void CCommanderOverlayPanel::SelectPlayersInRectangle( int x0, int y0, int x1, int y1, bool clearOldSelections /*= true*/ )
{
	int num_selected = 0;

	// Assume zero players in selection
	if ( clearOldSelections )
	{
		m_bHaveActiveSelection = false;
	}
	else
	{
		// Count # selected
		num_selected = CountSelectedPlayers();
	}

	CMapPlayers *pPlayer;
	C_BaseTFPlayer	*tf2player;

	// See which players on my team are within the rectangle in screen space
	//
	for ( int playerNum = 1; playerNum <= MAX_PLAYERS; playerNum++ )
	{
		pPlayer = &MapData().m_Players[ playerNum - 1 ];

		if ( clearOldSelections )
		{
			pPlayer->m_bSelected = false;
		}

		if ( playerNum > gpGlobals->maxClients )
		{
			continue;
		}

		tf2player = dynamic_cast<C_BaseTFPlayer *>( cl_entitylist->GetEnt( playerNum ) );
		if ( !tf2player )
		{
			continue;
		}

		// Check pvs on this guy
		// If the entity was in the commander's PVS then show it on the minimap, too
		//
		if ( ArePlayersOnSameTeam( engine->GetLocalPlayer(), playerNum ) == false )
		{
			continue;
		}

		// Check their actual draw position
		int drawX, drawY;
		Vector pos, screen;

		pos = tf2player->GetAbsOrigin();

		// Transform
		debugoverlay->ScreenPosition( pos, screen );

		// FIXME: Get the player icon size from where?!?					
		drawX = screen.x - 32;
		drawY = screen.y - 40;
		int intersectX = 64;
		int intersectY = 64;

		// Check for intersection ( with slop )
		if ( drawX + intersectX < x0 )
			continue;
		if ( drawX > x1 )
			continue;
		if ( drawY + intersectY < y0 )
			continue;
		if ( drawY > y1 )
			continue;

		// Already selected?
		if ( !pPlayer->m_bSelected )
		{
			// Add to selected list
			pPlayer->m_bSelected = true;
			num_selected++;
		}
	}

	if ( num_selected != 0 )
	{
		m_bHaveActiveSelection = true;
	}
}

//-----------------------------------------------------------------------------
// Returns the visible area (not including the tech tree)
//-----------------------------------------------------------------------------
void CCommanderOverlayPanel::GetVisibleSize( int& w, int& h )
{
	GetSize( w, h );

	// FIXME: Hack to make the map appear above the tech tree
	h -= 200;
}

//-----------------------------------------------------------------------------
// Returns the visible area (not including the tech tree)
//-----------------------------------------------------------------------------
void CCommanderOverlayPanel::GetVisibleArea( Vector& mins, Vector& maxs )
{
	float w, h;
	GetVisibleOrthoSize( w, h );

	ActualToVisibleOffset( mins );
	mins += m_vecTacticalOrigin;
	maxs = mins;
	mins.x -= w; maxs.x +=w;
	mins.y -= h; maxs.y +=h;
}

//-----------------------------------------------------------------------------
// Purpose: User let go of left mouse button
//-----------------------------------------------------------------------------
void CCommanderOverlayPanel::LeftMouseReleased( void )
{
	if ( !m_left.m_bMouseDown )
		return;

	m_left.m_bMouseDown = false;

	int mx, my;
	GetMousePos( mx, my );
	UpdateMousePos( mx, my, m_left );

	vgui::input()->SetMouseCapture( NULL );

	// Normalize the rectangle
	int x0, y0, x1, y1;
	x0 = MIN( m_left.m_nXStart, m_left.m_nXCurrent );
	x1 = MAX( m_left.m_nXStart, m_left.m_nXCurrent );
	y0 = MIN( m_left.m_nYStart, m_left.m_nYCurrent );
	y1 = MAX( m_left.m_nYStart, m_left.m_nYCurrent );

	bool clearOldStates = true;
	if ( vgui::input()->IsKeyDown( vgui::KEY_LCONTROL ) )
	{
		clearOldStates = false;
	}

	SelectPlayersInRectangle( x0, y0, x1, y1, clearOldStates );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCommanderOverlayPanel::GetOrthoRenderBox(Vector &vCenter, float &xSize, float &ySize)
{
	vCenter = m_vecTacticalOrigin;

	// min and max depends on the current zoom level
	int w, h;
	GetParent()->GetSize( w, h );

	xSize = m_fZoom;
	ySize = xSize * ( (float)h / (float)w );

	xSize *= 0.5f;
	ySize *= 0.5f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCommanderOverlayPanel::GetVisibleOrthoSize(float &xSize, float &ySize)
{
	// min and max depends on the current zoom level
	int w, h;
	GetVisibleSize( w, h );
	xSize = m_fZoom;
	ySize = xSize * ( (float)h / (float)w );

	xSize *= 0.5f;
	ySize *= 0.5f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CCommanderOverlayPanel::WorldUnitsPerPixel()
{
	int w, h;
	GetParent()->GetSize( w, h );
	return m_fZoom / w;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCommanderOverlayPanel::ActualToVisibleOffset( Vector& offset )
{
	// FIXME: This doesn't work arbitrarily; we assume the visible
	// part is on the top of the screen..
	int w, h, wact, hact;
	GetVisibleSize( w, h );
	GetParent()->GetSize( wact, hact );
	
	float worldUnitsPerPixel = m_fZoom / wact;
	float dWorldY = (hact - h) * 0.5f * worldUnitsPerPixel;
	offset.Init( 0, dWorldY, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Make sure origin is inside map x, y bounds ( not z )
//-----------------------------------------------------------------------------
void CCommanderOverlayPanel::BoundOrigin( Vector& camera )
{
	// We're going to do this entire computation assuming the camera
	// is in the center of the viewable area, which it isn't. At the
	// end, we'll need to apply a fixup to deal with that

	Vector mins, maxs, halfsize;
	MapData().GetMapBounds( mins, maxs );
	VectorSubtract( maxs, mins, halfsize );
	halfsize *= 0.5f;

	// avoid black edges...
	float dim[2];
	GetVisibleOrthoSize( dim[0], dim[1] );

	// Compute the position of the center of the visible area based on
	// the new suggested camera position 
	Vector actualToVisible, newVisCenter;
	ActualToVisibleOffset( actualToVisible );
	VectorAdd( camera, actualToVisible, newVisCenter );

	// Only bound x & y
	for ( int i = 0; i < 2; i++ )
	{
		if (dim[i] > halfsize[i])
		{
			newVisCenter[i] = mins[i] + halfsize[i];
		}
		else
		{
			newVisCenter[ i ] = MAX( newVisCenter[i], mins[i] + dim[i] );
			newVisCenter[ i ] = MIN( newVisCenter[i], maxs[i] - dim[i] );
		}
	}

	// Remember: The viewport takes up the entire screen; but the visible
	// area only takes up the top half of the screen. Therefore, we need to
	// set the origin so that the center of what we want lies at the
	// center of the visible region
	VectorSubtract( newVisCenter, actualToVisible, camera ); 
}

//-----------------------------------------------------------------------------
// Purpose: 
// Given mouse/screen positions as well as the current
//  render origin and angles, returns a unit vector through the mouse position
//  that can be used to trace into the world under the mouse click pixel.
// Input  : fov - 
//			mousex - 
//			mousey - 
//			screenwidth - 
//			screenheight - 
//			vecRenderAngles - 
//			c_x - 
//			vpn - 
//			vup - 
//			360.0 - 
//-----------------------------------------------------------------------------
void CCommanderOverlayPanel::CreatePickingRay( int mousex, int mousey, 
	int screenwidth, int screenheight,
	const Vector& vecRenderOrigin, 
	const QAngle& vecRenderAngles, 
	Vector &rayOrigin,
	Vector &rayDirection
	)
{
	Vector vCenter;
	float xSize, ySize;
	GetOrthoRenderBox(vCenter, xSize, ySize);
	
	float xPos = RemapVal(mousex, 0, screenwidth,  vCenter.x-xSize, vCenter.x+xSize);
	float yPos = RemapVal(mousey, 0, screenheight, vCenter.y+ySize, vCenter.y-ySize); // (flip screen y)
	rayOrigin.Init(xPos, yPos, vCenter.z);
	rayDirection.Init(0, 0, -1);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCommanderOverlayPanel::OnMouseReleased(vgui::MouseCode code)
{
	switch ( code )
	{
	case vgui::MOUSE_LEFT:
		LeftMouseReleased();
		break;
	case vgui::MOUSE_RIGHT:
		RightMouseReleased();
		break;
	default:
		BaseClass::OnMouseReleased( code );
		return;
	}

//	RequestFocus();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCommanderOverlayPanel::CenterOnMouse( Vector& mouseWorldPos )
{
	// Figure out worldspace movement based on picking ray
	Vector rayForward;
	Vector centerOrigin;

	int mx, my;
	GetMousePos( mx, my );

	// Need to convert from screen space back to a worldspace ray
	CreatePickingRay( 
		mx, my, 
		ScreenWidth(), ScreenHeight(),
		CurrentViewOrigin(), 
		CurrentViewAngles(),
		mouseWorldPos,
		rayForward );

	CreatePickingRay( 
		ScreenWidth()/2, ScreenHeight()/2, 
		ScreenWidth(), ScreenHeight(),
		CurrentViewOrigin(), 
		CurrentViewAngles(),
		centerOrigin,
		rayForward );

	Vector offset;
	VectorSubtract( mouseWorldPos, centerOrigin, offset );
	offset.z = 0.0f;
	VectorAdd( m_vecTacticalOrigin, offset, m_vecTacticalOrigin );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : delta - 
//-----------------------------------------------------------------------------
void CCommanderOverlayPanel::OnMouseWheeled(int delta)
{
	// Figure out worldspace movement based on picking ray
	Vector rayOrigin;

	CenterOnMouse( rayOrigin );

	// Gotta do the zoom after picking the ray, or it'll use the wrong zoom factor
	// for computing the picking ray
	if ( delta < 0 )
	{
		m_fZoom *= 1.25f;
	}
	else if ( delta > 0 )
	{
		m_fZoom /= 1.25f;
	}

	m_fZoom = MIN( m_fZoom, m_MaxWorldWidth );
	m_fZoom = MAX( m_fZoom, m_MinWorldWidth );

	BoundOrigin( m_vecTacticalOrigin );

	// move mouse to center and zero out any delta
	m_pCommanderView->MoveMouse( rayOrigin );

//	RequestFocus();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCommanderOverlayPanel::Paint()
{
	if ( !m_left.m_bMouseDown )
		return;

	// Update mouse position, so we don't get the 1 frame lag
	int mx, my;
	GetMousePos( mx, my );
	UpdateMousePos( mx, my, m_left );

	int x0, x1, y0, y1;
	// Make sure it's normalized
	x0 = MIN( m_left.m_nXStart, m_left.m_nXCurrent );
	x1 = MAX( m_left.m_nXStart, m_left.m_nXCurrent );
	y0 = MIN( m_left.m_nYStart, m_left.m_nYCurrent );
	y1 = MAX( m_left.m_nYStart, m_left.m_nYCurrent );

	// Draw selection rectangle
	vgui::surface()->DrawSetColor( 200, 220, 250, 192 );
	vgui::surface()->DrawOutlinedRect( x0, y0, x1, y1 );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CCommanderOverlayPanel::GetZoom( void )
{
	return (m_fZoom - m_MinWorldWidth) / (m_MaxWorldWidth - m_MinWorldWidth);
}
