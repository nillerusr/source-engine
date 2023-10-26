#include "cbase.h"
#include "hud.h"
#include "asw_hud_crosshair.h"
#include "iclientmode.h"
#include "view.h"
#include "vgui_controls/controls.h"
#include "vgui/ISurface.h"
#include "vgui/Cursor.h"
#include "IVRenderView.h"
#include "iinput.h"	  // asw
#include "asw_input.h"
#include "c_asw_player.h"
#include "c_asw_marine.h"
#include "c_asw_weapon.h"
#include "asw_hud_minimap.h" // asw
#include "tier0/vprof.h"
#include <vgui/ILocalize.h>
#include "vguimatsurface/imatsystemsurface.h"
#include <vgui/ISurface.h>
#include "idebugoverlaypanel.h"
#include "engine/IVDebugOverlay.h"
#include "asw_remote_turret_shared.h"
#include "iasw_client_aim_target.h"
#include "c_asw_alien.h"
#include "fmtstr.h"
#include "cdll_bounded_cvars.h"
#include "materialsystem/imaterialvar.h"
#include "asw_vgui_hack_wire_tile.h"
#include "asw_vgui_computer_tumbler_hack.h"
#include "asw_vgui_computer_frame.h"
#include "asw_vgui_computer_menu.h"
#include "vgui/radialmenu.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar asw_crosshair_use_perspective("asw_crosshair_use_perspective", "1", FCVAR_CHEAT, "Show the crosshair that has perspective or the old version?");
extern ConVar cl_observercrosshair;
extern ConVar asw_crosshair_progress_size;
extern ConVar asw_fast_reload_enabled;


using namespace vgui;

int ScreenTransform( const Vector& point, Vector& screen );

ConVar crosshair( "crosshair", "1", FCVAR_ARCHIVE );

DECLARE_HUDELEMENT( CASWHudCrosshair );

CASWHudCrosshair::CASWHudCrosshair( const char *pElementName ) :
  CASW_HudElement( pElementName ), BaseClass( NULL, "ASWHudCrosshair" )
{
	vgui::Panel *pParent = GetClientMode()->GetViewport();
	SetParent( pParent );

	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
	SetScheme(scheme);

	m_pAmmoProgress = new vgui::ASWCircularProgressBar( this, "ASWCircularAmmoProgress" );
	m_pFastReloadBar = new vgui::ASWCircularProgressBar( this, "ASWFastReloadBar" );

	m_iShowGiveAmmoType = -1;
	m_bShowGiveAmmo = m_bShowGiveHealth = false;
	m_bIsReloading = false;
	m_clrCrosshair = Color( 255, 255, 255, 255 );

	m_vecCrossHairOffsetAngle.Init();

	SetHiddenBits( HIDEHUD_CROSSHAIR );

	// asw - make sure crosshair is above all the other HUD elements
	SetZPos(5);

	m_nCrosshairTexture = -1;
	m_nMinimapDrawCrosshairTexture = -1;

	m_pTurretTextTopLeft = new vgui::Label( this, "TurretTextTopLeft", "#asw_turret_text_top_left");
	m_pTurretTextTopLeft->SetContentAlignment( vgui::Label::a_northwest );
	m_pTurretTextTopRight = new vgui::Label( this, "TurretTextTopRight", "#asw_turret_text_top_right");
	m_pTurretTextTopRight->SetContentAlignment( vgui::Label::a_northeast );
	m_pTurretTextTopLeftGlow = new vgui::Label( this, "TurretTextTopLeftGlow", "#asw_turret_text_top_left");
	m_pTurretTextTopLeftGlow->SetContentAlignment( vgui::Label::a_northwest );
	m_pTurretTextTopRightGlow = new vgui::Label( this, "TurretTextTopRightGlow", "#asw_turret_text_top_right");
	m_pTurretTextTopRightGlow->SetContentAlignment( vgui::Label::a_northeast );
	
}

void CASWHudCrosshair::PerformLayout()
{
	BaseClass::PerformLayout();

	m_pTurretTextTopLeft->SetBounds( YRES( 5 ), YRES( 5 ), YRES( 200 ), YRES( 100 ) );
	m_pTurretTextTopRight->SetBounds( ScreenWidth() - YRES( 205 ), YRES( 5 ), YRES( 200 ), YRES( 100 ) );
	m_pTurretTextTopLeftGlow->SetBounds( YRES( 5 ), YRES( 5 ), YRES( 200 ), YRES( 100 ) );
	m_pTurretTextTopRightGlow->SetBounds( ScreenWidth() - YRES( 205 ), YRES( 5 ), YRES( 200 ), YRES( 100 ) );
}

void CASWHudCrosshair::ApplySchemeSettings( IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	SetPaintBackgroundEnabled( false );

	m_pAmmoProgress->SetVisible( false );
	m_pAmmoProgress->SetBounds( 0, 0, ScreenWidth(), ScreenWidth() );
	m_pAmmoProgress->SetFgImage( "hud/AmmoProgressCircle" );
	m_pAmmoProgress->SetIsOnCursor( true );

	m_pFastReloadBar->SetVisible( false );
	m_pFastReloadBar->SetBounds( 0, 0, ScreenWidth(), ScreenWidth() );
	m_pFastReloadBar->SetFgImage( "hud/AmmoProgressCircle" );
	m_pFastReloadBar->SetIsOnCursor( true );

	m_pTurretTextTopLeft->SetFont( scheme->GetFont( "Default", true ) );
	m_pTurretTextTopLeft->SetFgColor( Color( 255, 255, 255, 255 ) );
	m_pTurretTextTopLeft->SetPaintBackgroundEnabled( false );
	m_pTurretTextTopRight->SetFont( scheme->GetFont( "Default", true ) );
	m_pTurretTextTopRight->SetFgColor( Color( 255, 255, 255, 255 ) );
	m_pTurretTextTopRight->SetPaintBackgroundEnabled( false );

	m_pTurretTextTopLeftGlow->SetFont( scheme->GetFont( "DefaultBlur", true ) );
	m_pTurretTextTopLeftGlow->SetFgColor(scheme->GetColor("LightBlue", Color(128,128,128,255)));
	m_pTurretTextTopLeftGlow->SetPaintBackgroundEnabled( false );
	m_pTurretTextTopRightGlow->SetFont( scheme->GetFont( "DefaultBlur", true ) );
	m_pTurretTextTopLeftGlow->SetFgColor(scheme->GetColor("LightBlue", Color(128,128,128,255)));
	m_pTurretTextTopRightGlow->SetPaintBackgroundEnabled( false );
}

void CASWHudCrosshair::Paint( void )
{
	VPROF_BUDGET( "CASWHudCrosshair::Paint", VPROF_BUDGETGROUP_ASW_CLIENT );
	if ( !crosshair.GetInt() )
		return;

	if ( engine->IsDrawingLoadingImage() || engine->IsPaused() || !engine->IsActiveApp() )
		return;
	
	C_ASW_Player* pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( !pPlayer )
		return;
/*
	// draw a crosshair only if alive or spectating in eye
	bool shouldDraw = false;
	if ( pPlayer->IsAlive() )
		shouldDraw = true;

	if ( pPlayer->GetObserverMode() == OBS_MODE_IN_EYE )
		shouldDraw = true;

	if ( pPlayer->GetObserverMode() == OBS_MODE_ROAMING && cl_observercrosshair.GetBool() )
		shouldDraw = true;

    shouldDraw = true; // asw: always draw a crosshair
	if ( !shouldDraw )
		return;
*/
	if ( pPlayer->GetFlags() & FL_FROZEN )
		return;

	if ( pPlayer->entindex() != render->GetViewEntity() )
		return;

	m_curViewAngles = CurrentViewAngles();
	m_curViewOrigin = CurrentViewOrigin();

	bool bControllingTurret = (pPlayer->GetMarine() && pPlayer->GetMarine()->IsControllingTurret());	
	if ( bControllingTurret )
	{
		PaintTurretTextures();
		return;	// don't draw the normal cross hair in addition
	}

	int x, y;
	GetCurrentPos( x, y );

	int nCrosshair = GetCurrentCrosshair( x, y );

	if ( nCrosshair == m_nCrosshairTexture )
	{
		if ( pPlayer->IsSniperScopeActive() )
		{
			DrawSniperScope( x, y );
		}
		else
		{
			DrawDirectionalCrosshair( x, y, YRES( asw_crosshair_progress_size.GetInt() ) );
		}
	}
	else if ( nCrosshair != -1 )
	{
		const float fCrosshairScale = 1.0f;
		int w = YRES( 20 ) * fCrosshairScale;
		int h = YRES( 20 ) * fCrosshairScale;
		surface()->DrawSetColor( m_clrCrosshair );
		surface()->DrawSetTexture( nCrosshair );
		surface()->DrawTexturedRect( x - w, y - h, x + w, y + h );
	}

	// icons attached to the cursor
	x += 32; y += 16;	// move them down and to the right a bit
	if ( m_bShowGiveAmmo )
	{
		// todo: this text should in the ammo report tooltip?
		const wchar_t *ammoName = g_pVGuiLocalize->Find(GetAmmoName(m_iShowGiveAmmoType));
		DrawAttachedIcon(m_nGiveAmmoTexture, x, y, ammoName);
	}
	else if (m_bShowGiveHealth)
	{
		DrawAttachedIcon(m_nGiveHealthTexture, x, y);
	}
}

void CASWHudCrosshair::DrawAttachedIcon( int iTexture, int &x, int &y, const wchar_t *text )
{
	if ( iTexture == -1 )
	{
		return;
	}

	surface()->DrawSetColor(Color(255,255,255,255));
	surface()->DrawSetTexture(iTexture);
	int w = 32;
	int h = 32;
	surface()->DrawTexturedRect( x - w, y - h, x + w, y + h );	
	x += w;
	if (text != 0)
	{
		char ansi[64];
		g_pVGuiLocalize->ConvertUnicodeToANSI(text, ansi, sizeof(ansi));
		g_pMatSystemSurface->DrawColoredText( m_hGiveAmmoFont, x, y, 255, 255, 255, 255, ansi );
		//surface()->DrawColoredText( m_hGiveAmmoFont, x, y, Color(255,255,255,255), text );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASWHudCrosshair::SetCrosshairAngle( const QAngle& angle )
{
	VectorCopy( angle, m_vecCrossHairOffsetAngle );
}

// find localized name for this ammo type - this must be kept in sync with the ammodefs in gamerules!!!
const char* CASWHudCrosshair::GetAmmoName(int iAmmoType)
{
	//Msg("Finding ammo name for ammo type %d\n", iAmmoType);
	switch (iAmmoType)
	{
		case 2: return "#asw_give_rifle_ammo"; break;
		case 4: return "#asw_give_autogun_ammo"; break;
		case 5: return "#asw_give_shotgun_ammo"; break;
		case 6: return "#asw_give_vindicator_ammo"; break;
		case 7: return "#asw_give_flamer_ammo"; break;
		case 8: return "#asw_give_pistol_ammo"; break;
		case 10: return "#asw_give_railgun_ammo"; break;
		case 21: return "#asw_give_pdw_ammo"; break;
		case 0:
		default: Msg("unknown ammo type: %d\n", iAmmoType); return "#asw_unknown_ammo_type";	break;	
	}
	return "#asw_unknown_ammo_type";
}

extern ConVar asw_weapon_max_shooting_distance;

void CASWHudCrosshair::PaintTurretTextures()
{
	// draw border in centre of screen
	//int h = ScreenHeight();
	//int w = h * 1.333333f;
	//int x = (ScreenWidth() * 0.5f) - (w * 0.5f);
	//int y = 0;

	surface()->DrawSetColor(Color(255,255,255,255));

	/*
	int tx = YRES( 5 );
	int ty = YRES( 5 );
	vgui::surface()->DrawSetTextFont( m_hTurretFont );
	vgui::surface()->DrawSetTextColor( 255, 255, 255, 200 );
	vgui::surface()->DrawSetTextPos( tx, ty );

	int nFontTall = vgui::surface()->GetFontTall( m_hTurretFont );

	wchar_t szconverted[ 1024 ];
	g_pVGuiLocalize->ConvertANSIToUnicode( "#asw_turret_text_top_left", szconverted, sizeof( szconverted )  );
	vgui::surface()->DrawPrintText( szconverted, wcslen( szconverted ) );

	ty = ScreenHeight() - nFontTall * 8 - YRES( 100 );
	vgui::surface()->DrawSetTextPos( tx, ty );
	g_pVGuiLocalize->ConvertANSIToUnicode( "#asw_turret_text_lower_left", szconverted, sizeof( szconverted )  );
	vgui::surface()->DrawPrintText( szconverted, wcslen( szconverted ) );
	*/

	// draw black boxes either side
	//if (m_nBlackBarTexture != -1)
	//{
		//surface()->DrawSetTexture(m_nBlackBarTexture);
		//surface()->DrawTexturedRect(0, y, x, y + h);
		//surface()->DrawTexturedRect(x + w, y, x, y + h);
	//}

	// draw fancy brackets over all turret targets
	float fScale = (ScreenHeight() / 768.0f);
	int iBSize = fScale * 32.0f;
	for (int i=0;i<ASW_MAX_TURRET_TARGETS;i++)
	{
		C_BaseEntity *pEnt = m_TurretTarget[i];
		if (!pEnt)
			continue;

		// distance of each bracket part from the ent
		float fDist = fScale * 200.0f * (1.0f - m_fTurretTargetLock[i]);
		
		Vector pos = (pEnt->WorldSpaceCenter() - pEnt->GetAbsOrigin()) + pEnt->GetRenderOrigin();
		Vector screenPos;
		debugoverlay->ScreenPosition( pos, screenPos );
		surface()->DrawSetColor(Color(255,255,255,255.0f * m_fTurretTargetLock[i] ));

		surface()->DrawSetTexture(m_nLeftBracketTexture);
		int bx = screenPos[0] - fDist;
		int by = screenPos[1] - fDist;
		surface()->DrawTexturedRect(bx - iBSize, by - iBSize, bx + iBSize, by + iBSize);

		surface()->DrawSetTexture(m_nRightBracketTexture);
		bx = screenPos[0] + fDist;
		by = screenPos[1] - fDist;
		surface()->DrawTexturedRect(bx - iBSize, by - iBSize, bx + iBSize, by + iBSize);

		surface()->DrawSetTexture(m_nLowerBracketTexture);
		bx = screenPos[0];
		by = screenPos[1] + fDist;
		surface()->DrawTexturedRect(bx - iBSize, by - iBSize, bx + iBSize, by + iBSize);
	}

	// draw crosshair
	C_ASW_Player* pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( pPlayer )
	{
		bool bControllingTurret = (pPlayer->GetMarine() && pPlayer->GetMarine()->IsControllingTurret());	
		if (bControllingTurret)
		{
			C_ASW_Remote_Turret *pTurret = pPlayer->GetMarine()->GetRemoteTurret();
			if (pTurret)
			{
				Vector vecWeaponSrc = pTurret->GetTurretMuzzlePosition();
				QAngle angFacing = pTurret->EyeAngles();
				Vector vecWeaponDir;
				AngleVectors(angFacing, &vecWeaponDir);

				// trace along until we hit something
				trace_t tr;
				UTIL_TraceLine(vecWeaponSrc, vecWeaponSrc + vecWeaponDir * 1200,	// fog range of the turret
					MASK_SHOT, pTurret, COLLISION_GROUP_NONE, &tr);
				//Msg("Tracing from %s ", VecToString(vecWeaponSrc));
				//Msg("at angle %s\n", VecToString(angFacing));
				
				Vector pos = tr.DidHit() ? tr.endpos : vecWeaponSrc + vecWeaponDir * 1200;
				Vector screenPos;
				debugoverlay->ScreenPosition( pos, screenPos );

				// paint a crosshair at that spot
				surface()->DrawSetColor(Color(255,255,255,255));
				surface()->DrawSetTexture(m_nTurretCrosshair);
				surface()->DrawTexturedRect(screenPos[0] - iBSize, screenPos[1] - iBSize,	// shift it up a bit to match where the gun actually fires (not sure why it's not matched already!)
											screenPos[0] + iBSize, screenPos[1] + iBSize);
			}
		}
	}

	// draw interlace/noise overlay
	if (m_nTurretTexture!=-1)
	{
		surface()->DrawSetColor(Color(255,255,255,255));
		surface()->DrawSetTexture(m_nTurretTexture);
		surface()->DrawTexturedRect(0, 0, ScreenWidth(), ScreenHeight());
	}
}

#define ASW_TURRET_TARGET_RANGE 800
ConVar asw_turret_dot("asw_turret_dot", "0.9", FCVAR_CHEAT, "Dot angle above which potential targets are shown in the remote turret view");

void CASWHudCrosshair::PaintReloadProgressBar( void )
{
	if ( !engine->IsActiveApp() )
	{
		m_pAmmoProgress->SetVisible( false );
		m_pFastReloadBar->SetVisible( false );
		return;
	}

	C_ASW_Player* pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( !pPlayer )
		return;

	C_ASW_Marine* pMarine = pPlayer->GetMarine();
	if ( !pMarine )
		return;

	C_ASW_Weapon* pWeapon = pMarine->GetActiveASWWeapon();
	if ( !pWeapon || !asw_crosshair_use_perspective.GetBool() )
	{
		m_pAmmoProgress->SetVisible( false );
		m_pFastReloadBar->SetVisible( false );
		return;
	}

	int x, y;
	GetCurrentPos( x, y );
	int nCrosshair = GetCurrentCrosshair( x, y );

	if ( pWeapon && pWeapon->IsReloading() )
	{	
		m_bIsReloading = true;

		float flProgress = 0.0;

		float fStart = pWeapon->m_fReloadStart;
		float fNext = pWeapon->m_flNextPrimaryAttack;
		float fTotalTime = fNext - fStart;
		if (fTotalTime <= 0)
			fTotalTime = 0.1f;

		// if we're in single player, the progress code in the weapon doesn't run on the client because we aren't predicting
		if ( !cl_predict->GetInt() )
			flProgress = RescaleProgessForArt( (gpGlobals->curtime - fStart) / fTotalTime );
		else
			flProgress = RescaleProgessForArt( pWeapon->m_fReloadProgress );

		if ((int(gpGlobals->curtime*10) % 2) == 0)
			m_pAmmoProgress->SetFgColor( Color( 215, 205, 80, 255) );
		else
			m_pAmmoProgress->SetFgColor( Color( 175, 80, 80, 255) );

		if ( asw_fast_reload_enabled.GetBool() )
		{
			m_pFastReloadBar->SetFgColor( Color( 235, 235, 235, 100) );
			m_pFastReloadBar->SetBgColor( Color( 0, 0, 0, 0 ) );
			// fractions of ammo wide for fast reload start/end
			float fFastStart = RescaleProgessForArt( (pWeapon->m_fFastReloadStart - fStart) / fTotalTime );
			float fFastEnd = RescaleProgessForArt( (pWeapon->m_fFastReloadEnd - fStart) / fTotalTime );
			m_pFastReloadBar->SetStartProgress( fFastStart );
			m_pFastReloadBar->SetProgress( fFastEnd );
			m_pFastReloadBar->SetVisible( true );
		}

		m_pAmmoProgress->SetProgress( flProgress );
		m_pAmmoProgress->SetAlpha( 255 );
		m_pAmmoProgress->SetBgColor( Color( 100, 100, 100, 160*m_pAmmoProgress->GetScale() ) );
		m_pAmmoProgress->SetVisible( true );
	}
	/*else if ( m_bIsReloading )
	{
		m_bIsReloading = false;
		m_pAmmoProgress->SetFgColor( Color( 200, 200, 200, 255 ) );
		m_pAmmoProgress->SetProgress( 1.0 );
		vgui::GetAnimationController()->RunAnimationCommand( m_pAmmoProgress, "alpha",	0,		0, 0.5f, vgui::AnimationController::INTERPOLATOR_ACCEL );
	}
	*/
	else
	{
		float flScale = m_pAmmoProgress->GetScale();
		m_bIsReloading = false;
		m_pAmmoProgress->SetFgColor( Color( 220, 220, 220, 140*flScale ) );
		m_pFastReloadBar->SetVisible( false );

		int	nClip1 = pWeapon->Clip1();
		if ( nClip1 < 0 )
		{
			m_pAmmoProgress->SetVisible( false );
			return;
		}

		float flProgress = RescaleProgessForArt( float(nClip1) / float(pWeapon->GetMaxClip1()) ); 

		m_pAmmoProgress->SetProgress( flProgress );
		if ( flProgress < 0.25 && (int(gpGlobals->curtime*10) % 2) == 0 )
		{
			m_pAmmoProgress->SetBgColor( Color( 130, 90, 50, 160*flScale ) );
		}
		else
		{
			m_pAmmoProgress->SetBgColor( Color( 80, 80, 80, 130*flScale ) );
		}

		m_pAmmoProgress->SetVisible( !pPlayer->IsSniperScopeActive() && nCrosshair == m_nCrosshairTexture );
	}
};

float CASWHudCrosshair::RescaleProgessForArt( float flProgress )
{
	float flModdedProgress = flProgress;
	flModdedProgress *= 0.748;
	flModdedProgress += 0.126;
	return flModdedProgress;
}

void CASWHudCrosshair::OnThink()
{
	C_ASW_Player* pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( !pPlayer )
		return;

	PaintReloadProgressBar();

	bool bControllingTurret = (pPlayer->GetMarine() && pPlayer->GetMarine()->IsControllingTurret());	
	if (!bControllingTurret)
		return;

	C_ASW_Remote_Turret *pTurret = pPlayer->GetMarine()->GetRemoteTurret();
	if (!pTurret)
		return;

	Vector vecWeaponPos = pTurret->GetAbsOrigin();
	QAngle angWeapon = pTurret->EyeAngles();
	Vector vecWeaponDir;
	AngleVectors(angWeapon, &vecWeaponDir);

	// increase lock time of all targets and discard out of range/facing ones
	for (int k=0;k<ASW_MAX_TURRET_TARGETS;k++)
	{
		C_BaseEntity *pEnt = m_TurretTarget[k].Get();
		if (pEnt)
		{
			// validate the target
			bool bValid = true;
			Vector diff = pEnt->GetAbsOrigin() - vecWeaponPos;
			if (diff.Length() > ASW_TURRET_TARGET_RANGE || !pEnt->ShouldDraw())				
			{
				bValid = false;
			}
			else
			{
				diff.NormalizeInPlace();
				float flDot = diff.Dot(vecWeaponDir);
				if (flDot < asw_turret_dot.GetFloat())
					bValid = false;
					
			}
			// fade it in or out appropriately
			if (bValid)
			{
				if (m_fTurretTargetLock[k] < 1.0f)
				{
					m_fTurretTargetLock[k] += gpGlobals->frametime * 2;
					if (m_fTurretTargetLock[k] > 1.0f)
						m_fTurretTargetLock[k] = 1.0f;
				}
			}
			else
			{
				if (m_fTurretTargetLock[k] <= 0)
				{
					m_TurretTarget[k] = NULL;
				}
				else
				{
					m_fTurretTargetLock[k] -= gpGlobals->frametime * 2;
					if (m_fTurretTargetLock[k] < 0.0f)
						m_fTurretTargetLock[k] = 0.0f;
				}
			}
		}
	}

	// check for adding new targets to the array
	for ( int i = 0; i < IASW_Client_Aim_Target::AutoList().Count(); i++ )
	{
		IASW_Client_Aim_Target *pAimTarget = static_cast< IASW_Client_Aim_Target* >( IASW_Client_Aim_Target::AutoList()[ i ] );
		C_BaseEntity *pEnt = pAimTarget->GetEntity();
		if (!pEnt || !pAimTarget->IsAimTarget() || !pEnt->ShouldDraw())
			continue;
		
		// check he's in range
		Vector vecAlienPos = pEnt->WorldSpaceCenter();
		float flAlienDist = vecAlienPos.DistTo(vecWeaponPos);		
		if (flAlienDist > ASW_TURRET_TARGET_RANGE)
			continue;

		// check it's in front of us
		Vector dir = vecAlienPos - vecWeaponPos;
		dir.NormalizeInPlace();
		if (dir.Dot(vecWeaponDir) <= asw_turret_dot.GetFloat())
			continue;

		// we've got a possible target, add him to the list if he's not in there already
		bool bAlreadyInArray = false;
		int iSpace = -1;
		for (int k=0;k<ASW_MAX_TURRET_TARGETS;k++)
		{
			if (m_TurretTarget[k].Get() == pEnt)
			{
				bAlreadyInArray = true;
				break;
			}
			if (iSpace == -1 && m_TurretTarget[k].Get() == NULL)
				iSpace = k;
		}

		if (bAlreadyInArray)
			continue;

		if (iSpace != -1)
		{
			m_TurretTarget[iSpace] = pEnt;
			m_fTurretTargetLock[iSpace] = 0;
		}
	}
}

void CASWHudCrosshair::DrawDirectionalCrosshair( int x, int y, int iSize )
{
	C_ASW_Player *local = C_ASW_Player::GetLocalASWPlayer();
	if (!local)
		return;	
	
	C_ASW_Marine *pMarine = local->GetMarine();
	if (!pMarine)
		return;

	Vector MarinePos = pMarine->GetRenderOrigin();
	Vector screenPos;
	debugoverlay->ScreenPosition( MarinePos + Vector( 0, 0, 45 ), screenPos );

	int cx, cy, dx, dy;
	ASWInput()->ASW_GetWindowCenter(cx, cy);
	dx = x - cx;
	dy = y - cy;
	float flDistance = FastSqrt( dx * dx + dy * dy );

	//CASWScriptCreatedItem *pItem = static_cast<CASWScriptCreatedItem*>( pMarine->GetActiveASWWeapon()->GetAttributeContainer()->GetItem() );
	//if ( !m_bIsCastingSuitAbility )//pItem && pItem->IsMeleeWeapon() &&
	//{
		// here is where we do the crosshair scaling
		int iMax = YRES(54);
		int iMin = YRES(16);
		// clamp the min and max distance the cursor is from the marine on screen
		float flSizeDist = clamp( flDistance, iMin, iMax );

		// this is the full percentage to scale by
		float flTemp = (flSizeDist - iMin) / ( iMax - iMin );
		
		float flMinTexScale = 0.8;
		float flMaxTexScale = 1.0;
		// this is the final percentage to scale by based on the clamping we want for the min and max texture scaling
		float flAdjust = flTemp * (flMaxTexScale - flMinTexScale) + flMinTexScale;

		iSize *= flAdjust;
		m_pAmmoProgress->SetScale( flAdjust );
	//}

	iSize *= 2;
	int iXHairHalfSize = iSize / 2;
	// draw a red pointing, potentially blinking, arrow
	//Vector vecFacing(screenPos.x - (ScreenWidth() * 0.5f), screenPos.y - (ScreenHeight() * 0.5f), 0);
	//float fFacingYaw = -UTIL_VecToYaw(vecFacing);

	Vector2D vXHairCenter( x , y );

	Vector vecCornerTL(-iXHairHalfSize, -iXHairHalfSize, 0);
	Vector vecCornerTR(iXHairHalfSize, -iXHairHalfSize, 0);
	Vector vecCornerBR(iXHairHalfSize, iXHairHalfSize, 0);
	Vector vecCornerBL(-iXHairHalfSize, iXHairHalfSize, 0);
	Vector vecCornerTL_rotated, vecCornerTR_rotated, vecCornerBL_rotated, vecCornerBR_rotated;	

	int current_posx = 0;
	int current_posy = 0;
	ASWInput()->GetSimulatedFullscreenMousePos( &current_posx, &current_posy );
	Vector vecFacing( 0, 0, 0 );
	vecFacing.x = screenPos.x - current_posx;
	vecFacing.y = screenPos.y - current_posy;
	float fFacingYaw = -UTIL_VecToYaw( vecFacing );

	/*
	if ( ASWInput()->ControllerModeActive() )
	{
		int current_posx = 0;
		int current_posy = 0;
		ASWInput()->GetSimulatedFullscreenMousePos( &current_posx, &current_posy );
		vecFacing.x = screenPos.x - current_posx;
		vecFacing.y = screenPos.y - current_posy;
		fFacingYaw = -UTIL_VecToYaw( vecFacing );
	}
	*/

	// rotate it by our facing yaw
	QAngle angFacing(0, -fFacingYaw + 90.0, 0);
	VectorRotate(vecCornerTL, angFacing, vecCornerTL_rotated);
	VectorRotate(vecCornerTR, angFacing, vecCornerTR_rotated);
	VectorRotate(vecCornerBR, angFacing, vecCornerBR_rotated);
	VectorRotate(vecCornerBL, angFacing, vecCornerBL_rotated);
	//


	float flAlpha = 245;

	// fade the crosshair out when it is very close to the marine
	float flAdjust2 = flDistance / YRES(28);
	flAdjust2 = clamp( flAdjust2, 0.1f, 1.0f );

	bool bShotWarn = false;
	Color colorCross = Color(255,255,255,flAlpha*flAdjust2);
	C_ASW_Weapon* pWeapon = pMarine->GetActiveASWWeapon();
	if ( pWeapon && pWeapon->IsOffensiveWeapon() )
	{
		C_BaseEntity *pEnt = pWeapon->GetLaserTargetEntity();
		if ( pEnt && pEnt->Classify() == CLASS_ASW_MARINE )
		{
			bShotWarn = true;
			colorCross = Color(255,0,0,flAlpha*flAdjust2);
		}
	}
	
	surface()->DrawSetColor( colorCross );
	
	if ( !asw_crosshair_use_perspective.GetBool() )
	{
		surface()->DrawSetTexture( m_nDirectCrosshairTexture2 );
	}
	else
	{
		if ( bShotWarn )
			surface()->DrawSetTexture( m_nDirectCrosshairTextureX );
		else
			surface()->DrawSetTexture( m_nDirectCrosshairTexture );
	}
	

	Vertex_t points[4] = 
	{ 
		Vertex_t( Vector2D(vXHairCenter.x + vecCornerTL_rotated.x, vXHairCenter.y + vecCornerTL_rotated.y), 
		Vector2D(0,0) ), 
		Vertex_t( Vector2D(vXHairCenter.x + vecCornerTR_rotated.x, vXHairCenter.y + vecCornerTR_rotated.y), 
		Vector2D(1,0) ), 
		Vertex_t( Vector2D(vXHairCenter.x + vecCornerBR_rotated.x, vXHairCenter.y + vecCornerBR_rotated.y), 
		Vector2D(1,1) ), 
		Vertex_t( Vector2D(vXHairCenter.x + vecCornerBL_rotated.x, vXHairCenter.y + vecCornerBL_rotated.y), 
		Vector2D(0,1) ) 
	}; 
	surface()->DrawTexturedPolygon( 4, points );

	// draw the center
	if ( !asw_crosshair_use_perspective.GetBool() )
	{
		surface()->DrawSetColor( colorCross );
		surface()->DrawSetTexture( m_nCrosshairTexture );
		surface()->DrawTexturedRect(vXHairCenter.x - iXHairHalfSize, vXHairCenter.y - iXHairHalfSize, vXHairCenter.x + iXHairHalfSize, vXHairCenter.y + iXHairHalfSize);
	}
}

void CASWHudCrosshair::GetCurrentPos( int &x, int &y )
{
	C_ASW_Player* pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( !pPlayer )
		return;

	bool bControllingTurret = (pPlayer->GetMarine() && pPlayer->GetMarine()->IsControllingTurret());	

	m_pTurretTextTopLeft->SetVisible( bControllingTurret );
	m_pTurretTextTopRight->SetVisible( bControllingTurret );
	m_pTurretTextTopLeftGlow->SetVisible( bControllingTurret );
	m_pTurretTextTopRightGlow->SetVisible( bControllingTurret );

	if (::input->CAM_IsThirdPerson() && !bControllingTurret)
	{
		int mx, my, ux, uy;
		mx = 1;
		my = 1;

		ASWInput()->GetSimulatedFullscreenMousePos(&mx, &my, &ux, &uy);

		x = mx;
		y = my;
	}
	else
	{
		x = ScreenWidth()/2;
		y = ScreenHeight()/2;

		if ( bControllingTurret )
		{
			PaintTurretTextures();
			return;	// don't draw the normal cross hair in addition
		}
	}
}

int CASWHudCrosshair::GetCurrentCrosshair( int x, int y )
{
	CRadialMenu *pRadialMenu = GET_HUDELEMENT( CRadialMenu );
	if ( !pRadialMenu->IsFading() && pRadialMenu->GetAlpha() > 0 )
	{
		return m_nHackActiveCrosshairTexture;
	}

	CASWHudMinimap *pMap = GET_HUDELEMENT( CASWHudMinimap );
	bool bMouseIsOverMinimap = ( pMap && pMap->UseDrawCrosshair( x, y ) );
	if ( bMouseIsOverMinimap )
	{
		return m_nMinimapDrawCrosshairTexture;
	}

	CASW_VGUI_Computer_Frame *pComputerFrame = dynamic_cast< CASW_VGUI_Computer_Frame* >( GetClientMode()->GetPanelFromViewport( "ComputerContainer/VGUIComputerFrame" ) );
	if ( pComputerFrame )
	{
		if ( pComputerFrame->m_pMenuPanel )
		{
			CASW_VGUI_Computer_Tumbler_Hack *pTumbler = dynamic_cast< CASW_VGUI_Computer_Tumbler_Hack* >( pComputerFrame->m_pMenuPanel->m_hCurrentPage.Get() );
			if ( pTumbler )
			{
				if ( pTumbler->m_iMouseOverTumblerControl != -1 )
				{
					return m_nHackActiveCrosshairTexture;
				}
			}
		}
	}
	else
	{
		CASW_VGUI_Hack_Wire_Tile *pWireTile = dynamic_cast< CASW_VGUI_Hack_Wire_Tile* >( GetClientMode()->GetPanelFromViewport( "WireTileContainer/HackWireTile" ) );
		if ( pWireTile )
		{
			if ( pWireTile->IsCursorOverWireTile( x, y ) )
			{
				return m_nHackActiveCrosshairTexture;
			}
		}
	}

	Panel *pPanel = GetClientMode()->GetPanelFromViewport( "ComputerContainer" );
	if ( !pPanel )
	{
		pPanel = GetClientMode()->GetPanelFromViewport( "WireTileContainer" );
	}

	if ( pPanel && pPanel->IsCursorOver() )
	{
		return m_nHackCrosshairTexture;
	}

	return m_nCrosshairTexture;
}

ConVar asw_sniper_scope_radius( "asw_sniper_scope_radius", "100" );

void CASWHudCrosshair::DrawSniperScope( int x, int y )
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( !pPlayer )
		return;	

	C_ASW_Marine *pMarine = pPlayer->GetMarine();
	if ( !pMarine )
		return;

	const int NUM_CIRCLE_POINTS = 40;
	static vgui::Vertex_t points[ NUM_CIRCLE_POINTS ];
	float width = YRES( asw_sniper_scope_radius.GetFloat() );
	float height = YRES( asw_sniper_scope_radius.GetFloat() );
	for ( int i = 0; i < NUM_CIRCLE_POINTS; i++ )
	{
		float flAngle = 2.0f * M_PI * ( (float) i / (float) NUM_CIRCLE_POINTS );
		points[ i ].Init( Vector2D( x + width * cos( flAngle ), y + height * sin( flAngle ) ), Vector2D( 0.5f + 0.5f * cos( flAngle ), 0.5f + 0.5f * sin( flAngle ) ) );
	}
	surface()->DrawSetColor( Color(255,255,255,255) );
	surface()->DrawSetTexture( m_nSniperMagnifyTexture );
	IMaterial *pMaterial = materials->FindMaterial( "effects/magnifyinglens", TEXTURE_GROUP_OTHER );
	IMaterialVar *pMagnificationCenterVar = pMaterial->FindVar( "$magnifyCenter", NULL );
	
	float flCenterX = ( ( float )x / ( float )ScreenWidth() ) - 0.5f;
	float flCenterY = ( ( float )y / ( float )ScreenHeight() ) - 0.5f;
	pMagnificationCenterVar->SetVecValue( flCenterX, flCenterY, 0, 0 );	

	vgui::surface()->DrawTexturedPolygon( NUM_CIRCLE_POINTS, points );
}