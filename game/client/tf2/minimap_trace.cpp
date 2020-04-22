//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hud.h"
#include "minimap_trace.h"
#include "c_basetfplayer.h"
#include "mapdata.h"
#include "model_types.h"
#include "clientmode_tfnormal.h"
#include <vgui/IVGui.h>
#include "vgui_bitmapimage.h"
#include <KeyValues.h>
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "shareddefs.h"
#include "engine/ivmodelinfo.h"

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CMinimapTracePanel::CMinimapTracePanel( vgui::Panel *parent, const char *panelName)
	: BaseClass( parent, "CMinimapTracePanel" )
{
	m_pEntity = NULL;
	SetPaintBackgroundEnabled( false );
	m_bCanScale = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMinimapTracePanel::~CMinimapTracePanel()
{
}

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
bool CMinimapTracePanel::Init( KeyValues* pKeyValues, MinimapInitData_t* pInitData )
{
	// NOTE: Can't use a EHANDLE here because the EHANDLE for pEntity is set up
	// when AddEntity is called; this gets called before that happens
	m_pEntity = pInitData->m_pEntity;
	m_vecPosition = pInitData->m_vecPosition;

	int w, h;
	if (!ParseCoord(pKeyValues, "offset", m_OffsetX, m_OffsetY))
		return false;

	if (!ParseCoord(pKeyValues, "size", w, h ))
		return false;

	// Lifetime of the minimap trace
	float flLifeTime = pKeyValues->GetFloat("lifetime", -1.0f);
	if (flLifeTime > 0.0f)
		m_flDeletionTime = gpGlobals->curtime + flLifeTime;
	else
		m_flDeletionTime = -1.0f;

	// Optional parameters
	m_bVisibleWhenZoomedIn = (pKeyValues->GetInt( "detail", 0 ) == 0 );
	m_bClampToMap = (pKeyValues->GetInt( "clamp", 0 ) != 0);
	m_bClipToMap = (pKeyValues->GetInt( "noclip", 0 ) == 0);
	m_bCanScale = (pKeyValues->GetInt( "noscale", 0 ) == 0);
	m_bVisible = true;

	m_SizeW = w;
	m_SizeH = h;

	SetSize( w, h );

	return true;
}


//-----------------------------------------------------------------------------
// Entity accessor
//-----------------------------------------------------------------------------
C_BaseEntity* CMinimapTracePanel::GetEntity()
{ 
	return m_pEntity; 
}

void CMinimapTracePanel::SetEntity( C_BaseEntity* pEntity )
{
	m_pEntity = pEntity;
}

//-----------------------------------------------------------------------------
// Sets the position of the trace in world space
//-----------------------------------------------------------------------------
void CMinimapTracePanel::SetPosition( const Vector &vecPosition )
{
	m_vecPosition = vecPosition;
}


//-----------------------------------------------------------------------------
// Computes the entity position
//-----------------------------------------------------------------------------
bool CMinimapTracePanel::GetEntityPosition( float &x, float &y )
{
	C_BaseEntity *pEntity = GetEntity();

	Vector pos;
	if (!pEntity)
	{
		pos = m_vecPosition;
	}
	else
	{
		if (pEntity == C_BaseTFPlayer::GetLocalPlayer())
		{
			// Use the predicted position...
			pos = pEntity->GetAbsOrigin();
		}
		else if ( !pEntity->GetModel() || modelinfo->GetModelType( pEntity->GetModel() ) != mod_brush )
		{  
			// If it's not a brush model, use the origin
			pos = pEntity->GetRenderOrigin();
		}
		else
		{
			Vector mins, maxs;
			pEntity->GetRenderBounds( mins, maxs );
			pos = (mins + maxs) * 0.5f;
		}

		// Position is an offset when there's an entity
		pos += m_vecPosition;
	}

	return CMinimapPanel::MinimapPanel()->WorldToMinimap( m_bClampToMap ? MINIMAP_CLIP : MINIMAP_NOCLIP, pos, x, y );
}


//-----------------------------------------------------------------------------
// Causes the minimap panel to not be visible
//-----------------------------------------------------------------------------
void CMinimapTracePanel::SetTraceVisibility( bool bVisible )
{
	m_bVisible = bVisible;
}


//-----------------------------------------------------------------------------
// Call this before rendering to see if the entity should be rendered
// and to get the rendering position
//-----------------------------------------------------------------------------
bool CMinimapTracePanel::ComputeVisibility( )
{
	if (!m_bVisible)
		return false;

	C_BaseEntity* pEntity = GetEntity();
	
	// No entity? We must be a positional trace
	if (!pEntity)
		return true;

	// Don't draw if it's not in the PVS
	if ( pEntity->IsDormant() )
		return false;

	// Check visible to tactical
	if( !MapData().IsEntityVisibleToTactical(pEntity) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// called when we're ticked...
//-----------------------------------------------------------------------------
void CMinimapTracePanel::OnTick()
{
	// Don't do anything when not in a game
	if ( !engine->IsInGame() )
	{
		SetVisible( false );
		return;
	}

	// Update our current position
	float sx, sy;
	bool onMap = GetEntityPosition( sx, sy );

	int ofsx, ofsy;
	ofsx = m_OffsetX;
	ofsy = m_OffsetY;

	if ( m_bCanScale )
	{
		int w, h;
		GetSize( w, h );

		float scale = CMinimapPanel::MinimapPanel()->GetTrueZoom();
		if ( 1 || scale != 1.0f )
		{
			ofsx *= scale;
			ofsy *= scale;

			int sizew = m_SizeW * scale;
			int sizeh = m_SizeH * scale;

			SetPos( (int)(sx + ofsx + 0.5f), (int)(sy + ofsy + 0.5f));
			SetSize( sizew, sizeh );
		}
		else
		{
			SetPos( (int)(sx + ofsx + 0.5f), (int)(sy + ofsy + 0.5f));
		}
	}
	else
	{
		SetPos( (int)(sx + ofsx + 0.5f), (int)(sy + ofsy + 0.5f));
	}

	// Update our visibility
	if (!onMap)
		SetVisible( false );
	else
		SetVisible( ComputeVisibility( ) );

	if ((m_flDeletionTime >= 0.0f) && (gpGlobals->curtime >= m_flDeletionTime))
	{
		MarkForDeletion();
	}
}


//-----------------------------------------------------------------------------
// Computes a panel alpha based on zoom level...
//-----------------------------------------------------------------------------
float CMinimapTracePanel::ComputePanelAlpha()
{
	if (m_bVisibleWhenZoomedIn)
		return 1.0f;

	int a;
	if (!CMinimapPanel::MinimapPanel())
		return 0.0f;

	if (!CMinimapPanel::MinimapPanel()->ShouldDrawZoomDetails( a ))
		return 0.0f;

	return a / 255.0f;
}


//-----------------------------------------------------------------------------
//
// A standard minimap panel that displays a bitmap
//
//-----------------------------------------------------------------------------
DECLARE_MINIMAP_FACTORY( CMinimapTraceBitmapPanel, "minimap_image_panel" );


//-----------------------------------------------------------------------------
// Sets the bitmap and size
//-----------------------------------------------------------------------------
bool CMinimapTraceBitmapPanel::Init( KeyValues* pKeyValues, MinimapInitData_t* pInitData )
{
	if (!BaseClass::Init(pKeyValues, pInitData))
		return false;

	return m_Image.Init( GetVPanel(), pKeyValues );
}


//-----------------------------------------------------------------------------
// Performs the rendering...
//-----------------------------------------------------------------------------
void CMinimapTraceBitmapPanel::Paint(  )
{
	if ( gHUD.IsHidden( HIDEHUD_MISCSTATUS ) )
		return;

	if (!m_bClipToMap)
		g_pMatSystemSurface->DisableClipping( true );
	m_Image.DoPaint( GetVPanel(), 0, ComputePanelAlpha() );
	g_pMatSystemSurface->DisableClipping( false );
}



//-----------------------------------------------------------------------------
//
// A standard minimap renderable that displays a bitmap that changes when team changes
//
//-----------------------------------------------------------------------------
DECLARE_MINIMAP_FACTORY( CMinimapTraceTeamBitmapPanel, "minimap_team_image_panel" );


//-----------------------------------------------------------------------------
// Sets the bitmap and size
//-----------------------------------------------------------------------------
bool CMinimapTraceTeamBitmapPanel::Init( KeyValues* pKeyValues, MinimapInitData_t* pInitData )
{
	if (!BaseClass::Init(pKeyValues, pInitData))
		return false;

	return m_TeamImage.Init( this, pKeyValues, pInitData->m_pEntity );
}


//-----------------------------------------------------------------------------
// Performs the rendering...
//-----------------------------------------------------------------------------
void CMinimapTraceTeamBitmapPanel::Paint( )
{
	if ( gHUD.IsHidden( HIDEHUD_MISCSTATUS ) )
		return;

	if (!m_bClipToMap)
		g_pMatSystemSurface->DisableClipping( true );
	m_TeamImage.SetAlpha( ComputePanelAlpha() );
	m_TeamImage.Paint();
	g_pMatSystemSurface->DisableClipping( false );
}
