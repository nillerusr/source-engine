//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef COMMANDEROVERLAYPANEL_H
#define COMMANDEROVERLAYPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "CommanderOverlay.h"
#include <vgui_controls/Panel.h>

#define TACTICAL_ZOFFSET	100	
#define TACTICAL_MIN_VIEWABLE_SIZE 1000
#define TACTICAL_SPACEBAR_VIEWABLE_SIZE 4000

#define FOG_ALPHAMAP_SIZE	64

class C_TFTeam;
class IVTFTexture;
class ITexture;

class CCommanderOverlayPanel : public vgui::Panel
{
	typedef vgui::Panel BaseClass;
// Panel overrides.
public:
	CCommanderOverlayPanel( void );
	virtual ~CCommanderOverlayPanel( void );

	virtual void	Paint();

	virtual void	OnMousePressed(vgui::MouseCode code);
	virtual void	OnMouseReleased(vgui::MouseCode code);
	virtual void	OnCursorMoved(int x, int y);
	virtual void	OnMouseWheeled(int delta);
	virtual void	OnKeyPressed(vgui::KeyCode code);
	
	// Call on enable
	void	Enable();
	void	Disable();

	// called when we're ticked (updates our state)...
	void	OnTick();

	// Call when the map changes
	void	LevelInit( char const* pMapName );
	void	LevelShutdown( void );

	// Returns the visible area (not including the tech tree)
	void	GetVisibleSize( int& w, int& h );
	
	// Returns the visible area in world units
	void	GetVisibleArea( Vector& mins, Vector& maxs );

	// Returns the number of world units per pixel
	float	WorldUnitsPerPixel();

	// Fill in the rectangle that the view is rendered from.
	// It is looking down negative Z, and goes from [vCenter.x-xSize, vCenter.y-ySize] to [vCenter.x+xSize, vCenter.y+ySize].
	void	GetOrthoRenderBox(Vector &vCenter, float &xSize, float &ySize);
	void	GetVisibleOrthoSize(float &xSize, float &ySize);
	void	ActualToVisibleOffset( Vector& offset );

	void BoundOrigin( Vector& camera );

	void			CreatePickingRay( int mousex, int mousey, 
		int screenwidth, int screenheight,
		const Vector& vecRenderOrigin, 
		const QAngle& vecRenderAngles, 
		Vector &rayOrigin,
		Vector &rayDirection
		);

	Vector&	TacticalOrigin() { return m_vecTacticalOrigin; }
	QAngle&	TacticalAngles() { return m_vecTacticalAngles; }

	float	GetZoom( void );

	void	SetCommanderView( CClientModeCommander *commander );
	bool	IsRightMouseMapMoving( void );

private:
	struct MouseDown_t
	{
		bool	m_bMouseDown;
		int		m_nXStart;
		int		m_nYStart;
		int		m_nXCurrent;
		int		m_nYCurrent;
		int		m_nXPrev;
		int		m_nYPrev;
	};

	// Compute render origin
	void	InitializeRenderOrigin();

	// Methods relating to zoom
	void	ComputeZoomRange();

	void	LeftMousePressed( void );
	void	LeftMouseReleased( void );
	void	RightMousePressed( void );
	void	RightMouseReleased( void );
	void	RightMouseMapMove( void );

	// Shows the entire view
	void	ChangeZoomLevel( float newZoom );

	// Centers the view on the mouse
	void	CenterOnMouse( Vector& mouseWorldPos );

	int		CountSelectedPlayers( void );
	void	GetSelectedPlayerBitField( CBitVec< MAX_PLAYERS >& bits );
	void	SelectPlayersInRectangle( int x0, int y0, int x1, int y1, bool clearOldSelections = true );

	void	GetMousePos( int& x, int& y );
	void	StartMovingMouse( MouseDown_t& mouse );
	void	UpdateMousePos( int x, int y, MouseDown_t& mouse );

	// camera origin
	Vector			m_vecTacticalOrigin;
	QAngle			m_vecTacticalAngles;

	MouseDown_t	m_left, m_right;

	float	m_fZoom;		// 0 = sitting at world maxs.z. 1 = at maxs.z + (maxs.z-mins.z)*2.
	float	m_MinWorldWidth;
	float	m_MaxWorldWidth;
	bool	m_bHaveActiveSelection;
	float	m_flPreviousMaxWorldWidth;

	CClientModeCommander	*m_pCommanderView;

	vgui::HCursor	m_CursorCommander;
	vgui::HCursor	m_CursorRightMouseMove;
};

#endif // COMMANDEROVERLAYPANEL_H
