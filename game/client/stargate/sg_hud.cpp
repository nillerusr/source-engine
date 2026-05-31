//========= StargateTC ============================================================//
//
// sg_hud.cpp - StargateTC custom HUD (1:1 port of the single GoldSrc client HUD
// element). Paints radar+compass+blips, vertical life/armor/stamina stat bars,
// naquada/progress bar, teammate portrait roster + local avatar, tactical alert,
// territory tickets, round timer, bomb key and the zat-stun overlay.
//
// Sprites come from the AI-enhanced VGUI materials at vgui/stargate/hud/* built
// by `stgs textures compile-hud`. Each is a 4x atlas padded to a power of two;
// DrawSpr() derives the content UV from the GoldSrc-native sprite size.
//=================================================================================//
#include "cbase.h"
#include "sg_hud.h"
#include "hud.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include "c_baseplayer.h"
#include "view.h"
#include <vgui/ISurface.h>
#include <vgui_controls/Controls.h>
#include "vgui/IScheme.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

DECLARE_HUDELEMENT( CHudStargate );

DECLARE_HUD_MESSAGE( CHudStargate, SgAddPortrait );
DECLARE_HUD_MESSAGE( CHudStargate, SgReset );
DECLARE_HUD_MESSAGE( CHudStargate, SgAboutMe );
DECLARE_HUD_MESSAGE( CHudStargate, SgTogglePor );
DECLARE_HUD_MESSAGE( CHudStargate, SgSetRndTim );
DECLARE_HUD_MESSAGE( CHudStargate, SgNewRnd );
DECLARE_HUD_MESSAGE( CHudStargate, SgZatTouch );
DECLARE_HUD_MESSAGE( CHudStargate, SgRadio );
DECLARE_HUD_MESSAGE( CHudStargate, SgArmor );
DECLARE_HUD_MESSAGE( CHudStargate, SgStamina );
DECLARE_HUD_MESSAGE( CHudStargate, SgSubModel );
DECLARE_HUD_MESSAGE( CHudStargate, SgHasKey );
DECLARE_HUD_MESSAGE( CHudStargate, SgHideHud );
DECLARE_HUD_MESSAGE( CHudStargate, SgHudInfoMode );
DECLARE_HUD_MESSAGE( CHudStargate, SgTicketMode );
DECLARE_HUD_MESSAGE( CHudStargate, SgGtickets );
DECLARE_HUD_MESSAGE( CHudStargate, SgTtickets );
DECLARE_HUD_MESSAGE( CHudStargate, SgProgress );
DECLARE_HUD_MESSAGE( CHudStargate, SgBlip );

// GoldSrc cvars: view_radar / view_portraits.
ConVar sg_view_radar( "sg_view_radar", "1", FCVAR_ARCHIVE, "Show the StargateTC radar." );
ConVar sg_view_portraits( "sg_view_portraits", "1", FCVAR_ARCHIVE, "Show the teammate portrait roster." );
// Inject dummy state so every HUD block can be verified in a single screenshot.
ConVar sg_hud_test( "sg_hud_test", "0", FCVAR_CHEAT, "Force-populate all StargateTC HUD blocks for layout testing." );

// GoldSrc team coloring: Tau'ri side green, Goa'uld side red.
#define SG_TEAM_TAURI   2   // StargateTC: team 2 = Tau'ri
#define SG_TEAM_GOAULD  3   // StargateTC: team 3 = Goa'uld

static const char *SG_NUM[ 10 ] = {
	"vgui/stargate/hud/num/0", "vgui/stargate/hud/num/1", "vgui/stargate/hud/num/2",
	"vgui/stargate/hud/num/3", "vgui/stargate/hud/num/4", "vgui/stargate/hud/num/5",
	"vgui/stargate/hud/num/6", "vgui/stargate/hud/num/7", "vgui/stargate/hud/num/8",
	"vgui/stargate/hud/num/9",
};
#define SG_NUM_W 16
#define SG_NUM_H 24

//-----------------------------------------------------------------------------
CHudStargate::CHudStargate( const char *pElementName )
	: CHudElement( pElementName ), BaseClass( NULL, "HudStargate" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetPaintBackgroundEnabled( false );
	SetPaintBorderEnabled( false );
	// We manage our own visibility; only hide while fully HUD-suppressed.
	SetHiddenBits( HIDEHUD_PLAYERDEAD );
}

//-----------------------------------------------------------------------------
void CHudStargate::Init( void )
{
	HOOK_HUD_MESSAGE( CHudStargate, SgAddPortrait );
	HOOK_HUD_MESSAGE( CHudStargate, SgReset );
	HOOK_HUD_MESSAGE( CHudStargate, SgAboutMe );
	HOOK_HUD_MESSAGE( CHudStargate, SgTogglePor );
	HOOK_HUD_MESSAGE( CHudStargate, SgSetRndTim );
	HOOK_HUD_MESSAGE( CHudStargate, SgNewRnd );
	HOOK_HUD_MESSAGE( CHudStargate, SgZatTouch );
	HOOK_HUD_MESSAGE( CHudStargate, SgRadio );
	HOOK_HUD_MESSAGE( CHudStargate, SgArmor );
	HOOK_HUD_MESSAGE( CHudStargate, SgStamina );
	HOOK_HUD_MESSAGE( CHudStargate, SgSubModel );
	HOOK_HUD_MESSAGE( CHudStargate, SgHasKey );
	HOOK_HUD_MESSAGE( CHudStargate, SgHideHud );
	HOOK_HUD_MESSAGE( CHudStargate, SgHudInfoMode );
	HOOK_HUD_MESSAGE( CHudStargate, SgTicketMode );
	HOOK_HUD_MESSAGE( CHudStargate, SgGtickets );
	HOOK_HUD_MESSAGE( CHudStargate, SgTtickets );
	HOOK_HUD_MESSAGE( CHudStargate, SgProgress );
	HOOK_HUD_MESSAGE( CHudStargate, SgBlip );

	Reset();
}

//-----------------------------------------------------------------------------
void CHudStargate::Reset( void )
{
	m_iLocalTeam = 0;
	m_iLocalId = 0;
	m_flArmor = 0;
	m_flStamina = 0;
	m_iSubModel = 0;
	m_bHasKey = false;
	m_iHideBits = 0;
	m_iInfoMode = 0;
	m_bTicketMode = false;
	m_iGtickets = 0;
	m_iTtickets = 0;
	m_iProgress = 0;
	m_flRoundLen = 0;
	m_flRoundStart = 0;
	m_bZat = false;
	m_flZatTime = 0;
	m_flSlideOffset = 0;
	Q_memset( m_Blips, 0, sizeof( m_Blips ) );
	Q_memset( m_Portraits, 0, sizeof( m_Portraits ) );
}

void CHudStargate::VidInit( void )
{
	// Texture ids are bound to the surface; drop the cache on resolution change.
	m_TexIds.RemoveAll();
}

void CHudStargate::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	int sw, sh;
	surface()->GetScreenSize( sw, sh );
	SetBounds( 0, 0, sw, sh );
}

//-----------------------------------------------------------------------------
bool CHudStargate::ShouldDraw( void )
{
	if ( !engine->IsInGame() )
		return false;
	// NOTE: intentionally NOT gating on CHudElement::ShouldDraw() yet - the
	// player sits in an unassigned/observer limbo until the team/class join
	// flow exists, which would otherwise suppress the whole HUD. Re-add the
	// CHudElement::ShouldDraw() gate once join/spawn is wired (Phase 7-8).
	return true;
}

//-----------------------------------------------------------------------------
void CHudStargate::OnThink( void )
{
	int sw, sh;
	surface()->GetScreenSize( sw, sh );
	if ( GetWide() != sw || GetTall() != sh )
		SetBounds( 0, 0, sw, sh );

	// Decay radar blips (GoldSrc: alpha -= 5 each frame).
	for ( int i = 0; i < SG_MAX_BLIPS; i++ )
	{
		if ( m_Blips[ i ].alpha > 0 )
		{
			m_Blips[ i ].alpha -= 5;
			if ( m_Blips[ i ].alpha < 0 )
				m_Blips[ i ].alpha = 0;
		}
	}
}

//-----------------------------------------------------------------------------
// helpers
//-----------------------------------------------------------------------------
float CHudStargate::Scale( void ) const
{
	int sw, sh;
	surface()->GetScreenSize( sw, sh );
	float s = sh / 600.0f;          // GoldSrc HUD was authored ~640x480..800x600
	return ( s < 1.0f ) ? 1.0f : s;
}

Color CHudStargate::TeamColor( int team ) const
{
	if ( team == SG_TEAM_TAURI )
		return Color( 0, 180, 0, 255 );
	if ( team == SG_TEAM_GOAULD )
		return Color( 210, 0, 0, 255 );
	return Color( 200, 200, 200, 255 );
}

int CHudStargate::GetTex( const char *pszMat )
{
	int idx = m_TexIds.Find( pszMat );
	if ( idx != m_TexIds.InvalidIndex() )
		return m_TexIds[ idx ];
	int id = surface()->CreateNewTextureID();
	surface()->DrawSetTextureFile( id, pszMat, true, false );
	m_TexIds.Insert( pszMat, id );
	return id;
}

static int SgNextPow2( int n ) { int p = 1; while ( p < n ) p <<= 1; return p; }

void CHudStargate::DrawSpr( const char *pszMat, float sx, float sy,
                            int nativeW, int nativeH, const Color &c )
{
	// content texels = native * 4 (HAT upscale); atlas padded to next pow2.
	const int texW = nativeW * 4, texH = nativeH * 4;
	const float u = (float)texW / (float)SgNextPow2( texW );
	const float v = (float)texH / (float)SgNextPow2( texH );
	const float s = Scale();
	const int x0 = (int)sx, y0 = (int)sy;
	const int x1 = x0 + (int)( nativeW * s ), y1 = y0 + (int)( nativeH * s );

	surface()->DrawSetColor( c );
	surface()->DrawSetTexture( GetTex( pszMat ) );
	surface()->DrawTexturedSubRect( x0, y0, x1, y1, 0.0f, 0.0f, u, v );
}

void CHudStargate::DrawHudNumber( int value, float x, float y, float s, const Color &c )
{
	if ( value < 0 )
		value = 0;
	char buf[ 16 ];
	Q_snprintf( buf, sizeof( buf ), "%d", value );
	float cx = x;
	for ( const char *p = buf; *p; ++p )
	{
		int d = *p - '0';
		if ( d >= 0 && d <= 9 )
			DrawSpr( SG_NUM[ d ], cx, y, SG_NUM_W, SG_NUM_H, c );
		cx += SG_NUM_W * s;
	}
}

//-----------------------------------------------------------------------------
void CHudStargate::Paint( void )
{
	int sw, sh;
	surface()->GetScreenSize( sw, sh );
	const float s = Scale();

	if ( sg_hud_test.GetBool() )
	{
		// Inject dummy state so every block is visible for layout verification.
		m_flArmor = 70;  m_flStamina = 55;  m_bHasKey = true;
		m_iInfoMode = 2; m_bTicketMode = true; m_iGtickets = 42; m_iTtickets = 37;
		m_iProgress = 66; if ( m_flRoundLen == 0 ) { m_flRoundLen = 300; m_flRoundStart = gpGlobals->curtime; }

		// A small paint probe makes VGUI-present-but-assets-missing cases obvious.
		surface()->DrawSetColor( 0, 0, 0, 180 );
		surface()->DrawFilledRect( 16, 16, 228, 88 );
		surface()->DrawSetColor( 255, 60, 40, 255 );
		surface()->DrawFilledRect( 24, 24, 220, 40 );
		surface()->DrawSetColor( 0, 190, 90, 255 );
		surface()->DrawFilledRect( 24, 48, 220, 64 );
		surface()->DrawSetColor( 60, 140, 255, 255 );
		surface()->DrawFilledRect( 24, 72, 220, 80 );
	}

	DrawFrame( sw, sh, s );
	DrawStatBars( sw, sh, s );
	if ( sg_view_radar.GetBool() )
		DrawRadar( sw, sh, s );
	DrawNaquada( sw, sh, s );
	DrawTac( sw, sh, s );
	DrawTickets( sw, sh, s );
	DrawRoundTimer( sw, sh, s );
	DrawBombKey( sw, sh, s );
	if ( sg_view_portraits.GetBool() )
		DrawPortraits( sw, sh, s );
	DrawZatOverlay( sw, sh, s );
}

//-----------------------------------------------------------------------------
// sub-blocks
//-----------------------------------------------------------------------------
void CHudStargate::DrawFrame( int sw, int sh, float s )
{
	// Bottom-left and bottom-right HUD chrome (hud_left 200x168, Hud_Right 200x168).
	const Color white( 255, 255, 255, 255 );
	DrawSpr( "vgui/stargate/hud/hud_left", 0, sh - (int)( 168 * s ), 200, 168, white );
	DrawSpr( "vgui/stargate/hud/Hud_Right", sw - (int)( 200 * s ), sh - (int)( 168 * s ), 200, 168, white );
}

void CHudStargate::DrawRadar( int sw, int sh, float s )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	int team = pPlayer ? pPlayer->GetTeamNumber() : m_iLocalTeam;
	Color disk = ( team == SG_TEAM_TAURI ) ? Color( 0, 160, 0, 255 ) : Color( 160, 0, 0, 255 );

	// Disk: top-right, 200x200 native. y shifts down when portraits present.
	bool hasPortraits = false;
	for ( int i = 0; i < SG_MAX_PORTRAITS; i++ ) if ( m_Portraits[ i ].used ) { hasPortraits = true; break; }
	const int rx = sw - (int)( 208 * s );
	const int ry = (int)( ( hasPortraits ? 112 : 8 ) * s );
	DrawSpr( "vgui/stargate/hud/radar", rx, ry, 200, 200, disk );

	// Compass needle: 16-frame ladder driven by view yaw, centered on the disk.
	float yaw = pPlayer ? pPlayer->EyeAngles().y : 0.0f;
	yaw = anglemod( yaw );
	int frame = (int)( ( yaw / 360.0f ) * 16.0f ) & 15;
	frame = ( frame + 3 ) & 15;
	frame = 15 - frame;
	char needle[ 64 ];
	Q_snprintf( needle, sizeof( needle ), "vgui/stargate/hud/needle_%03d", frame );
	const int ncx = rx + (int)( ( 100 - 16 ) * s );   // disk center - half needle
	const int ncy = ry + (int)( ( 100 - 16 ) * s );
	DrawSpr( needle, ncx, ncy, 32, 32, Color( 210, disk.g(), 0, 255 ) );

	// Blips (decaying), positioned relative to disk centre.
	const int cx = rx + (int)( 100 * s ), cy = ry + (int)( 100 * s );
	for ( int i = 0; i < SG_MAX_BLIPS; i++ )
	{
		if ( m_Blips[ i ].alpha <= 0 )
			continue;
		int bf = m_Blips[ i ].type % 6; if ( bf < 0 ) bf += 6;
		char blip[ 64 ];
		Q_snprintf( blip, sizeof( blip ), "vgui/stargate/hud/blip_%03d", bf );
		const int bx = cx - (int)( m_Blips[ i ].x * s ) - (int)( 8 * s );
		const int by = cy + (int)( m_Blips[ i ].y * s ) - (int)( 8 * s );
		DrawSpr( blip, bx, by, 16, 16, Color( 255, 255, 255, m_Blips[ i ].alpha ) );
	}
}

void CHudStargate::DrawStatBars( int sw, int sh, float s )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	const float maxv = 100.0f;
	float life = pPlayer ? (float)pPlayer->GetHealth() : 0.0f;
	const int barY = sh - (int)( 24 * s );
	const int barH = 64;

	// Vertical bars crop from the top by (max-value)/max - statbar.spr is 16x64.
	struct { const char *mat; float val; int x; Color c; } bars[] = {
		{ "vgui/stargate/hud/statbar", life,        4,    Color( 0, 220, 0, 255 ) },
		{ "vgui/stargate/hud/statbar", m_flArmor,   28,   Color( 80, 160, 255, 255 ) },
		{ "vgui/stargate/hud/statbar", m_flStamina, 52,   Color( 255, 170, 0, 255 ) },
	};
	for ( int i = 0; i < 3; i++ )
	{
		float val = bars[ i ].val;
		// life (i==0) draws even at full/empty; armor/stamina only when > 0.
		if ( i > 0 && val <= 0 )
			continue;
		float frac = val / maxv; if ( frac < 0 ) frac = 0; if ( frac > 1 ) frac = 1;
		int filledH = (int)( barH * frac );
		int topGap = barH - filledH;            // empty portion at top
		// UV: statbar is 16x64 native -> 64x256 atlas, pow2 64x256, u=v=1.
		const int texW = 16 * 4, texH = 64 * 4;
		float u = (float)texW / SgNextPow2( texW );
		float vTop = (float)topGap / 64.0f * ( (float)texH / SgNextPow2( texH ) );
		float vBot = (float)texH / SgNextPow2( texH );
		int x0 = bars[ i ].x;
		int y0 = barY + (int)( topGap * s );
		int x1 = x0 + (int)( 16 * s );
		int y1 = barY + (int)( barH * s );
		surface()->DrawSetColor( bars[ i ].c );
		surface()->DrawSetTexture( GetTex( bars[ i ].mat ) );
		surface()->DrawTexturedSubRect( x0, y0, x1, y1, 0.0f, vTop, u, vBot );
	}
}

void CHudStargate::DrawNaquada( int sw, int sh, float s )
{
	if ( m_iProgress <= 0 || m_iProgress >= 100 )
		return;
	const int bx = sw / 2 - (int)( 71 * s );
	const int by = sh - sh / 4 - (int)( 7 * s );
	DrawSpr( "vgui/stargate/hud/pbar", bx, by, 128, 32, Color( 255, 255, 255, 255 ) );
	// Fill width scaled by progress.
	float frac = m_iProgress / 100.0f;
	const int fx = sw / 2 - (int)( 64 * s );
	int fullW = 144, drawW = (int)( fullW * frac );
	const int texW = 144 * 4, texH = 48 * 4;
	float uFull = (float)texW / SgNextPow2( texW );
	float v = (float)texH / SgNextPow2( texH );
	float u = uFull * frac;
	surface()->DrawSetColor( Color( (int)( 255 * ( 1 - frac ) ), (int)( 255 * frac ), 40, 255 ) );
	surface()->DrawSetTexture( GetTex( "vgui/stargate/hud/pbare" ) );
	surface()->DrawTexturedSubRect( fx, by, fx + (int)( drawW * s ), by + (int)( 48 * s ), 0.0f, 0.0f, u, v );
}

void CHudStargate::DrawTac( int sw, int sh, float s )
{
	if ( m_iInfoMode <= 0 )
		return;
	const char *mat = "vgui/stargate/hud/tac_green";
	if ( m_iInfoMode == 1 ) mat = "vgui/stargate/hud/tac_red";
	else if ( m_iInfoMode == 2 ) mat = "vgui/stargate/hud/tac_yellow";
	DrawSpr( mat, (int)( 115 * s ), sh - (int)( 76 * s ), 64, 8, Color( 200, 200, 200, 255 ) );
}

void CHudStargate::DrawTickets( int sw, int sh, float s )
{
	if ( !m_bTicketMode )
		return;
	DrawSpr( "vgui/stargate/hud/ticketdock", (int)( 120 * s ), sh - (int)( 37 * s ), 120, 64, Color( 200, 200, 200, 255 ) );
	DrawHudNumber( m_iGtickets, (int)( 70 * s ), sh - (int)( 37 * s ), s, Color( 210, 0, 0, 255 ) );
	DrawHudNumber( m_iTtickets, (int)( 130 * s ), sh - (int)( 37 * s ), s, Color( 0, 180, 0, 255 ) );
}

void CHudStargate::DrawRoundTimer( int sw, int sh, float s )
{
	if ( m_flRoundLen <= 0 )
		return;
	int remaining = (int)( m_flRoundLen - ( gpGlobals->curtime - m_flRoundStart ) );
	if ( remaining < 0 ) remaining = 0;
	int mins = remaining / 60, secs = remaining % 60;
	const Color cyan( 168, 211, 255, 255 );
	float cx = sw / 2 - (int)( 28 * s );
	float cy = (int)( sh / 10 );
	DrawHudNumber( mins, cx, cy, s, cyan );
	DrawSpr( "vgui/stargate/hud/dots", cx + (int)( SG_NUM_W * 2 * s ), cy, 8, 24, cyan );
	// seconds, zero-padded
	char sb[ 8 ]; Q_snprintf( sb, sizeof( sb ), "%02d", secs );
	float dx = cx + (int)( ( SG_NUM_W * 2 + 8 ) * s );
	for ( const char *p = sb; *p; ++p )
	{
		DrawSpr( SG_NUM[ *p - '0' ], dx, cy, SG_NUM_W, SG_NUM_H, cyan );
		dx += SG_NUM_W * s;
	}
}

void CHudStargate::DrawBombKey( int sw, int sh, float s )
{
	if ( !m_bHasKey )
		return;
	DrawSpr( "vgui/stargate/hud/key", (int)( 75 * s ), sh - (int)( 52 * s ), 72, 48, Color( 200, 200, 200, 255 ) );
}

void CHudStargate::DrawPortraits( int sw, int sh, float s )
{
	// Bottom-right teammate avatar column (Portrait_middel cells, 80px wide).
	int n = 0;
	for ( int i = 0; i < SG_MAX_PORTRAITS; i++ )
	{
		if ( !m_Portraits[ i ].used )
			continue;
		int px = sw - (int)( ( ( n + 1 ) * 80 + 16 ) * s );
		int py = sh - (int)( 120 * s );
		DrawSpr( "vgui/stargate/hud/Portrait_middel", px, py, 80, 104, TeamColor( m_Portraits[ i ].team ) );
		if ( m_Portraits[ i ].avatar[ 0 ] )
		{
			char av[ 96 ];
			Q_snprintf( av, sizeof( av ), "vgui/stargate/avatars/%s", m_Portraits[ i ].avatar );
			DrawSpr( av, px + (int)( 8 * s ), py + (int)( 8 * s ), 64, 64, Color( 255, 255, 255, 255 ) );
		}
		n++;
	}
}

void CHudStargate::DrawZatOverlay( int sw, int sh, float s )
{
	if ( !m_bZat )
		return;
	float age = gpGlobals->curtime - m_flZatTime;
	if ( age > 1.5f ) { m_bZat = false; return; }
	int a = (int)( 200 * ( 1.0f - age / 1.5f ) );
	surface()->DrawSetColor( Color( 0, 130, 255, a ) );
	surface()->DrawSetTexture( GetTex( "vgui/stargate/weapons/d_zap" ) );
	surface()->DrawTexturedRect( 0, 0, sw, sh );
}

//-----------------------------------------------------------------------------
// message handlers
//-----------------------------------------------------------------------------
void CHudStargate::MsgFunc_SgArmor( bf_read &msg )      { m_flArmor = msg.ReadShort(); }
void CHudStargate::MsgFunc_SgStamina( bf_read &msg )    { m_flStamina = msg.ReadShort(); }
void CHudStargate::MsgFunc_SgSubModel( bf_read &msg )   { m_iSubModel = msg.ReadShort(); }
void CHudStargate::MsgFunc_SgHasKey( bf_read &msg )     { m_bHasKey = msg.ReadShort() != 0; }
void CHudStargate::MsgFunc_SgHideHud( bf_read &msg )    { m_iHideBits = msg.ReadShort(); }
void CHudStargate::MsgFunc_SgHudInfoMode( bf_read &msg ){ m_iInfoMode = msg.ReadByte(); }
void CHudStargate::MsgFunc_SgTicketMode( bf_read &msg ) { m_bTicketMode = msg.ReadByte() != 0; }
void CHudStargate::MsgFunc_SgGtickets( bf_read &msg )   { m_iGtickets = msg.ReadByte(); }
void CHudStargate::MsgFunc_SgTtickets( bf_read &msg )   { m_iTtickets = msg.ReadByte(); }
void CHudStargate::MsgFunc_SgTogglePor( bf_read &msg )  { sg_view_portraits.SetValue( !sg_view_portraits.GetBool() ); }
void CHudStargate::MsgFunc_SgNewRnd( bf_read &msg )     { m_iSubModel = 0; Q_memset( m_Portraits, 0, sizeof( m_Portraits ) ); }
void CHudStargate::MsgFunc_SgRadio( bf_read &msg )      { msg.ReadByte(); msg.ReadByte(); /* TODO radio text */ }

void CHudStargate::MsgFunc_SgAboutMe( bf_read &msg )
{
	m_iLocalId = msg.ReadShort();
	m_iLocalTeam = msg.ReadShort();
}

void CHudStargate::MsgFunc_SgSetRndTim( bf_read &msg )
{
	m_flRoundLen = msg.ReadShort();
	msg.ReadByte();
	m_flRoundStart = gpGlobals->curtime;
}

void CHudStargate::MsgFunc_SgZatTouch( bf_read &msg )
{
	m_bZat = true;
	m_flZatTime = gpGlobals->curtime;
}

void CHudStargate::MsgFunc_SgProgress( bf_read &msg )
{
	m_iProgress = msg.ReadByte();
	// remaining payload (flags + 28-entry array + trailer) consumed but unused.
	int remaining = msg.GetNumBytesLeft();
	for ( int i = 0; i < remaining; i++ )
		msg.ReadByte();
}

void CHudStargate::MsgFunc_SgBlip( bf_read &msg )
{
	int cmd = msg.ReadByte();
	float x = msg.ReadShort();
	float y = msg.ReadShort();
	if ( cmd == '=' )       // 0x3d - clear origin reference
		return;
	// add a blip into the first free/decayed slot
	for ( int i = 0; i < SG_MAX_BLIPS; i++ )
	{
		if ( m_Blips[ i ].alpha <= 0 )
		{
			m_Blips[ i ].type = ( cmd == '2' ) ? 1 : 0;
			m_Blips[ i ].x = x;
			m_Blips[ i ].y = y;
			m_Blips[ i ].alpha = 255;
			return;
		}
	}
}

void CHudStargate::MsgFunc_SgReset( bf_read &msg )
{
	int id = msg.ReadShort();
	int flag = msg.ReadByte();
	if ( flag == 0 )
	{
		Q_memset( m_Portraits, 0, sizeof( m_Portraits ) );
		return;
	}
	for ( int i = 0; i < SG_MAX_PORTRAITS; i++ )
		if ( m_Portraits[ i ].used && m_Portraits[ i ].id == id )
			m_Portraits[ i ].used = false;
}

void CHudStargate::MsgFunc_SgAddPortrait( bf_read &msg )
{
	int id = msg.ReadShort();
	int team = msg.ReadShort();
	char avatar[ 32 ];
	msg.ReadString( avatar, sizeof( avatar ) );
	// find existing or free slot
	int slot = -1;
	for ( int i = 0; i < SG_MAX_PORTRAITS; i++ )
	{
		if ( m_Portraits[ i ].used && m_Portraits[ i ].id == id ) { slot = i; break; }
		if ( slot < 0 && !m_Portraits[ i ].used ) slot = i;
	}
	if ( slot < 0 )
		return;
	m_Portraits[ slot ].used = true;
	m_Portraits[ slot ].id = id;
	m_Portraits[ slot ].team = team;
	Q_strncpy( m_Portraits[ slot ].avatar, avatar, sizeof( m_Portraits[ slot ].avatar ) );
}
