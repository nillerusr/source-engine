//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "soundenvelope.h"
#include "Sprite.h"
#include "physics.h"
#include "c_asw_tesla_trap.h"
#include "particle_parse.h"
#include "asw_gamerules.h"
#include "asw_util_shared.h"
#include <vgui/ISurface.h>
#include <vgui_controls/Panel.h>
#include <vgui/ILocalize.h>
#include "c_asw_sentry_base.h"
#include "asw_hud_use_icon.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

enum
{
	TESLATRAP_STATE_DORMANT = 0,
	TESLATRAP_STATE_DEPLOY,		// Try to lock down and arm
	TESLATRAP_STATE_CHARGING,		// Held in the physgun
	TESLATRAP_STATE_CHARGED,		// Locked down and looking for targets
};

IMPLEMENT_CLIENTCLASS_DT( C_ASW_TeslaTrap, DT_ASW_TeslaTrap, CASW_TeslaTrap )
	RecvPropInt( RECVINFO( m_iTrapState )),
	RecvPropInt( RECVINFO( m_iAmmo )),
	RecvPropInt( RECVINFO( m_iMaxAmmo )),
	RecvPropFloat( RECVINFO( m_flRadius )),
	RecvPropFloat( RECVINFO( m_flDamage )),
	RecvPropFloat( RECVINFO( m_flNextFullChargeTime )),
	RecvPropBool(RECVINFO ( m_bAssembled )),
	
END_NETWORK_TABLE()
BEGIN_DATADESC( C_ASW_TeslaTrap )
	DEFINE_FIELD( m_iTrapState, FIELD_INTEGER ),
	DEFINE_FIELD( m_flRadius, FIELD_FLOAT ),
	DEFINE_FIELD( m_flDamage, FIELD_FLOAT ),
	DEFINE_FIELD( m_flNextFullChargeTime, FIELD_FLOAT ),
END_DATADESC()

bool C_ASW_TeslaTrap::s_bLoadedTeslaIcon = false;
int  C_ASW_TeslaTrap::s_nTeslaIconTextureID = -1;

C_ASW_TeslaTrap::C_ASW_TeslaTrap()
{
	//SetPredictionEligible( true );
	m_iTrapState = 0;
	m_flRadius = 120;
	m_flDamage = 140;
	m_flNextFullChargeTime = 0;
}


C_ASW_TeslaTrap::~C_ASW_TeslaTrap()
{
	if ( m_hEffects )
	{
		ParticleProp()->StopEmissionAndDestroyImmediately( m_hEffects );
		m_hEffects = NULL;
	}
}

void C_ASW_TeslaTrap::OnDataChanged( DataUpdateType_t type )
{	
	/*
	bool bPredict = ShouldPredict();
	if (bPredict)
	{
		SetPredictionEligible( true );
	}
	else
	{
		SetPredictionEligible( false );
	}
*/
	BaseClass::OnDataChanged( type );

	if ( type == DATA_UPDATE_CREATED )
	{
		// We want to think every frame.
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}

	UpdateEffects();
}

void C_ASW_TeslaTrap::ClientThink()
{
	if ( m_hEffects && m_iTrapState == TESLATRAP_STATE_CHARGING )
	{
		// TODO: fix this hardcoded recharge time limit
		// TODO: return this from a function so we don't duplicate in UpdateEffects
		float flAmount = 1.0f - ((m_flNextFullChargeTime - gpGlobals->curtime) / (3.0f * 1.5f) );
		m_hEffects->SetControlPoint( 1, Vector( flAmount, flAmount, 0 ) );
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
void C_ASW_TeslaTrap::UpdateEffects( void )
{
	//m_iTrapState = iState;
	if ( !m_hEffects && m_bAssembled )
	{
		Vector vecOrigin;
		QAngle vecAngles;
		// Get the muzzle origin
		if ( !GetAttachment( "effects", vecOrigin, vecAngles ) )
		{
			vecOrigin = GetAbsOrigin();
			vecAngles = QAngle( 0, 0, 0 );
		}

		m_hEffects = ParticleProp()->Create( "tesla_trap_fx", PATTACH_ABSORIGIN_FOLLOW, -1, vecOrigin - GetAbsOrigin() );

		if ( m_hEffects )
		{
			//m_hEffects->SetSortOrigin( vecOrigin );
			//m_hEffects->SetControlPoint( 0, vecOrigin );
			Vector vecForward, vecRight, vecUp;
			AngleVectors( vecAngles, &vecForward, &vecRight, &vecUp );
			m_hEffects->SetControlPointOrientation( 0, vecForward, vecRight, vecUp );
		}
	}
	
	if ( m_hEffects )
	{
		switch( m_iTrapState )
		{
		case TESLATRAP_STATE_DORMANT:
			// off
			m_hEffects->SetControlPoint( 1, Vector( 0, 0, 0 ) );
			//UpdateLight( false, 0, 0, 0, 0 );
			break;

		case TESLATRAP_STATE_DEPLOY:
			// setting up (blue, no ring)
			m_hEffects->SetControlPoint( 1, Vector( 0, 0, 0 ) );
			//UpdateLight( true, 0, 0, 255, 190 );
			break;

		case TESLATRAP_STATE_CHARGING:
			{
				// searching (green glow, red ring)
				// TODO: fix this hardcoded recharge time limit
				// 3 seconds hardcoded charge time, plus extra time so the scale pops to full from charging so the state is more obvious
				float flAmount = 1.0f - ((m_flNextFullChargeTime - gpGlobals->curtime) / (3.0f * 1.5f) );
				m_hEffects->SetControlPoint( 1, Vector( flAmount, flAmount, 0 ) );
				//UpdateLight( false, 0, 255, 0, 255 );
				break;
			}

		case TESLATRAP_STATE_CHARGED:
			// found poetntial enemy (red glow, red ring)
			m_hEffects->SetControlPoint( 1, Vector( 1, 1, 0 ) );
			//UpdateLight( false, 255, 0, 0, 255 );
			break;

		default:
			DevMsg("**Unknown Trap State: %d\n", m_iTrapState );
			break;
		}
	}
}



int C_ASW_TeslaTrap::GetTeslaIconTextureID()
{
	if ( !s_bLoadedTeslaIcon )
	{
		// load the textures
		s_nTeslaIconTextureID = vgui::surface()->CreateNewTextureID();		
		vgui::surface()->DrawSetTextureFile( s_nTeslaIconTextureID, "vgui/swarm/EquipIcons/EquipTeslaTrap", true, false);
		s_bLoadedTeslaIcon = true;
	}

	return s_nTeslaIconTextureID;
}

bool C_ASW_TeslaTrap::IsUsable(C_BaseEntity *pUser)
{
	return (pUser && pUser->GetAbsOrigin().DistTo(GetAbsOrigin()) < ASW_MARINE_USE_RADIUS);	// near enough?
}

bool C_ASW_TeslaTrap::GetUseAction(ASWUseAction &action, C_ASW_Marine *pUser)
{
	action.iUseIconTexture = GetTeslaIconTextureID();
	TryLocalize( "#asw_tesla_ammo", action.wszText, sizeof( action.wszText ) );
	action.UseTarget = this;
	action.fProgress = -1;
	action.UseIconRed = 66;
	action.UseIconGreen = 142;
	action.UseIconBlue = 192;
	action.bShowUseKey = false;
	action.iInventorySlot = -1;
	return true;
}

void C_ASW_TeslaTrap::CustomPaint(int ix, int iy, int alpha, vgui::Panel *pUseIcon)
{
	if ( !m_bAssembled )
		return;

	if (C_ASW_Sentry_Base::s_hAmmoFont == vgui::INVALID_FONT)
	{
		vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
		vgui::IScheme *pScheme = vgui::scheme()->GetIScheme(scheme);
		if (pScheme)
			C_ASW_Sentry_Base::s_hAmmoFont = vgui::scheme()->GetIScheme(scheme)->GetFont("DefaultSmall", true);
	}

	if (C_ASW_Sentry_Base::s_hAmmoFont == vgui::INVALID_FONT || alpha <= 0)
		return;

	int nAmmo = MAX( 0, GetAmmo() );
	Color textColor( 255, 255, 255, 255 );
	if ( nAmmo < 10 )
	{
		textColor = Color( 255, 255, 0, 255 );
	}

	if ( pUseIcon )
	{
		CASW_HUD_Use_Icon *pUseIconPanel = static_cast<CASW_HUD_Use_Icon*>(pUseIcon);
		float flProgress = (float) nAmmo / (float) GetMaxAmmo();
		char szCountText[64];
		Q_snprintf( szCountText, sizeof( szCountText ), "%d", MAX( nAmmo, 0 ) );
		pUseIconPanel->CustomPaintProgressBar( ix, iy, alpha / 255.0f, flProgress, szCountText, C_ASW_Sentry_Base::s_hAmmoFont, textColor, "#asw_ammo_label" );
	}
}