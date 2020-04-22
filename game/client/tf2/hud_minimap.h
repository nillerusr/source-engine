//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $NoKeywords: $
//=============================================================================//
#if !defined( HUD_MINIMAP_H )
#define HUD_MINIMAP_H

#ifdef _WIN32
#pragma once
#endif

#include "mathlib/vector.h"
#include "cdll_client_int.h"
#include "hudelement.h"
#include "utllinkedlist.h"
#include "CommanderOverlay.h"
#include "MapData.h"

class BitmapImage;
class CTextHelpPanel;

//-----------------------------------------------------------------------------
// This interfaces gets called when a click occurs in the minimap
//-----------------------------------------------------------------------------
class IMinimapClient
{
public:
	virtual void MinimapClicked( const Vector& clickWorldPos ) = 0;
};


//-----------------------------------------------------------------------------
// Ways to perform WorldToMinimap
//-----------------------------------------------------------------------------
enum MinimapPosType_t
{
	MINIMAP_NOCLIP = 0,	// Don't draw things off the minimap
	MINIMAP_CLAMP,		// Clamp the position to lie within the minimap
	MINIMAP_CLIP,		// Clip the vector from minimap center to object
						// to the minimap bounds and put the object on the edge
	MINIMAP_ALWAYS_ACCEPT // Just do the scaling and return a value - don't worry about clipping.
};


//-----------------------------------------------------------------------------
// Purpose: On the left side of the screen is an overview map and a highlight 
// rectangle showing the current viewport
//
// NOTE: The minimap panel is architected in such a way as to view some global
// state. There can be many instances of the minimap panel
//-----------------------------------------------------------------------------
class CMinimapPanel : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CMinimapPanel, vgui::Panel );

public:
	enum
	{
		STABLE = 0,
		GROWING,
		SHRINKING
	};
	
							CMinimapPanel( const char *pElementName );
	virtual					~CMinimapPanel( void );

	void					Init( IMinimapClient* pClient = NULL );

	virtual void			LevelInit( const char *mapname );
	virtual void			LevelShutdown( void );

	virtual void			ApplySchemeSettings( vgui::IScheme *scheme );

	virtual void			OnMousePressed(vgui::MouseCode code);

	virtual void			Paint();

	virtual void			OnTick();
	virtual void			OnThink();

	virtual void			OnSizeChanged( int w, int h );

	// Converts a world-space position to a coordinate in minimap panel space
	bool					WorldToMinimap( MinimapPosType_t posType, const Vector &pos, float& outx, float& outy );

	// The following methods deal with management of the minimap data
	static vgui::Panel		*MinimapRootPanel();
	static CMinimapPanel	*MinimapPanel();

	// Call this when the minimap panel is going to be drawn...
	void					Activate();

	// 0 is minimized and won't draw zoom details, 1.0 is fully maximized
	bool					ShouldDrawZoomDetails( int& alpha );

	bool					InternalWorldToMinimap( MinimapPosType_t posType, const Vector &pos, const Vector& origin, float zoomscale, float& outx, float& outy );

	static void				Zoom_Minimap_f( void );
	void					ZoomIn( void );

	void					ToggleMinimap( void );
	void					SetMinimapZoom( bool bZoom);

	virtual void			ProcessInput( void );

	float					GetAdjustedZoom( void );
	float					GetTrueZoom( void );

	// Handler for our message
	void					MsgFunc_MinimapPulse( bf_read &msg );

private:
	void					SetBackgroundMaterials( char const* pMaterialName );

	void					GetMapOriginAndScale( Vector& origin, float& scale );

	void					SetBackgroundViewport( float minx, float miny, float maxx, float maxy, bool includedetails );
	void					PaintActOverlays( int teamIndex, int alpha );
	void					AdjustNormalizedPositionForAspectRatio( float& x, float& y );

	void					DrawVisibleArea( void );

	void					ComputeMapOrigin( Vector& origin );

	void					InitOverlays( const char *materialrootname );
	void					ShutdownOverlays( void );

	vgui::Panel				*GetTextPaintPanel( void );

	void					InvokeOnTickOnChildren( vgui::Panel *parent );

	bool	m_DrawVisibleArea;
	IMinimapClient	*m_pClient;

	CPanelAnimationVar( float, m_flExpansionFrac, "ExpansionFrac", "0" );
	CPanelAnimationVar( float, m_flDetailsAlpha, "DetailsAlpha", "0" );
	CPanelAnimationVar( float, m_flMapScale, "MapScale", "2000" );
	CPanelAnimationVar( float, m_flZoomAmount, "ZoomAmount", "1.0" );
	CPanelAnimationVar( float, m_flCenterOnPlayer, "CenterOnPlayer", "1" );

	CPanelAnimationVar( float, m_flInsetPixels, "InsetPixels", "16" );

	float					m_flPrevZoomAmount;

	CPanelAnimationVar( Color, m_BackgroundColor, "BackgroundColor", "Black" );
	CPanelAnimationVar( Color, m_BorderColor, "BorderColor", "FgColor" );

	int						m_nZoomLevel;
	bool					m_bMinimapZoomed;
	int						m_nCurrentAct;

	float					m_flZoomAdjust;  // Adjustment needed to get map to fit exactly into box in one dimension assuming
												// m_flZoomAmount == 1.0f


	enum
	{
		NUM_WIDTHS = 2,
	};


	// Calculated once per frame and cached
	Vector					m_vecCurrentOrigin;
	Vector					m_vecMapCenter;
	float					m_flMapAspectRatio;
	float					m_flViewportAspectRatio;
	// map aspect / viewport aspect
	float					m_flAspectAdjustment;

	float					m_flNormalizedXScale;
	float					m_flNormalizedYScale;
	float					m_flNormalizedXOffset;
	float					m_flNormalizedYOffset;


	//	These are the wordspace corners of the minimap
	float					m_flWorldSpaceBounds[ 4 ];
	 // These are the above, except clamped to actual world mins/maxs
	float					m_flClippedWorldSpaceBounds[ 4 ];
	// These are used to clamp the map origin to keep the zoomed map from scrolling off screen
	float					m_flWorldSpaceInsets[ 4 ];

	enum
	{
		MAX_ACTS = 16,
		MAX_ACT_TEAMS = 2
	};

	struct Overlays
	{
		bool				m_bInUse;
		BitmapImage			*m_pOverlay;
		BitmapImage			*m_pText;
	};

	BitmapImage				*m_pBackground[ MAX_ACT_TEAMS ];
	Overlays				m_rgOverlays[ MAX_ACT_TEAMS ][ MAX_ACTS ];

	CTextHelpPanel			*m_pTextPanel;
	vgui::Panel				*m_pBackgroundPanel;
};


//-----------------------------------------------------------------------------
// minimap render order
//-----------------------------------------------------------------------------
enum
{
	MINIMAP_GROUND_LINES = 0,
	MINIMAP_RESOURCE_ZONES,
	MINIMAP_OBJECTS, 
	MINIMAP_SPY_CAMERAS,
	MINIMAP_COLLECTORS,
	MINIMAP_MAP_GOALS,
	MINIMAP_PLAYERS,
	MINIMAP_LOCAL_PLAYER,
	MINIMAP_SNIPER_RESPAWN,
	MINIMAP_PERSONAL_ORDERS
};


//-----------------------------------------------------------------------------
// Instantiate a temporary trace (position based, or entity based)
//-----------------------------------------------------------------------------
void MinimapCreateTempTrace( const char* pMetaClassName, int sortOrder, const Vector &vecPosition );
void MinimapCreateTempTrace( const char* pMetaClassName, int sortOrder, C_BaseEntity *pEntity, const Vector &vecOffset );


//-----------------------------------------------------------------------------
// Helper macro to make overlay factories one line of code. Use like this:
//	DECLARE_MINIMAP_FACTORY( CEntityImagePanel, "image" );
//-----------------------------------------------------------------------------
struct MinimapInitData_t
{
	C_BaseEntity *m_pEntity;
	Vector m_vecPosition;	// relative to m_pEntity if it's specified, otherwise absolute position

	MinimapInitData_t() : m_pEntity(NULL), m_vecPosition( 0, 0, 0 ) {}
	MinimapInitData_t( C_BaseEntity *pEntity ) : m_pEntity(pEntity), m_vecPosition( 0, 0, 0 ) {}
};

#define DECLARE_MINIMAP_FACTORY( _PanelClass, _nameString )	\
	DECLARE_PANEL_FACTORY( _PanelClass, MinimapInitData_t, _nameString )


//-----------------------------------------------------------------------------
// Macros for help with simple registration of minimap
// Put DECLARE_MINIMAP_PANEL() in your class definition
// and CONSTRUCT_MINIMAP_PANEL( "panelname", SORT_ORDER ) in your class constructor
//-----------------------------------------------------------------------------
#define DECLARE_MINIMAP_PANEL( )	DECLARE_METACLASS_PANEL( m_MinimapTrace )
#define CONSTRUCT_MINIMAP_PANEL( _name, _sortorder )	\
	MinimapInitData_t _traceInit( this );				\
	CONSTRUCT_METACLASS_PANEL( m_MinimapTrace, _name, CMinimapPanel::MinimapRootPanel(), _sortorder, &_traceInit )
#define DESTRUCT_MINIMAP_PANEL( )		\
	DESTRUCT_METACLASS_PANEL( m_MinimapTrace )
#define IS_MINIMAP_PANEL_DEFINED( )	( m_MinimapTrace.GetPanel() != NULL )

// Kind of a hack; assumes all minimap panels inherit from CMinimapTracePanel, but that's reasonable..
#define SET_MINIMAP_PANEL_VISIBILITY( _visible )		\
	do													\
	{													\
		if (m_MinimapTrace.GetPanel())					\
		{												\
			static_cast<CMinimapTracePanel*>(m_MinimapTrace.GetPanel())->SetTraceVisibility( _visible );	\
		}												\
	} while (0)

#endif // HUD_MINIMAP_H