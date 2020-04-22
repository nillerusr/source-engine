//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "iefx.h"
#include "dlight.h"
#include "engine/IEngineSound.h"
#include "view.h"
#include "beamdraw.h"
#include "clienteffectprecachesystem.h"
#include "weapon_combat_usedwithshieldbase.h"
#include "c_weapon__stubs.h"
#include <vgui/ISurface.h>

#define BALL_GROW_TIME	1.5

// Precache the effects
CLIENTEFFECT_REGISTER_BEGIN( PrecacheWeaponCombat_ChargeablePlasma )
CLIENTEFFECT_MATERIAL( "sprites/chargeball_team1" )
CLIENTEFFECT_MATERIAL( "sprites/chargeball_team2" )
CLIENTEFFECT_REGISTER_END()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_WeaponCombat_ChargeablePlasma : public C_WeaponCombatUsedWithShieldBase
{
	DECLARE_CLASS( C_WeaponCombat_ChargeablePlasma, C_WeaponCombatUsedWithShieldBase );

public:
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	C_WeaponCombat_ChargeablePlasma( void );
	~C_WeaponCombat_ChargeablePlasma( void );

	virtual void	PreDataUpdate( DataUpdateType_t updateType );
	virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual bool	Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	virtual void	DrawCrosshair( void );
	virtual void	ClientThink( );
 	virtual int		DrawModel( int flags );
	virtual void	ViewModelDrawn( C_BaseViewModel *pBaseViewModel );
	virtual void	NotifyShouldTransmit( ShouldTransmitState_t state );
	virtual	bool	IsTransparent( );

private:
	void			StartCharging();
	void			StopCharging();
	void			DrawChargingEffect( float flSize, C_BaseAnimating *pAttachedEnt );

private:
	bool			m_bCharging;
	bool			m_bLastCharging;
	float			m_flPower;
	float			m_flChargeStartTime;
	CMaterialReference m_hMaterial;

private:
	C_WeaponCombat_ChargeablePlasma( const C_WeaponCombat_ChargeablePlasma & );
};

STUB_WEAPON_CLASS_IMPLEMENT( weapon_combat_chargeableplasma, C_WeaponCombat_ChargeablePlasma );

IMPLEMENT_CLIENTCLASS_DT( C_WeaponCombat_ChargeablePlasma, DT_WeaponCombat_ChargeablePlasma, CWeaponCombat_ChargeablePlasma )
	RecvPropInt( RECVINFO(m_bCharging) ),
END_RECV_TABLE()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_WeaponCombat_ChargeablePlasma::C_WeaponCombat_ChargeablePlasma( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_WeaponCombat_ChargeablePlasma::~C_WeaponCombat_ChargeablePlasma( void )
{
	Holster( NULL );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_WeaponCombat_ChargeablePlasma::PreDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PreDataUpdate( updateType );

	m_bLastCharging = m_bCharging;
}


void C_WeaponCombat_ChargeablePlasma::NotifyShouldTransmit( ShouldTransmitState_t state )
{
	BaseClass::NotifyShouldTransmit(state);

	if (state == SHOULDTRANSMIT_START)
	{
		if (m_bCharging)
			StartCharging();
	}
	else if (state == SHOULDTRANSMIT_END)
	{
		if (m_bCharging)
			StopCharging();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_WeaponCombat_ChargeablePlasma::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	switch( updateType )
	{
	case DATA_UPDATE_CREATED:
		// So we can update our lights
		if ( GetTeamNumber() == 1 )
			m_hMaterial.Init( "sprites/chargeball_team1" );
		else 
			m_hMaterial.Init( "sprites/chargeball_team2" );

		break;								

	case DATA_UPDATE_DATATABLE_CHANGED:
		if ( m_bCharging != m_bLastCharging )
		{
			if ( m_bCharging )
			{
				StartCharging();
			}
			else
			{
				StopCharging();
			}
		}
		break;
	};

	if (WeaponState() == WEAPON_IS_ACTIVE)
	{
		// Start thinking so we can manipulate the light
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
	else
	{
		SetNextClientThink( CLIENT_THINK_NEVER );
	}
}

  
//-----------------------------------------------------------------------------
// Deal with dynamic lighting
//-----------------------------------------------------------------------------
void C_WeaponCombat_ChargeablePlasma::ClientThink( )
{
	BaseClass::ClientThink();

	C_BaseTFPlayer *pPlayer = (C_BaseTFPlayer *)GetOwner();
	if ( !pPlayer || (pPlayer->GetHealth() <= 0))
	{
		SetNextClientThink( CLIENT_THINK_NEVER );
		return;
	}

	if (!m_bCharging)
		return;

	// Determine the ball size...
	m_flPower = (gpGlobals->curtime - m_flChargeStartTime) / BALL_GROW_TIME;
	m_flPower = clamp( m_flPower, 0, 1 );

	// FIXME: dl->origin should be based on the attachment point
	dlight_t *dl = effects->CL_AllocDlight( entindex() );
	dl->origin = GetRenderOrigin();

	if (GetTeamNumber() == 1)
	{
		dl->color.r = 40;
		dl->color.g = 60;
		dl->color.b = 250;
	}
	else
	{
		dl->color.r = 250;
		dl->color.g = 60;
		dl->color.b = 40;
	}

	dl->color.exponent = 5;
	dl->radius = 20 * m_flPower + 10;
	dl->die = gpGlobals->curtime + 0.01;
}


//-----------------------------------------------------------------------------
// Purpose: Remove the ball if we're switching away
//-----------------------------------------------------------------------------
bool C_WeaponCombat_ChargeablePlasma::Holster( C_BaseCombatWeapon *pSwitchingTo )
{
	StopCharging();

	return BaseClass::Holster( pSwitchingTo );
}

void C_WeaponCombat_ChargeablePlasma::StartCharging()
{
	CLocalPlayerFilter filter;
	EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, "WeaponCombat_ChargeablePlasma.Charging" );
	m_flChargeStartTime = gpGlobals->curtime;
}


void C_WeaponCombat_ChargeablePlasma::StopCharging()
{
	StopSound( SOUND_FROM_LOCAL_PLAYER, "WeaponCombat_ChargeablePlasma.Charging" );
	m_bCharging = false;
}


//-----------------------------------------------------------------------------
// We're transparent because we draw a transparent charging effect
//-----------------------------------------------------------------------------
bool C_WeaponCombat_ChargeablePlasma::IsTransparent( )
{
	if (m_bCharging)
		return true;
	return BaseClass::IsTransparent();
}


//-----------------------------------------------------------------------------
// Purpose: Draws the charging effect
//-----------------------------------------------------------------------------
void C_WeaponCombat_ChargeablePlasma::DrawChargingEffect( float flSize, C_BaseAnimating *pAttachedEnt )
{
	if (!pAttachedEnt)
		return;

	Vector vecOrigin;
	QAngle vecAngles;
	int iAttachment = pAttachedEnt->LookupAttachment( "muzzle" );
	if ( pAttachedEnt->GetAttachment( iAttachment, vecOrigin, vecAngles ) )
	{
		color32 color = { 255, 255, 255, 255 };
		materials->Bind( m_hMaterial, (IClientRenderable*)this );
		DrawSprite( vecOrigin, flSize, flSize, color );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Draws the model
//-----------------------------------------------------------------------------
int C_WeaponCombat_ChargeablePlasma::DrawModel( int flags )
{
	int retval = BaseClass::DrawModel( flags );
	if (retval == 0)
		return 0;

	if (m_bCharging && IsCarrierAlive())
	{
		// Draw the charging effect
		float flSize = 20 * m_flPower + 10;

		DrawChargingEffect( flSize, this );
	}
	return retval;
}


//-----------------------------------------------------------------------------
// Purpose: Draws the model
//-----------------------------------------------------------------------------
void C_WeaponCombat_ChargeablePlasma::ViewModelDrawn( C_BaseViewModel *pBaseViewModel )
{
	if (!m_bCharging)
		return;

	// Draw the charging effect
	float flSize = 12 * m_flPower + 6;

	if ( m_iClip1 > 0 )
	{
		DrawChargingEffect( flSize, pBaseViewModel );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Draw targeting reticle
//-----------------------------------------------------------------------------
void C_WeaponCombat_ChargeablePlasma::DrawCrosshair( void )
{
	BaseClass::DrawCrosshair();

	// Find enemy players in front of me
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	trace_t tr;
	Vector vecStart, vecEnd;
	VectorMA( CurrentViewOrigin(), 1500, CurrentViewForward(), vecEnd );
	VectorMA( CurrentViewOrigin(), 48, CurrentViewForward(), vecStart );
	UTIL_TraceLine( vecStart, vecEnd, MASK_SOLID, NULL, COLLISION_GROUP_NONE, &tr );

	if ( tr.DidHitNonWorldEntity() )
	{
		C_BaseEntity *pEntity = tr.m_pEnt;
		if ( pEntity && pEntity->IsPlayer() && !pPlayer->InSameTeam( pEntity ) )
		{
			// Draw a reticle
			vgui::Color clr = gHUD.m_clrYellowish;
			clr[3] = 128;

			// Calculate circle size
			int iRatio = 30;

			// Draw the circle 
			int iDegrees = 0;
			Vector vecPoint, vecLastPoint(0,0,0);
			vecPoint.z = 0.0f;
			for ( int i = 0; i < 360; i++ )
			{
				float flRadians = DEG2RAD( iDegrees );
				iDegrees += (360 / 360);

				float ca = cos( flRadians );
				float sa = sin( flRadians );
							 
				// Rotate it around the circle
				vecPoint.x = (int)((ScreenWidth() / 2) + (iRatio * sa));
				vecPoint.y = (int)((ScreenHeight() / 2) - (iRatio * ca));

				// Draw the point, if it's not on the previous point, to avoid smaller circles being brighter
				if ( vecLastPoint != vecPoint )
				{
					vgui::surface()->DrawSetColor( clr );
					vgui::surface()->DrawFilledRect( vecPoint.x, vecPoint.y, vecPoint.x + 1, vecPoint.y + 1 );
				}

				vecLastPoint = vecPoint;
			}
		}
	}
}

