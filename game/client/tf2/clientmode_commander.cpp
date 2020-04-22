//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $NoKeywords: $
//===========================================================================//
#include "cbase.h"
#include "hud_chat.h"
#include "clientmode_commander.h"
#include "vgui_int.h"
#include "ivmodemanager.h"
#include "iinput.h"
#include "kbutton.h"
#include "usercmd.h"
#include "c_basetfplayer.h"
#include "view_shared.h"
#include "in_main.h"
#include "commanderoverlaypanel.h"
#include "iviewrender.h"
#include <vgui/IInput.h>
#include <vgui/IPanel.h>


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

enum
{
	VISIBLE_STATIC_PROP_HEIGHT = 7500
};

extern Vector g_vecRenderOrigin;
extern QAngle g_vecRenderAngles;

static ConVar Commander_SlueSpeed( "commander_speed", "800.0", 0 );
static ConVar Commander_MouseSpeed( "commander_mousespeed", "200.0", 0 );
static ConVar Commander_RightMoveSpeedScale( "commander_rightmovespeedscale", "2.0", 0 );
static ConVar Commander_InvertMouse( "commander_invertmouse", "1.0", 0 );

// Public version of the commander mode;
IClientMode *ClientModeCommander()
{
	// TF2 Commander View Mode
	static CClientModeCommander g_ClientModeCommander;
	return &g_ClientModeCommander;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *commander - 
//-----------------------------------------------------------------------------
void CCommanderViewportPanel::SetCommanderView( CClientModeCommander *commander )
{
	m_pCommanderView = commander;

	if ( m_pOverlayPanel )
	{
		m_pOverlayPanel->SetCommanderView( m_pCommanderView );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CCommanderOverlayPanel
//-----------------------------------------------------------------------------
CCommanderOverlayPanel *CCommanderViewportPanel::GetCommanderOverlayPanel( void )
{
	return m_pOverlayPanel;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCommanderViewportPanel::CCommanderViewportPanel( void ) :
	m_CursorCommander( vgui::dc_arrow ),
	m_CursorRightMouseMove(vgui::dc_hand)
{
	m_pOverlayPanel = new CCommanderOverlayPanel();
	m_pOverlayPanel->SetParent( this );

	SetPaintEnabled( false );
	SetPaintBorderEnabled( false );
	SetPaintBackgroundEnabled( false );

	SetCursor( m_CursorCommander );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCommanderViewportPanel::~CCommanderViewportPanel( void )
{
}

//-----------------------------------------------------------------------------
// call these when commander view is enabled/disabled
//-----------------------------------------------------------------------------


void CCommanderViewportPanel::Enable()
{
	vgui::VPANEL pRoot = VGui_GetClientDLLRootPanel();

	SetCursor(m_CursorCommander);
	vgui::surface()->SetCursor( m_CursorCommander );

	// Make the viewport fill the root panel.
	if ( pRoot)
	{
		int wide, tall;
		vgui::ipanel()->GetSize(pRoot, wide, tall);
		SetBounds(0, 0, wide, tall);
	}

	C_BaseEntity *ent = cl_entitylist->GetEnt( 0 );

	if ( m_pOverlayPanel && ent )
	{
		m_pOverlayPanel->Enable();
	}

	SetVisible( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCommanderViewportPanel::Disable()
{
	if ( m_pOverlayPanel )
	{
		m_pOverlayPanel->Disable();
	}

	SetVisible( false );
}

void CCommanderViewportPanel::MinimapClicked( const Vector& clickWorldPos )
{
	// Don't use Z... our current z is what we want
	Vector actualOrigin, offset;
	VectorCopy( clickWorldPos, actualOrigin );
	actualOrigin.z = m_pOverlayPanel->TacticalOrigin().z;

	m_pOverlayPanel->ActualToVisibleOffset( offset );
	VectorSubtract( actualOrigin, offset, actualOrigin );
	m_pOverlayPanel->BoundOrigin( actualOrigin );
	m_pOverlayPanel->TacticalOrigin() = actualOrigin;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CClientModeCommander::CClientModeCommander() : BaseClass()
{
	m_pClear = NULL;
	m_pSkyBox = NULL;
	m_ScaledSlueSpeed = 10;
	m_Log_BaseEto2 = 1.4427f;	// factor to convert from a logarithm of base E to base 2.

	m_pViewport = new CCommanderViewportPanel;
	GetCommanderViewport()->SetCommanderView( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CClientModeCommander::~CClientModeCommander()
{

}

CCommanderViewportPanel *CClientModeCommander::GetCommanderViewport()
{
	Assert( m_pViewport );
	return static_cast< CCommanderViewportPanel * >( m_pViewport );
}

//-----------------------------------------------------------------------------
// Purpose: Called once at dll load time
//-----------------------------------------------------------------------------
void CClientModeCommander::Init( void )
{
	BaseClass::Init();
	GetCommanderViewport()->RequestFocus();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : vgui::Panel
//-----------------------------------------------------------------------------
vgui::Panel *CClientModeCommander::GetMinimapParent( void )
{
	return GetCommanderOverlayPanel();
}

//-----------------------------------------------------------------------------
// Inherited from IMinimapClient
//-----------------------------------------------------------------------------
void CClientModeCommander::MinimapClicked( const Vector& clickWorldPos )
{
	if ( GetCommanderViewport() )
	{
		GetCommanderViewport()->MinimapClicked( clickWorldPos );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCommanderOverlayPanel	*CClientModeCommander::GetCommanderOverlayPanel( void )
{
	if ( GetCommanderViewport() )
	{
		return GetCommanderViewport()->GetCommanderOverlayPanel();
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

void CClientModeCommander::Enable()
{
	// HACK: Find a better place for these
	m_pClear = (ConVar *)cvar->FindVar( "gl_clear" );
	m_pSkyBox = (ConVar *)cvar->FindVar( "r_drawskybox" );

	HudCommanderOverlayMgr()->Enable( true );

	BaseClass::Enable();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CClientModeCommander::Disable()
{
	BaseClass::Disable();

	::input->ResetMouse();
	
	HudCommanderOverlayMgr()->Enable( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

void CClientModeCommander::Update()
{
	if ( !engine->IsInGame() )
	{
		// Disable commander view
		modemanager->SwitchMode( false, false );
		return;
	}

	ClientModeTFBase::Update();

	Vector mins, maxs;
	GetCommanderViewport()->GetCommanderOverlayPanel()->GetVisibleArea( mins, maxs );
	MapData().SetVisibleArea( mins, maxs );

	HudCommanderOverlayMgr()->Tick( );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CClientModeCommander::Layout()
{
	BaseClass::Layout();

	// Force it to recompute it's boundaries
	GetCommanderViewport()->GetCommanderOverlayPanel()->Disable();
	GetCommanderViewport()->GetCommanderOverlayPanel()->Enable();
}

//-----------------------------------------------------------------------------
// Purpose: The mode can choose to not draw fog
//-----------------------------------------------------------------------------
bool CClientModeCommander::ShouldDrawFog( void )
{
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Checks map bounds and determines ideal height for tactical view
// Input  : fov - 
//			zoom - 
// Output : float
//-----------------------------------------------------------------------------
float CClientModeCommander::GetHeightForMap( float zoom )
{
	Vector mins, maxs;
	MapData().GetMapBounds( mins, maxs );
	return maxs.z + TACTICAL_ZOFFSET; 
}


bool CClientModeCommander::GetOrthoParameters(CViewSetup *pSetup)
{
	Vector vCenter;
	float xSize, ySize;
	GetCommanderViewport()->GetCommanderOverlayPanel()->GetOrthoRenderBox(vCenter, xSize, ySize);

	pSetup->m_bOrtho = true;
	pSetup->m_OrthoLeft   = -xSize;
	pSetup->m_OrthoTop    = -ySize;
	pSetup->m_OrthoRight  = xSize;
	pSetup->m_OrthoBottom = ySize;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *angles - 
//-----------------------------------------------------------------------------
void CClientModeCommander::OverrideView( CViewSetup *pSetup )
{
	// Turn off vis when in commander mode
	view->DisableVis();
	VectorCopy( GetCommanderViewport()->GetCommanderOverlayPanel()->TacticalAngles(), pSetup->angles );
	VectorCopy( GetCommanderViewport()->GetCommanderOverlayPanel()->TacticalOrigin(), pSetup->origin );
}

//-----------------------------------------------------------------------------
// Purpose: Scale commander slue speed based on viewport zoom factor
// Output : float
//-----------------------------------------------------------------------------
float CClientModeCommander::GetScaledSlueSpeed( void )
{
	return m_ScaledSlueSpeed;
}

//-----------------------------------------------------------------------------
// Purpose: Convert move to scaled move
// Input  : in - 
// Output : float
//-----------------------------------------------------------------------------
float CClientModeCommander::Commander_ResampleMove( float in )
{
	float sign;
	float move;

	if ( !in )
		return 0.0;

	sign = in > 0.0 ? 1.0 : -1.0;
	
	move = GetScaledSlueSpeed();

	return move * sign;
}

//-----------------------------------------------------------------------------
// Purpose: Zero out any movement in the command
// Input  : *cmd - 
//-----------------------------------------------------------------------------
void CClientModeCommander::ResetCommand( CUserCmd *cmd )
{
	cmd->buttons = 0;
	cmd->forwardmove = 0;
	cmd->sidemove = 0;
	cmd->upmove = 0;
	cmd->viewangles.Init();
}	

//-----------------------------------------------------------------------------
// Purpose: TF2 commander mode movement logic
//-----------------------------------------------------------------------------
void CClientModeCommander::IsometricMove( CUserCmd *cmd )
{
	int i;
	Vector wishvel;
	float fmove, smove;
	Vector forward, right, up;

	AngleVectors ( cmd->viewangles, &forward, &right, &up);  // Determine movement angles
	
	// Copy movement amounts
	fmove = cmd->forwardmove;
	smove = cmd->sidemove;
	
	// No up / down movement
	forward.Init(1, 0, 0);
	right.Init(0, -1, 0);

	wishvel.Init();
	
	// Determine x and y parts of velocity
	for (i=0; i < 3; i++)       
	{
		wishvel[i] = forward[i]*fmove + right[i]*smove;
	}

	GetCommanderViewport()->GetCommanderOverlayPanel()->TacticalOrigin() += TICK_INTERVAL * wishvel;
	GetCommanderViewport()->GetCommanderOverlayPanel()->BoundOrigin( GetCommanderViewport()->GetCommanderOverlayPanel()->TacticalOrigin() );
}

#define WINDOWED_KEEPMOVING_PIXELS 300
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : frametime - 
//			*cmd - 
//-----------------------------------------------------------------------------
void CClientModeCommander::CreateMove( float flInputSampleTime, CUserCmd *cmd )
{
	int mx, my;
	int realx, realy;
	//int sidex, sidey;

	cmd->upmove = 0;

	// Figure out the speed scale so their perceptual movement speed stays the same.
	m_ScaledSlueSpeed  = Commander_SlueSpeed.GetFloat() * GetCommanderViewport()->GetCommanderOverlayPanel()->WorldUnitsPerPixel();
	m_ScaledMouseSpeed = Commander_MouseSpeed.GetFloat() * GetCommanderViewport()->GetCommanderOverlayPanel()->WorldUnitsPerPixel();

	// Translate WASD while in commander mode...
	float temp = cmd->forwardmove;
	// Swap forward/right
	cmd->forwardmove = cmd->sidemove;
	// Invert right/left
	cmd->sidemove = -temp;

	// Normalize nonzero inputs to scaled speed
	if ( cmd->forwardmove )
	{
		cmd->forwardmove = ( cmd->forwardmove > 0 ) ? GetScaledSlueSpeed() : -GetScaledSlueSpeed();
	}
	if ( cmd->sidemove )
	{
		cmd->sidemove = ( cmd->sidemove > 0 ) ? GetScaledSlueSpeed() : -GetScaledSlueSpeed();
	}

	// Sample mouse
	::input->GetFullscreenMousePos( &mx, &my, &realx, &realy );

	if( GetCommanderViewport()->GetCommanderOverlayPanel()->IsRightMouseMapMoving() || ( in_commandermousemove.state & 1 ) )
	{
		cmd->forwardmove = m_ScaledMouseSpeed * (mx - m_LastMouseX);
		cmd->sidemove = m_ScaledMouseSpeed * (my - m_LastMouseY);

		if ( Commander_InvertMouse.GetInt() )
		{
			cmd->forwardmove *= -1.0f;
			cmd->sidemove *= -1.0f;
		}

		//input->SetFullscreenMousePos( m_LastMouseX, m_LastMouseY );
		mx = m_LastMouseX;
		my = m_LastMouseY;
	}
	/*
	else if ( input->IsFullscreenMouse() )
	{
		if ( abs( realx - mx ) < WINDOWED_KEEPMOVING_PIXELS && 
			 abs( realy - my ) < WINDOWED_KEEPMOVING_PIXELS )
		{
			sidex = 2;
			sidey = 2;

			// Check Size of viewport
			if ( mx < sidex )
			{
				cmd->forwardmove = -GetScaledSlueSpeed();
			}
			else if ( mx > ( ScreenWidth() - sidex ))
			{
				cmd->forwardmove = GetScaledSlueSpeed();
			}
			
			if ( my < sidey )
			{
				cmd->sidemove = -GetScaledSlueSpeed();
			}
			else if ( my > ( ScreenHeight() - sidey ) )
			{
				cmd->sidemove = GetScaledSlueSpeed();
			}
		}
	}
	*/

	m_LastMouseX = mx;
	m_LastMouseY = my;

	// Look straight down
	cmd->viewangles.x = 90; // 45;
	cmd->viewangles.y = 90; //45fmod( 3.0* 360 * (gpGlobals->curtime * 0.01), 360 );
	cmd->viewangles.z = 0;

	GetCommanderViewport()->GetCommanderOverlayPanel()->TacticalAngles() = cmd->viewangles;

	IsometricMove( cmd );

	// Reset command
	ResetCommand( cmd );
}

//-----------------------------------------------------------------------------
// Purpose: Makes sure the mouse is over the same world position as it started
//-----------------------------------------------------------------------------

void CClientModeCommander::MoveMouse( Vector& worldPos )
{
	Vector worldCenter;
	float wworld, hworld;
	GetCommanderViewport()->GetCommanderOverlayPanel()->GetOrthoRenderBox(worldCenter, wworld, hworld);
	wworld *= 2; hworld *= 2;

	Vector worldDelta;
	VectorSubtract( worldPos, worldCenter, worldDelta );

	int w, h;
	GetCommanderViewport()->GetSize( w, h );

	int mx, my;
	mx = (worldDelta.x / wworld + 0.5f) * w;
	my = (0.5f - worldDelta.y / hworld) * h;

	// Clamp
	if (mx < 0)	mx = 0; else if (mx > w) mx = w;
	if (my < 0)	my = 0; else if (my > h) my = h;

	//input->SetFullscreenMousePos( mx, my );

	m_LastMouseX = mx;
	m_LastMouseY = my;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *newmap - 
//-----------------------------------------------------------------------------
void CClientModeCommander::LevelInit( const char *newmap )
{
	BaseClass::LevelInit( newmap );

	HudCommanderOverlayMgr()->LevelShutdown();
	MapData().LevelInit( newmap );
	GetCommanderViewport()->GetCommanderOverlayPanel()->LevelInit( newmap );
	HudCommanderOverlayMgr()->LevelInit( );

	GetCommanderViewport()->Enable();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CClientModeCommander::LevelShutdown( void )
{
	GetCommanderViewport()->Disable();

	MapData().LevelShutdown();

	HudCommanderOverlayMgr()->LevelShutdown();

	GetCommanderViewport()->GetCommanderOverlayPanel()->LevelShutdown();

	BaseClass::LevelShutdown();
}

//-----------------------------------------------------------------------------
// returns the viewport panel
//-----------------------------------------------------------------------------
vgui::Panel *CClientModeCommander::GetViewport()
{
	return m_pViewport;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CClientModeCommander::ShouldDrawEntity(C_BaseEntity *pEnt)
{
	return MapData().IsEntityVisibleToTactical(pEnt);
}

bool CClientModeCommander::ShouldDrawDetailObjects( )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Always draw the local player while in commander mode
//-----------------------------------------------------------------------------
bool CClientModeCommander::ShouldDrawLocalPlayer( C_BasePlayer *pPlayer )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CClientModeCommander::ShouldDrawViewModel( void )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Return false to disable crosshair
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CClientModeCommander::ShouldDrawCrosshair( void )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Adjust engine rendering viewport rectangle if needed
// Input  : x - 
//			y - 
//			width - 
//			height - 
//-----------------------------------------------------------------------------
void CClientModeCommander::AdjustEngineViewport( int& x, int& y, int& width, int& height )
{
}


//-----------------------------------------------------------------------------
// Should I draw particles
//-----------------------------------------------------------------------------

bool CClientModeCommander::ShouldDrawParticles( )
{
	Vector vCenter;
	float xSize, ySize;
	GetCommanderViewport()->GetCommanderOverlayPanel()->GetOrthoRenderBox(vCenter, xSize, ySize);

	// Activate/deactivate particles rendering based on zoom level
	float maxSize = MAX( xSize, ySize );
	return (maxSize < VISIBLE_STATIC_PROP_HEIGHT);
}


//-----------------------------------------------------------------------------
// Purpose: When in commander mode, force gl_clear and don't draw the skybox
//-----------------------------------------------------------------------------

void CClientModeCommander::PreRender( CViewSetup *pSetup )
{
	if ( !m_pClear || !m_pSkyBox )
		return;

	m_fOldClear = m_pClear->GetFloat();
	m_pClear->SetValue( 1.0f );
	pSetup->clearColor = !!m_pClear->GetInt();

	m_fOldSkybox = m_pSkyBox->GetFloat();
	m_pSkyBox->SetValue( 0.0f );

	GetOrthoParameters(pSetup);
	render->DrawTopView( true );
	Vector2D mins = pSetup->origin.AsVector2D();
	Vector2D maxs = pSetup->origin.AsVector2D();
	mins.x += pSetup->m_OrthoLeft;
	maxs.x += pSetup->m_OrthoRight;
	mins.y += pSetup->m_OrthoTop;
	maxs.y += pSetup->m_OrthoBottom;
	render->TopViewBounds( mins, maxs );

	// Activate/deactivate static prop + particles rendering based on zoom level
	Vector2D size;
	Vector2DSubtract( maxs, mins, size );
	float maxSize = MAX( size.x, size.y );
	bool showStaticProps = (maxSize < VISIBLE_STATIC_PROP_HEIGHT);
	ClientLeafSystem()->DrawStaticProps(showStaticProps);
	ClientLeafSystem()->DrawSmallEntities(showStaticProps);

	BaseClass::PreRender(pSetup);
}

void CClientModeCommander::PostRenderWorld()
{
	render->DrawTopView( false );
	ClientLeafSystem()->DrawStaticProps(true);
	ClientLeafSystem()->DrawSmallEntities(true);
}

//-----------------------------------------------------------------------------
// Purpose: Restore cvar values
//-----------------------------------------------------------------------------
void CClientModeCommander::PostRender( void )
{
	if ( !m_pClear || !m_pSkyBox )
		return;

	m_pClear->SetValue( m_fOldClear );
	m_pSkyBox->SetValue( m_fOldSkybox );

	BaseClass::PostRender();
}

//-----------------------------------------------------------------------------
// Purpose: Swallow mouse wheel when in this view
// Input  : down - 
//			keynum - 
//			*pszCurrentBinding - 
// Output : int
//-----------------------------------------------------------------------------
int CClientModeCommander::KeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding )
{
	switch ( keynum )
	{
	case MOUSE_WHEEL_UP:
	case MOUSE_WHEEL_DOWN:
		// Swallow
		return 0;
	}

	// Allow engine to process
	return BaseClass::KeyInput( down, keynum, pszCurrentBinding );
}

