//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hud_minimap.h"
#include <vgui_controls/Controls.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <vgui/IInput.h>
#include <vgui_controls/AnimationController.h>
#include "vgui_bitmapimage.h"
#include "clientmode_tfbase.h"
#include "clientmode_tfnormal.h"
#include "hud.h"
#include "hud_commander_statuspanel.h"
#include "view.h"
#include "filesystem.h"
#include "imessagechars.h"
#include "hud_macros.h"
#include "c_tfteam.h"
#include "c_info_act.h"
#include "engine/IEngineSound.h"
#include "iinput.h"
#include "in_buttons.h"
#include "c_basetfplayer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar minimap_visible( "minimap_visible", "1", 0, "Draw minimap?" );
ConVar minimap_zoomtime( "minimap_zoomtime", "0.4", 0, "How long it takes to resize the minimap." );


static ConVar current_team( "current_team", "-1", 0 );

// Start out new maps at this zoom level
#define DEFAULT_ZOOM_LEVEL	0

using namespace vgui;

//-----------------------------------------------------------------------------
// Instantiate a temporary trace (position based, or entity based)
//-----------------------------------------------------------------------------
void MinimapCreateTempTrace( const char* pMetaClassName, int sortOrder, const Vector &vecPosition )
{
	MinimapInitData_t initData;
	initData.m_vecPosition = vecPosition;

	PanelMetaClassMgr()->CreatePanelMetaClass( 
		pMetaClassName, sortOrder, &initData, CMinimapPanel::MinimapRootPanel() );
}

void MinimapCreateTempTrace( const char* pMetaClassName, int sortOrder, C_BaseEntity *pEntity, const Vector &vecOffset )
{
	MinimapInitData_t initData;
	initData.m_pEntity = pEntity;
	initData.m_vecPosition = vecOffset;

	PanelMetaClassMgr()->CreatePanelMetaClass( 
		pMetaClassName, sortOrder, &initData, CMinimapPanel::MinimapRootPanel() );
}

//-----------------------------------------------------------------------------
// dummy root panel for all minimap traces
//-----------------------------------------------------------------------------
class CMinimapRootPanel : public Panel
{
	typedef Panel BaseClass;

public:
	CMinimapRootPanel( Panel *pParent = NULL ) 
		: BaseClass( pParent,"CMinimapRootPanel" ) 
	{
		SetPaintBackgroundEnabled( false );
		SetPaintEnabled( false );
		SetAutoDelete( false );
	}
};

class CTextHelpPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CTextHelpPanel, vgui::Panel )

public:
	
	CTextHelpPanel();

	virtual void Paint();
	virtual void PaintBackground();

	void	SetImage( BitmapImage *image );

	virtual void ApplySettings( KeyValues *inResourceData )
	{
		BaseClass::ApplySettings( inResourceData );
	}

private:

	BitmapImage		*m_pImage;

	CPanelAnimationVar( Color, m_OverlayColor, "OverlayColor", "White" );
	CPanelAnimationVar( Color, m_BorderColor, "BorderColor", "BrightFg" );
	CPanelAnimationVar( Color, m_BackgroundColor, "BackgroundColor", "Black" );
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTextHelpPanel::CTextHelpPanel()
: BaseClass( NULL, "HudMinimapTextHelpPanel" )
{
	Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetVisible( false );
	SetZPos( 1 );

	m_pImage = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTextHelpPanel::PaintBackground()
{
	// Get alpha from image
	if ( m_pImage )
	{
		Color bg = m_BackgroundColor;
		int r, g, b, a;
		m_pImage->GetColor( r, g, b, a );
		bg[3] = a;
		SetBgColor( bg );
	
		BaseClass::PaintBackground();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTextHelpPanel::Paint()
{
	BaseClass::Paint();

	if ( !m_pImage )
		return;

	m_pImage->SetColor( m_OverlayColor );
	m_pImage->DoPaint( GetVPanel() );

	int w, h;
	GetSize( w, h );

	surface()->DrawSetColor( m_BorderColor );
	surface()->DrawOutlinedRect( 0, 0, w, h );
	surface()->DrawOutlinedRect( 1, 1, w-1, h-1 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *image - 
//			x - 
//			y - 
//			w - 
//			h - 
//-----------------------------------------------------------------------------
void CTextHelpPanel::SetImage( BitmapImage *image )
{
	m_pImage = image;
}

//-----------------------------------------------------------------------------
// globals
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// All traces are children of this panel
//-----------------------------------------------------------------------------
Panel *CMinimapPanel::MinimapRootPanel()
{
	static CMinimapRootPanel s_MinimapRootPanel;
	return &s_MinimapRootPanel; 
}

CMinimapPanel *CMinimapPanel::MinimapPanel()
{
	ClientModeTFBase *pBasemode = ( ClientModeTFBase * )g_pClientMode;
	if ( !pBasemode )
		return NULL;

	return pBasemode->GetMinimap();
}

DECLARE_HUDELEMENT( CMinimapPanel );
DECLARE_HUD_MESSAGE( CMinimapPanel, MinimapPulse );

//-----------------------------------------------------------------------------
// Purpose: Placeholder for small overview map with viewport rectangle/selector
//-----------------------------------------------------------------------------
CMinimapPanel::CMinimapPanel( const char *pElementName )
: CHudElement( pElementName ), BaseClass( NULL, "HudMinimap" )
{
	Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetAutoDelete( false );
	for ( int i = 0; i < MAX_ACT_TEAMS; i++ )
	{
		m_pBackground[ i ] = 0;
	}
	memset( m_rgOverlays, 0, sizeof( m_rgOverlays ) );

	m_flExpansionFrac = 0.0f;

	// Minimap zoom
	m_bMinimapZoomed = false;
	m_nZoomLevel = DEFAULT_ZOOM_LEVEL;

	m_vecCurrentOrigin.Init();
	m_vecMapCenter.Init();
	m_flMapAspectRatio = 1.0f;
	m_flViewportAspectRatio = 1.0f;
	m_flAspectAdjustment = 1.0f;
	m_flNormalizedXScale = 1.0f;
	m_flNormalizedYScale = 1.0f;
	m_flNormalizedXOffset = 0.0f;
	m_flNormalizedYOffset = 0.0f;

	m_nCurrentAct = ACT_NONE_SPECIFIED;

	m_pTextPanel = new CTextHelpPanel();
	m_pBackgroundPanel = new Panel( NULL, "BackgroundPanel" );

	// We're gonna manage the lifetime of the text panel
	// since we change it's hierarchical connections from time to time
	m_pTextPanel->SetAutoDelete( false );
	m_pBackgroundPanel->SetAutoDelete( false );
	SetAutoDelete( false );
	
	m_pClient = NULL;

	m_flZoomAdjust = 1.0f;
	m_flPrevZoomAmount = 0.01f;

	SetZPos( 10 );

	ivgui()->AddTickSignal( GetVPanel() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMinimapPanel::~CMinimapPanel( void )
{
	delete m_pTextPanel;
	delete m_pBackgroundPanel;

	for ( int i = 0; i < MAX_ACT_TEAMS; i++ )
	{
		delete m_pBackground[ i ];
	}

	ShutdownOverlays();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *scheme - 
//-----------------------------------------------------------------------------
void CMinimapPanel::ApplySchemeSettings( IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );
	SetPaintBackgroundEnabled( false );
}


//-----------------------------------------------------------------------------
// initialization
//-----------------------------------------------------------------------------
void CMinimapPanel::Init( IMinimapClient* pClient )
{
	m_pClient = pClient;
}

//-----------------------------------------------------------------------------
// Call this when the minimap panel is going to be drawn...
//-----------------------------------------------------------------------------
void CMinimapPanel::Activate()
{
	// The panel is a view into the minimap root panel
	MinimapRootPanel()->SetParent( this );

	Panel *pParent = g_pClientMode->GetViewport();

	if ( pParent && m_pBackgroundPanel )
	{
		m_pBackgroundPanel->SetParent( pParent );
		m_pBackgroundPanel->SetBounds( XRES( 0 ), YRES( 0 ), XRES( 640 ), YRES( 480 ) );
		m_pBackgroundPanel->SetVisible( false );
		m_pBackgroundPanel->SetZPos( 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : w - 
//			h - 
//-----------------------------------------------------------------------------
void CMinimapPanel::OnSizeChanged( int w, int h )
{
	BaseClass::OnSizeChanged( w, h );

	MinimapRootPanel()->SetSize( w, h );

	// Make sure icons are snapped to current window size
	InvokeOnTickOnChildren( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CMinimapPanel::GetAdjustedZoom( void )
{
	return m_flZoomAmount * m_flZoomAdjust;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CMinimapPanel::GetTrueZoom()
{
	return m_flZoomAmount;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : center - 
//			scale - 
//-----------------------------------------------------------------------------
void CMinimapPanel::GetMapOriginAndScale( Vector& origin, float& scale )
{
	origin	= m_vecCurrentOrigin;
	scale	= GetAdjustedZoom();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : clip - 
//			pos - 
//			outx - 
//			outy - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMinimapPanel::WorldToMinimap( MinimapPosType_t posType, const Vector& pos, float& outx, float& outy )
{
	Vector origin;
	float zoomscale;

	GetMapOriginAndScale( origin, zoomscale );

	return InternalWorldToMinimap( posType, pos, origin, zoomscale, outx, outy );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : x - 
//			y - 
//-----------------------------------------------------------------------------
void CMinimapPanel::AdjustNormalizedPositionForAspectRatio( float& x, float& y )
{
	x = m_flNormalizedXOffset + x * m_flNormalizedXScale;
	y = m_flNormalizedYOffset + y * m_flNormalizedYScale;
}

//-----------------------------------------------------------------------------
// Converts a world-space position to a coordinate in minimap panel space
//-----------------------------------------------------------------------------
bool CMinimapPanel::InternalWorldToMinimap( MinimapPosType_t posType, const Vector &pos, const Vector& origin, float zoomscale, float& outx, float& outy )
{
	int wide, tall;
	MinimapRootPanel()->GetSize( wide, tall );

	Vector worldmins, worldmaxs;
	MapData().GetMapBounds( worldmins, worldmaxs );
	Vector worldsize = worldmaxs - worldmins;

	Vector test = ( pos -  origin );

	float xfraction = 0.0f;
	
	if ( worldsize.x > 0 )
	{
		xfraction = (test.x - worldmins.x) / (worldmaxs.x - worldmins.x);
	}

	float yfraction = 0.0f;
	
	if ( worldsize.y > 0 )
	{
		yfraction = (test.y - worldmins.y) / (worldmaxs.y - worldmins.y);
	}

	xfraction = ( xfraction - 0.5f ) * zoomscale + 0.5f; 
	yfraction = ( yfraction - 0.5f ) * zoomscale + 0.5f;

	yfraction = 1.0f - yfraction;

	// Adjust in case not all of map can be shown
	AdjustNormalizedPositionForAspectRatio( xfraction, yfraction );

	// Normalize?
	bool inside = true;
	switch ( posType )
	{
	case MINIMAP_CLIP:
		{
			// Clip the vector from minimap center to object
			// to the minimap bounds and put the object on the edge
			Vector2D delta( xfraction - 0.5f, yfraction - 0.5f );
			Vector2D fdelta( fabs(delta.x), fabs(delta.y) );
			if (fdelta.x > fdelta.y)
			{
				// It's more horizontal than vertical..
				if (fdelta.x >= 0.5f)
				{
					float flRatio = delta.y / delta.x;
					xfraction = clamp(xfraction, 0, 1);
					yfraction = (xfraction - 0.5f) * flRatio + 0.5f;
				}
			}
			else
			{
				if (fdelta.y >= 0.5f)
				{
					// It's more vertical than horizontal
					float flRatio = delta.x / delta.y;
					yfraction = clamp(yfraction, 0, 1);
					xfraction = (yfraction - 0.5f) * flRatio + 0.5f;
				}
			}
		}
		break;

	case MINIMAP_CLAMP:
		{
			// Clamp the position to lie within the minimap
			xfraction = clamp(xfraction, 0, 1);
			yfraction = clamp(yfraction, 0, 1);
		}
		break;

	case MINIMAP_NOCLIP:
		{
			// See if it's off screen
			if ( xfraction < 0.0 || xfraction > 1.0 ||
				 yfraction < 0.0 || yfraction > 1.0 )
			{
				inside = false;
			}
		}
		break;
	}

	outx = xfraction * wide;
	outy = yfraction * tall;
	
	return inside;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *mapname - 
//-----------------------------------------------------------------------------
void CMinimapPanel::LevelInit( const char *mapname )
{
	SetBackgroundMaterials( MapData().m_Minimap.m_szBackgroundMaterial );

	m_nZoomLevel = DEFAULT_ZOOM_LEVEL;

	HOOK_HUD_MESSAGE( CMinimapPanel, MinimapPulse );
}

//-----------------------------------------------------------------------------
// Purpose: Play a pulse on the minimap
//-----------------------------------------------------------------------------
void CMinimapPanel::MsgFunc_MinimapPulse( bf_read &msg )
{
	Vector vecPosition;
	msg.ReadBitVec3Coord( vecPosition );
	C_TFTeam *pTeam = (C_TFTeam *)GetLocalTeam();
	if ( pTeam )
	{
		pTeam->NotifyBaseUnderAttack( vecPosition, false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMinimapPanel::LevelShutdown( void )
{
}

//-----------------------------------------------------------------------------
// Sets the background material
//-----------------------------------------------------------------------------
void CMinimapPanel::SetBackgroundMaterials( const char *pMaterialName )
{
	int i;
	for ( i = 0; i < MAX_ACT_TEAMS; i++ )
	{
		delete m_pBackground[ i ];
		m_pBackground[ i ] = NULL;
	}

	if ( pMaterialName[ 0 ] )
	{
		for ( i = 0; i < MAX_ACT_TEAMS; i++ )
		{
			char teammaterial[ 512 ];

			Q_snprintf( teammaterial, sizeof( teammaterial ), "%s_team%i",
				pMaterialName, i + 1 );

			// If a _team# version exists, use that, otherwise, use the default
			if ( g_pFullFileSystem->FileExists( VarArgs( "materials/%s.vmt", teammaterial ) ) )
			{
				 m_pBackground[ i ] = new BitmapImage( GetVPanel(), teammaterial );
			}
			else
			{
				 m_pBackground[ i ] = new BitmapImage( GetVPanel(), pMaterialName );
			}
		}
	}

	if ( m_pTextPanel )
	{
		m_pTextPanel->SetImage( NULL );
	}

	ShutdownOverlays();
	InitOverlays( pMaterialName );
}

//-----------------------------------------------------------------------------
// Called when the mouse is hit 
//-----------------------------------------------------------------------------
void CMinimapPanel::OnMousePressed(MouseCode code)
{
	if ((code == MOUSE_LEFT) && m_pClient)
	{
		// Convert mouse position to world position
		int x, y;
		vgui::input()->GetCursorPos( x, y );

		int w, h;
		GetSize( w, h );

		Vector worldPos;
		worldPos.x = (float) x / (float) w;
		worldPos.y = 1.0f - (float) y / (float) h;
		worldPos.z = 0;	// z isn't used

		Vector worldMins, worldMaxs;
		MapData().GetMapBounds( worldMins, worldMaxs );
		worldPos *= (worldMaxs - worldMins);
		worldPos += worldMins;

		m_pClient->MinimapClicked( worldPos );
	}
}

void CMinimapPanel::SetBackgroundViewport( float minx, float miny, float maxx, float maxy, bool includedetails )
{
	int i;
	int x, y, w, h;

	x = 0;
	y = 0;
	w = GetWide();
	h = GetTall();

	if ( minx < 0.0f || maxx > 1.0f ||
		 miny < 0.0f || maxy > 1.0f )
	{
		float x0 = 0.0f;
		float y0 = 0.0f;
		float x1 = 1.0f;
		float y1 = 1.0f;

		float xrange = maxx - minx;
		float yrange = maxy - miny;

		if ( minx < 0.0f )
		{
			x0 = -minx / xrange;
			//xrange += minx;
			maxx -= minx;
			minx = 0.0f;
		}
		if ( maxx > 1.0f )
		{
			x1 = 1.0f - ( maxx - 1.0f ) / xrange;
			maxx = 1.0f;
		}
		if ( miny < 0.0f )
		{
			y0 = -miny / yrange;
			//yrange += miny;
			maxy -= miny;
			miny = 0.0f;
		}
		if ( maxy > 1.0f )
		{
			y1 =  1.0f - ( maxy - 1.0f ) / yrange;
			maxy = 1.0f;
		}

		x = x0 * w;
		y = y0 * h;
		w = x1 * w;
		h = y1 * h;
	}

	for ( i = 0; i < MAX_ACT_TEAMS; i++ )
	{
		if ( m_pBackground[ i ] )
		{
			m_pBackground[ i ]->SetPos( x, y );
			m_pBackground[ i ]->SetRenderSize( w, h );
			m_pBackground[ i ]->SetViewport( true, minx, miny, maxx, maxy );
		}
	}

	if ( includedetails )
	{
		int t;
		for ( t = 0; t < MAX_ACT_TEAMS; t++ )
		{
			for ( i = 0; i < MAX_ACTS; i++ )
			{
				Overlays *p = &m_rgOverlays[ t ][ i ];
				if ( !p->m_bInUse )
					continue;

				if ( !p->m_pOverlay )
					continue;

				p->m_pOverlay->SetPos( x, y );
				p->m_pOverlay->SetRenderSize( w, h );
				p->m_pOverlay->SetViewport( true, minx, miny, maxx, maxy );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMinimapPanel::PaintActOverlays( int teamIndex, int alpha )
{
	Assert( teamIndex >= 0 && teamIndex < MAX_ACT_TEAMS );

	bool textshowing = false;

	int i = GetCurrentActNumber();
	if ( i != ACT_NONE_SPECIFIED )
	{
		i = clamp( i, 0, MAX_ACTS - 1 );
		Overlays *p = &m_rgOverlays[ teamIndex ][ i ];
		if ( p->m_bInUse )
		{
			int r, g, b, a;

			if ( p->m_pOverlay )
			{
				p->m_pOverlay->GetColor( r, g, b, a );
				Color clr( r, g, b, alpha );
				p->m_pOverlay->SetColor( clr );
				p->m_pOverlay->DoPaint( NULL, 0, (float)alpha/255.0f );
			}

			if ( p->m_pText && m_pTextPanel )
			{
				p->m_pText->GetColor( r, g, b, a );
				Color clr( r, g, b, alpha );
				p->m_pText->SetColor( clr );

				m_pTextPanel->SetImage( p->m_pText );

				clr = m_BackgroundColor;
				clr[3] = alpha;
				m_pBackgroundPanel->SetBgColor( clr );
 
				textshowing = true;
			}				
		}
	}
	
	if ( !textshowing )
	{
		m_pTextPanel->SetVisible( false );
		m_pTextPanel->SetImage( NULL );
		m_pBackgroundPanel->SetVisible( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMinimapPanel::OnThink()
{
	BaseClass::OnThink();

	C_BaseTFPlayer *local = C_BaseTFPlayer::GetLocalPlayer();
	if ( local )
	{
		if ( local->m_TFLocal.m_bForceMapOverview && m_bMinimapZoomed != local->m_TFLocal.m_bForceMapOverview )
		{
			SetMinimapZoom( local->m_TFLocal.m_bForceMapOverview );
		}
	}

	if ( !IsVisible() )
		return;

	if ( m_flZoomAmount != m_flPrevZoomAmount )
	{
		m_flPrevZoomAmount = m_flZoomAmount;
		ComputeMapOrigin( m_vecCurrentOrigin );	
		InvokeOnTickOnChildren( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMinimapPanel::Paint()
{
	if ( gHUD.IsHidden( HIDEHUD_MISCSTATUS ) )
	{
		// Remove the minimap zoom if the hud's hidden
		SetMinimapZoom( false );
		return;
	}

	C_BasePlayer *local = C_BasePlayer::GetLocalPlayer();
	int team = 0;
	if ( local )
	{
		team = local->GetTeamNumber();
	}

	int w, h;
	GetSize( w, h );

	int alpha;
	bool shouldDrawDetails = ShouldDrawZoomDetails( alpha );

	if ( m_pTextPanel && m_pBackgroundPanel )
	{
		m_pTextPanel->SetVisible( shouldDrawDetails );
		m_pBackgroundPanel->SetVisible( shouldDrawDetails );
	}

	// DEBUGGING:  Allow cvar override
	if ( current_team.GetInt() >= 0 )
	{
		team = current_team.GetInt();
	}

	// Can can be 0 through MAX_ACT_TEAMS
	team = clamp( team, 0, MAX_ACT_TEAMS );

	// Array index is 0 to MAX_ACT_TEAMS - 1 where a team of zero means no team and won't be indexed
	//  due to logic that checks team > 0 
	int teamIndex = clamp( team - 1, 0, MAX_ACT_TEAMS - 1 );

	if ( m_pBackground[ teamIndex ] )
	{
		if ( shouldDrawDetails )
		{
			Color clr = m_BackgroundColor;
			clr[3] *= alpha / 255.0f;
			surface()->DrawSetColor( clr );
			surface()->DrawFilledRect( 0, 0, w, h );
		}

		float offsetx, offsety;

		// Need to translate m_vecCurrentOrigin into minimap space
		InternalWorldToMinimap( 
			MINIMAP_NOCLIP, 
			m_vecCurrentOrigin,
			-m_vecMapCenter,
			1,
			offsetx, offsety );

		// Scale to 0.0f to 1.0f
		offsetx /= (float)w;
		offsety /= (float)h;

		float minx, maxx, miny, maxy;

		float xscale = 1.0f;
		float yscale = 1.0f;
		float startx = 0.0f;
		float starty = 0.0f;

		Assert( m_flAspectAdjustment > 0.0f );

		// Note, the scale sense is inverted here
		float invaspect = 1.0f / m_flAspectAdjustment;

		if ( m_flAspectAdjustment < 1.0f )
		{
			xscale = invaspect;
			startx = ( 1.0f - xscale ) * 0.5f;
		}
		else
		{
			yscale = m_flAspectAdjustment;
			starty = ( 1.0f - yscale ) * 0.5f;
		}

		float halfzoom = ( 1.0f / GetAdjustedZoom() ) * 0.5f;

		// zoom scale is already normalized, so just take half in one direction, half the other
		minx = startx + xscale * ( offsetx - halfzoom );
		miny = starty + yscale * ( offsety - halfzoom );
		maxx = startx + xscale * ( offsetx + halfzoom );
		maxy = starty + yscale * ( offsety + halfzoom );

		SetBackgroundViewport( minx, miny, maxx, maxy, shouldDrawDetails );

		m_pBackground[ teamIndex ]->DoPaint( NULL );

		if ( shouldDrawDetails && ( team > 0 ) )
		{
			PaintActOverlays( teamIndex, alpha );
		}
	}
	else
	{
		Color clr = m_BackgroundColor;
		clr[3] *= alpha * 0.9f / 255.0f;

		surface()->DrawSetColor( clr );
		surface()->DrawFilledRect( 0, 0, w, h );
	}

	// Dumb place to do this
	if ( !shouldDrawDetails && CurrentActIsAWaitingAct() )
	{
		char *cmsg = "Wait for game start...";
		int width, height;
		messagechars->GetStringLength( g_hFontTrebuchet24, &width, &height, cmsg );
		messagechars->DrawString( g_hFontTrebuchet24, XRES(16), ScreenHeight() - (height * 6), 255, 255, 245, 255, cmsg, IMessageChars::MESSAGESTRINGID_NONE );
	}

	Color border = m_BorderColor;
	border[3] = border[3] * ( alpha * 0.9f ) / 255.0f;

	//border = vgui::Color( 255, 0, 120, 255 );

	surface()->DrawSetColor( border );
	surface()->DrawOutlinedRect( 0, 0, w, h );
	surface()->DrawOutlinedRect( 1, 1, w-1, h-1 );
}

void CMinimapPanel::InvokeOnTickOnChildren( vgui::Panel *parent )
{
	if ( !parent )
		return;

	int c = parent->GetChildCount();
	int i;
	for ( i = 0; i < c; i++ )
	{
		vgui::Panel *child = parent->GetChild( i );
		child->OnTick();
		InvokeOnTickOnChildren( child );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMinimapPanel::OnTick()
{
	// See if the act's changed. If it has, bring up the act overlays.
	if ( m_nCurrentAct != GetCurrentActNumber() )
	{
	//	g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "MinimapActChanged" );
	//	SetMinimapZoom( true );
		m_nCurrentAct = GetCurrentActNumber();
	}

	// Cache these only once per frame if in a valid game
	if ( C_BasePlayer::GetLocalPlayer() && minimap_visible.GetBool() )
	{
		SetVisible( true );
		ComputeMapOrigin( m_vecCurrentOrigin );	
	}
	else
	{
		SetVisible( false );
	}

	InvokeOnTickOnChildren( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : alpha - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMinimapPanel::ShouldDrawZoomDetails( int& alpha )
{
	alpha = (int)m_flDetailsAlpha;
	alpha = clamp( alpha, 0, 255 );

	if ( !alpha )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : center - 
//-----------------------------------------------------------------------------
void CMinimapPanel::ComputeMapOrigin( Vector& origin )
{
	Vector worldmins, worldmaxs, worldsize;
	MapData().GetMapBounds( worldmins, worldmaxs );
	VectorSubtract( worldmaxs, worldmins, worldsize );

	// Cache true map center
	m_vecMapCenter = ( worldmins + worldmaxs ) * 0.5f;

	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();

	Vector playerOrigin; 

	if( !pPlayer )
	{
		playerOrigin = m_vecMapCenter;
	}
	else
	{
		playerOrigin = pPlayer->GetAbsOrigin();
	}

	origin.Init();
	 
	playerOrigin.z = m_vecMapCenter.z = 0.0f;

	Vector delta = playerOrigin - m_vecMapCenter;

	// Map center pointer is biased toward player origin as we become zoomed in to 1.0x to 2.5x and toward true world center as we zoom all the way out
	VectorScale( delta, m_flCenterOnPlayer, origin );

	int vw, vh;
	GetSize( vw, vh );

	m_flMapAspectRatio = 1.0f;
	m_flViewportAspectRatio = 1.0f;
	m_flAspectAdjustment = 1.0f;

	if ( vh > 0 )
	{
		m_flViewportAspectRatio = ( float )vw / ( float )vh;
	}

	if ( worldsize.y > 0 )
	{
		m_flMapAspectRatio = worldsize.x / worldsize.y;
	}

	if ( m_flViewportAspectRatio > 0 )
	{
		m_flAspectAdjustment = m_flMapAspectRatio / m_flViewportAspectRatio;
	}

	float fittedworldunitsperpixel;
	float zoomedworldunitsperpixel;
	float zooomedoutworldunitsperpixel;
	float actualworldunitsperpixel;

	if ( m_flAspectAdjustment > 1.0f )
	{
		// World height fits exactly in minimap height at zoom 1x
		fittedworldunitsperpixel = worldsize.y / (float)( vh );

		// At higher zoom we get less world units per pixel
		zoomedworldunitsperpixel = fittedworldunitsperpixel / m_flZoomAmount;

		// at fully zoomed back view, world width fits window width instead
		zooomedoutworldunitsperpixel = worldsize.x / (float)( vw );

		// As we center more on player, we move away from zoomed out units and toward zoomed units per pixel
		actualworldunitsperpixel = zooomedoutworldunitsperpixel + m_flCenterOnPlayer * ( zoomedworldunitsperpixel - zooomedoutworldunitsperpixel );

		m_flZoomAdjust = (1-m_flCenterOnPlayer) + m_flCenterOnPlayer * ( 1/m_flViewportAspectRatio );
	}
	else
	{
		// World width fits exactly in minimap width at zoom 1x
		fittedworldunitsperpixel = worldsize.x / (float)( vw );

		// At higher zoom we get less world units per pixel
		zoomedworldunitsperpixel = fittedworldunitsperpixel / m_flZoomAmount;

		// at fully zoomed back view, world height fits window height instead
		zooomedoutworldunitsperpixel = worldsize.y / (float)( vh );

		// As we center more on player, we move away from zoomed out units and toward zoomed units per pixel
		actualworldunitsperpixel = zooomedoutworldunitsperpixel + m_flCenterOnPlayer * ( zoomedworldunitsperpixel - zooomedoutworldunitsperpixel );

		m_flZoomAdjust = (1-m_flCenterOnPlayer) + m_flCenterOnPlayer * ( 1/m_flAspectAdjustment );
	}

	Vector preOrigin = origin;

	float inset_pixels = m_flInsetPixels;

	float viewport_width_world_units	= ( float )( vw - 2 * inset_pixels ) * actualworldunitsperpixel;
	float viewport_height_world_units	= ( float )( vh - 2 * inset_pixels ) * actualworldunitsperpixel;

	// Insets apply when centering on player
	m_flWorldSpaceInsets[ 0 ] = MIN( m_vecMapCenter.x, worldmins.x + m_flCenterOnPlayer * ( viewport_width_world_units ) * 0.5f );
	m_flWorldSpaceInsets[ 1 ] = MIN( m_vecMapCenter.y, worldmins.y + m_flCenterOnPlayer * ( viewport_height_world_units ) * 0.5f );
	m_flWorldSpaceInsets[ 2 ] = MAX( m_vecMapCenter.x, worldmaxs.x - m_flCenterOnPlayer * ( viewport_width_world_units ) * 0.5f );
	m_flWorldSpaceInsets[ 3 ] = MAX( m_vecMapCenter.y, worldmaxs.y - m_flCenterOnPlayer * ( viewport_height_world_units ) * 0.5f );
	
	// Assuming origin is at center of view, compute world space left, top, right, bottom
	m_flWorldSpaceBounds[ 0 ] = m_vecMapCenter.x + origin.x - viewport_width_world_units * 0.5f;
	m_flWorldSpaceBounds[ 1 ] = m_vecMapCenter.y + origin.y - viewport_height_world_units * 0.5f;
	m_flWorldSpaceBounds[ 2 ] = m_vecMapCenter.x + origin.x + viewport_width_world_units * 0.5f;
	m_flWorldSpaceBounds[ 3 ] = m_vecMapCenter.y + origin.y + viewport_height_world_units * 0.5f;

	// Clip these bounds
	m_flClippedWorldSpaceBounds[ 0 ] = MAX( worldmins.x, m_flWorldSpaceBounds[ 0 ] );
	m_flClippedWorldSpaceBounds[ 1 ] = MAX( worldmins.y, m_flWorldSpaceBounds[ 1 ] );
	m_flClippedWorldSpaceBounds[ 2 ] = MIN( worldmaxs.x, m_flWorldSpaceBounds[ 2 ] );
	m_flClippedWorldSpaceBounds[ 3 ] = MIN( worldmaxs.y, m_flWorldSpaceBounds[ 3 ] );

	// Clip origin to inserts
	origin.x = clamp( origin.x, m_flWorldSpaceInsets[ 0 ] - m_vecMapCenter.x, m_flWorldSpaceInsets[ 2 ] - m_vecMapCenter.x );
	origin.y = clamp( origin.y, m_flWorldSpaceInsets[ 1 ] - m_vecMapCenter.y, m_flWorldSpaceInsets[ 3 ] - m_vecMapCenter.y );

	/*
	engine->Con_NPrintf( 1, "map bounds left %i top %i right %i bottom %i", 
		(int)worldmins.x,
		(int)worldmins.y,
		(int)worldmaxs.x,
		(int)worldmaxs.y );

	engine->Con_NPrintf( 2, "world space bounds left %i top %i right %i bottom %i", 
		(int)m_flWorldSpaceBounds[ 0 ],
		(int)m_flWorldSpaceBounds[ 1 ],
		(int)m_flWorldSpaceBounds[ 2 ],
		(int)m_flWorldSpaceBounds[ 3 ] );

	engine->Con_NPrintf( 3, "world space insets left %i top %i right %i bottom %i", 
		(int)m_flWorldSpaceInsets[ 0 ],
		(int)m_flWorldSpaceInsets[ 1 ],
		(int)m_flWorldSpaceInsets[ 2 ],
		(int)m_flWorldSpaceInsets[ 3 ] );

	engine->Con_NPrintf( 4, "world space clipping left %i top %i right %i bottom %i", 
		(int)m_flClippedWorldSpaceBounds[ 0 ],
		(int)m_flClippedWorldSpaceBounds[ 1 ],
		(int)m_flClippedWorldSpaceBounds[ 2 ],
		(int)m_flClippedWorldSpaceBounds[ 3 ] );


	engine->Con_NPrintf( 5, "world center %i %i", 
		(int)m_vecMapCenter.x, (int)m_vecMapCenter.y );

	engine->Con_NPrintf( 6, "player origin %i %i", 
		(int)playerOrigin.x, (int)playerOrigin.y );

	engine->Con_NPrintf( 7, "desired map center %i %i", 
		(int)( m_vecMapCenter.x + preOrigin.x ), (int)( preOrigin.y + m_vecMapCenter.x ) );

	engine->Con_NPrintf( 8, "actual map center %i %i", 
		(int)( m_vecMapCenter.x + origin.x ), (int)( origin.y + m_vecMapCenter.y ) );

	engine->Con_NPrintf( 9, "viewport (%ix%i) aspect %.2f world (%ix%i) aspect %f",
		vw, vh, m_flViewportAspectRatio, (int)worldsize.x, (int)worldsize.y, m_flMapAspectRatio );
	
	engine->Con_NPrintf( 10, "zoom %.3f zoom adjust %.3f", 
		m_flZoomAmount, m_flZoomAdjust );

	engine->Con_NPrintf( 11, "viewport %i x %i", 
		(int)viewport_width_world_units, (int)viewport_height_world_units );
	*/

	// Assume 100% scale and no x or y offset to make up for aspect ration diff
	m_flNormalizedXScale = 1.0f;
	m_flNormalizedYScale = 1.0f;
	m_flNormalizedXOffset = 0.0f;
	m_flNormalizedYOffset = 0.0f;

	if ( m_flAspectAdjustment < 1.0f )
	{
		m_flNormalizedXScale = m_flAspectAdjustment;
		// Offset 0->1 version of x value
		m_flNormalizedXOffset = ( 1.0f - m_flNormalizedXScale ) * 0.5f;
	}
	else
	{
		m_flNormalizedYScale = 1.0f / m_flAspectAdjustment;
		// Offset 0->1 version of y value
		m_flNormalizedYOffset = ( 1.0f - m_flNormalizedYScale ) * 0.5f;
	}
}

void CMinimapPanel::InitOverlays( const char *materialrootname )
{
	if ( !materialrootname || !materialrootname[0] )
		return;

	int t;
	for ( t = 0; t < MAX_ACT_TEAMS; t++ )
	{
		char teamnum[ 2 ];
		Q_snprintf( teamnum, sizeof( teamnum ), "%01i", t + 1 );

		int i;
		for ( i = 0; i < MAX_ACTS; i++ )
		{
			char actnum[ 3 ];

			char filename[ 512 ];

			Q_snprintf( actnum, sizeof( actnum ), "%02i", i );

			Overlays *p = &m_rgOverlays[ t ][ i ];
			Assert( p && !p->m_bInUse );

			Q_snprintf( filename, sizeof( filename ), "%s_act%s_overlay_team%s",
				materialrootname, actnum, teamnum );

			// Check if file exists
			if ( g_pFullFileSystem->FileExists( VarArgs( "materials/%s.vmt", filename ) ) )
			{
				p->m_pOverlay = new BitmapImage( GetVPanel(), filename );
				p->m_bInUse = true;
			}
			else
			{
				// Try it without the team number
				Q_snprintf( filename, sizeof( filename ), "%s_act%s_overlay",
					materialrootname, actnum );

				if ( g_pFullFileSystem->FileExists( VarArgs( "materials/%s.vmt", filename ) ) )
				{
					p->m_pOverlay = new BitmapImage( GetVPanel(), filename );
					p->m_bInUse = true;
				}
			}

			Q_snprintf( filename, sizeof( filename ), "%s_act%s_text_team%s",
				materialrootname, actnum, teamnum );

			// Check if file exists
			if ( g_pFullFileSystem->FileExists( VarArgs( "materials/%s.vmt", filename ) ) )
			{
				p->m_pText = new BitmapImage( GetTextPaintPanel()->GetVPanel(), filename );
				p->m_bInUse = true;
			}
			else
			{
				// Try it without the team number
				Q_snprintf( filename, sizeof( filename ), "%s_act%s_text",
					materialrootname, actnum );

				// Check if file exists
				if ( g_pFullFileSystem->FileExists( VarArgs( "materials/%s.vmt", filename ) ) )
				{
					p->m_pText = new BitmapImage( GetTextPaintPanel()->GetVPanel(), filename );
					p->m_bInUse = true;
				}
			}
		}	
	}
}

void CMinimapPanel::ShutdownOverlays( void )
{
	int t;
	for ( t = 0; t < MAX_ACT_TEAMS; t++ )
	{

		int i;
		for ( i = 0; i < MAX_ACTS; i++ )
		{
			Overlays *p = &m_rgOverlays[ t ][ i ];
			Assert( p );
			if ( !p->m_bInUse )
				continue;

			delete p->m_pOverlay;
			delete p->m_pText;
			p->m_bInUse = false;
			p->m_pOverlay = NULL;
			p->m_pText = NULL;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Panel
//-----------------------------------------------------------------------------
Panel *CMinimapPanel::GetTextPaintPanel( void )
{
	ClientModeTFBase *basemode = ( ClientModeTFBase * )g_pClientMode;
	if ( !basemode )
	{
		Assert( 0 );
		return NULL;
	}

	return basemode->GetMinimapParent();
}

void CMinimapPanel::ZoomIn( void )
{
	if ( m_flDetailsAlpha > 0 )
	{
		if ( m_nZoomLevel != 0 )
		{
		//	g_pClientMode->GetViewportAnimationController()->StartAnimationSequence(
		//		"MinimapZoomLevel0" );
		}

		// Full window
		m_nZoomLevel = 0;
	}
	else
	{
		m_nZoomLevel = ( m_nZoomLevel + 1 ) % ( NUM_WIDTHS );
		m_nZoomLevel = clamp( m_nZoomLevel, 0, NUM_WIDTHS - 1 );	

		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence(
			m_nZoomLevel == 0 ? 
				"MinimapZoomLevel0" :
				"MinimapZoomLevel1" );
	}
}

void CMinimapPanel::Zoom_Minimap_f( void )
{
	ClientModeTFBase *basemode = ( ClientModeTFBase * )g_pClientMode;
	if ( !basemode )
		return;

	CMinimapPanel *minimap = basemode->GetMinimap();
	if ( !minimap )
		return;

	minimap->ZoomIn();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMinimapPanel::ToggleMinimap( void )
{
	int iKeybits = ::input->GetButtonBits( 0 );
	bool hitting_button = ( iKeybits & (IN_ATTACK | IN_ATTACK2 | IN_JUMP) ) ? true : false;
	if ( hitting_button && !m_bMinimapZoomed )
	{
		CLocalPlayerFilter filter;
		C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, "ClientModeTFNormal.ToggleMinimap" );
	}

	SetMinimapZoom( !m_bMinimapZoomed );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Toggle_Minimap_f( void )
{
	ClientModeTFBase *basemode = ( ClientModeTFBase * )g_pClientMode;
	if ( !basemode )
		return;

	CMinimapPanel *minimap = basemode->GetMinimap();
	if ( !minimap )
		return;
	minimap->ToggleMinimap();
}

static ConCommand minimap( "minimap", Toggle_Minimap_f, "Toggle size of the tf2 minimap." );

//-----------------------------------------------------------------------------
// Purpose: Set the state of the minimap's zoom
//-----------------------------------------------------------------------------
void CMinimapPanel::SetMinimapZoom( bool bZoom )
{
	C_BaseTFPlayer *local = C_BaseTFPlayer::GetLocalPlayer();
	if ( local && local->m_TFLocal.m_bForceMapOverview ) 
	{
		bZoom = true;
	}

	bool changed = bZoom != m_bMinimapZoomed;
	m_bMinimapZoomed = bZoom;
	if ( bZoom )
	{
		m_nZoomLevel = 0;
	}

	if ( changed )
	{
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence(
			m_bMinimapZoomed ?
			"MinimapZoomToFullScreen" :
			"MinimapZoomToCorner");

		CLocalPlayerFilter filter;
		C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, m_bMinimapZoomed ? "Minimap.ZoomIn" : "Minimap.ZoomOut" );

	}
}

//-----------------------------------------------------------------------------
// Purpose: Get at input data before it's used
//-----------------------------------------------------------------------------
void CMinimapPanel::ProcessInput()
{
	int iKeybits = ::input->GetButtonBits( 0 );

	bool hitting_button = ( iKeybits & (IN_ATTACK | IN_ATTACK2 | IN_JUMP) ) ? true : false;

	// While the minimap's zoomed, 
	if ( m_bMinimapZoomed && hitting_button )
	{
		SetMinimapZoom( false );
		::input->ClearInputButton( (IN_ATTACK | IN_ATTACK2 | IN_JUMP) );
	}

	CHudElement::ProcessInput();
}

static ConCommand zoom_minimap( "zoom_minimap", CMinimapPanel::Zoom_Minimap_f, "Zoom in on minimap." );




