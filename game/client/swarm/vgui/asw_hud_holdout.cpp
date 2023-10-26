#include "cbase.h"
#include "asw_hud_holdout.h"
#include "c_user_message_register.h"
#include "clientmode_asw.h"
#include "holdout_hud_wave_panel.h"
#include "holdout_wave_announce_panel.h"
#include "holdout_wave_end_panel.h"
#include "holdout_resupply_frame.h"
#include "asw_holdout_mode.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// TODO:
//		A panel showing wave status + score
//				Wave number/name
//				Progress bar for aliens left to kill ( pWave->GetTotalAliens() - ASWHoldoutMode()->GetAliensKilledThisWave() )
//				Score?
//				'+100' scrolling up each time a kill is made
//
//		A panel showing end of wave summary
//				'Wave X complete' message
//				Kills per player
//				XP earned per player?
//				Score gained that round
//
//		Spawning a resupply frame
//				Resupply header
//				Timer
//				Button to leave resupply early
//				loadoutpanel stuff
//				Code for when you pick equipment to have the server switch out weapons.  If you leave resupply without changing, then refill ammo.

extern ConVar asw_draw_hud;

DECLARE_HUDELEMENT( CASW_Hud_Holdout );

CASW_Hud_Holdout::CASW_Hud_Holdout( const char *pElementName ) : CASW_HudElement( pElementName ), BaseClass( NULL, "ASW_Hud_Holdout" )
{
	SetParent( GetClientMode()->GetViewport() );
	SetScheme( vgui::scheme()->LoadSchemeFromFile( "resource/SwarmSchemeNew.res", "SwarmSchemeNew" ) );

	m_pWavePanel = new Holdout_Hud_Wave_Panel( this, "HoldoutHudWavePanel" );
}

CASW_Hud_Holdout::~CASW_Hud_Holdout()
{

}

void CASW_Hud_Holdout::Init()
{
	Reset();
}

void CASW_Hud_Holdout::Reset()
{	
}

void CASW_Hud_Holdout::VidInit()
{
	Reset();
}

void CASW_Hud_Holdout::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	vgui::Panel::ApplySchemeSettings(pScheme);
}

bool CASW_Hud_Holdout::ShouldDraw( void )
{
	if ( !ASWHoldoutMode() )
		return false;

	return asw_draw_hud.GetBool() && CASW_HudElement::ShouldDraw();
}

void CASW_Hud_Holdout::AnnounceNewWave( int nWave, float flDuration )
{
	// Spawn the panel announcing the new wave
	Holdout_Wave_Announce_Panel *pPanel = new Holdout_Wave_Announce_Panel( this, "HoldoutWaveAnnouncePanel" );
	pPanel->Init( nWave, flDuration );
}

void CASW_Hud_Holdout::ShowWaveScores( int nWave, float flDuration )
{
	// Spawn the panel showing scores for this wave
	Holdout_Wave_End_Panel *pPanel = new Holdout_Wave_End_Panel( this, "HoldoutWaveEndPanel" );
	pPanel->Init( nWave, flDuration );
}

void __MsgFunc_ASWNewHoldoutWave( bf_read &msg )
{
	int nWave = msg.ReadByte();
	float flDuration = msg.ReadFloat();

	CASW_Hud_Holdout *pHoldoutHud = GET_HUDELEMENT( CASW_Hud_Holdout );
	pHoldoutHud->AnnounceNewWave( nWave, flDuration );
}
USER_MESSAGE_REGISTER( ASWNewHoldoutWave );

void __MsgFunc_ASWShowHoldoutWaveEnd( bf_read &msg )
{
	int nWave = msg.ReadByte();
	float flDuration = msg.ReadFloat();

	CASW_Hud_Holdout *pHoldoutHud = GET_HUDELEMENT( CASW_Hud_Holdout );
	pHoldoutHud->ShowWaveScores( nWave, flDuration );
}
USER_MESSAGE_REGISTER( ASWShowHoldoutWaveEnd );

void __MsgFunc_ASWShowHoldoutResupply( bf_read &msg )
{
	new Holdout_Resupply_Frame( GetClientMode()->GetViewport(), "ResupplyFrame" );
}
USER_MESSAGE_REGISTER( ASWShowHoldoutResupply );

CON_COMMAND_F( asw_test_wave_announce, "Shows wave announce panel", FCVAR_CHEAT )
{
	CASW_Hud_Holdout *pHoldoutHud = GET_HUDELEMENT( CASW_Hud_Holdout );
	Holdout_Wave_Announce_Panel *pPanel = new Holdout_Wave_Announce_Panel( pHoldoutHud, "HoldoutWaveAnnouncePanel" );
	pPanel->Init( 0, 6.0f );
}