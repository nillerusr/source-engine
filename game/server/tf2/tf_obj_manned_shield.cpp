//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A stationary shield that players can man
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "tf_obj_base_manned_gun.h"
#include "tf_shield.h"
#include "in_buttons.h"


// ------------------------------------------------------------------------ //
// A stationary gun that players can man that's built by the player
// ------------------------------------------------------------------------ //
class CObjectMannedShield : public CObjectBaseMannedGun
{
	DECLARE_CLASS( CObjectMannedShield, CObjectBaseMannedGun );

public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CObjectMannedShield();
	virtual ~CObjectMannedShield();

	static CObjectMannedShield* Create(const Vector &vOrigin, const QAngle &vAngles);

	virtual void Spawn();
	virtual void Precache();
	virtual void SetPassenger( int nRole, CBasePlayer *pEnt );

	virtual void PowerupStart( int iPowerup, float flAmount, CBaseEntity *pAttacker, CDamageModifier *pDamageModifier );
	virtual	void PowerupEnd( int iPowerup );

protected:

	virtual void OnItemPostFrame( CBaseTFPlayer *pDriver );


private:

	void ShieldRotationThink();

	CHandle<CShield> m_hDeployedShield;
};

#define MANNED_SHIELD_CLIP_COUNT			3

#define MANNED_SHIELD_MINS					Vector(-20, -20, 0)
#define MANNED_SHIELD_MAXS					Vector( 20,  20, 55)
#define MANNED_SHIELD_MODEL					"models/objects/obj_manned_plasmagun.mdl"


BEGIN_DATADESC( CObjectMannedShield )
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CObjectMannedShield, DT_ObjectMannedShield)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS(obj_manned_shield, CObjectMannedShield);
PRECACHE_REGISTER(obj_manned_shield);

// CVars
ConVar	obj_manned_shield_health( "obj_manned_shield_health","100", FCVAR_NONE, "Manned Missile Launcher health" );

const char *g_pMannedShieldThinkContextName = "MannedShieldThinkContext";


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CObjectMannedShield::CObjectMannedShield()
{
}

CObjectMannedShield::~CObjectMannedShield()
{
	UTIL_Remove( m_hDeployedShield );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectMannedShield::Spawn()
{
	m_iHealth = obj_manned_shield_health.GetInt();
	BaseClass::Spawn();

	SetMovementStyle( MOVEMENT_STYLE_SIMPLE );
	SetModel( MANNED_SHIELD_MODEL );
	OnModelSelected();
	SetSolid( SOLID_BBOX );
	UTIL_SetSize(this, MANNED_SHIELD_MINS, MANNED_SHIELD_MAXS);

	SetMaxPassengerCount( 1 );

	SetType( OBJ_MANNED_SHIELD );
	
	SetContextThink( ShieldRotationThink, gpGlobals->curtime + 0.1, g_pMannedShieldThinkContextName );
}

//-----------------------------------------------------------------------------
// Purpose: Powerup has just started
//-----------------------------------------------------------------------------
void CObjectMannedShield::PowerupStart( int iPowerup, float flAmount, CBaseEntity *pAttacker, CDamageModifier *pDamageModifier )
{
	switch( iPowerup )
	{
	case POWERUP_BOOST:
		// Increase our shield's energy
		if ( m_hDeployedShield )
		{
			m_hDeployedShield->SetPower( m_hDeployedShield->GetPower() + (flAmount * 10) );
		}
		break;

	case POWERUP_EMP:
		if (m_hDeployedShield)
		{
			m_hDeployedShield->SetEMPed(true);
		}
		break;

	default:
		break;
	}

	BaseClass::PowerupStart( iPowerup, flAmount, pAttacker, pDamageModifier );
}

//-----------------------------------------------------------------------------
// Purpose: Powerup has just started
//-----------------------------------------------------------------------------
void CObjectMannedShield::PowerupEnd( int iPowerup )
{
	switch ( iPowerup )
	{
	case POWERUP_EMP:
		if (m_hDeployedShield)
		{
			m_hDeployedShield->SetEMPed(false);
		}
		break;

	default:
		break;
	}

	BaseClass::PowerupEnd( iPowerup );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectMannedShield::ShieldRotationThink( void )
{
	if ( m_hDeployedShield )
	{
		QAngle vAngles;
		vAngles[YAW] = GetAbsAngles()[YAW] + GetGunYaw();
		vAngles[PITCH] = GetAbsAngles()[PITCH] + GetGunPitch();
		vAngles[ROLL] = 0;
		m_hDeployedShield->SetCenterAngles( vAngles );
	}

	SetContextThink( ShieldRotationThink, gpGlobals->curtime + 0.1, g_pMannedShieldThinkContextName );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectMannedShield::Precache( void )
{
	PrecacheModel( MANNED_SHIELD_MODEL );
}

//-----------------------------------------------------------------------------
// Purpose: Get and set the current driver.
//-----------------------------------------------------------------------------
void CObjectMannedShield::SetPassenger( int nRole, CBasePlayer *pEnt )
{
	BaseClass::SetPassenger( nRole, pEnt );

	// If we just got a player, create the shield. Otherwise, remove it
	if ( pEnt )
	{
		if ( !m_hDeployedShield )
		{
			// Hack our angles so the mobile shield appears directly in front of the gun
			QAngle vecOldAngles = GetAbsAngles();
			QAngle vAngles;
			vAngles[YAW] = GetAbsAngles()[YAW] + GetGunYaw();
			vAngles[PITCH] = GetAbsAngles()[PITCH] + GetGunPitch();
			vAngles[ROLL] = 0;
			SetAbsAngles( vAngles );

			int nAttachmentIndex = LookupAttachment( "barrel" );
			m_hDeployedShield = CreateMobileShield( this, 0 );
			if ( m_hDeployedShield )
			{
				m_hDeployedShield->SetThetaPhi( 60, 40 );
				m_hDeployedShield->SetAngularSpringConstant( 15 );
				m_hDeployedShield->SetAlwaysOrient( false );
				m_hDeployedShield->SetAttachmentIndex( nAttachmentIndex );
			}

			SetAbsAngles( vecOldAngles );
		}
	}
	else
	{
		if ( m_hDeployedShield )
		{
			UTIL_Remove( m_hDeployedShield );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectMannedShield::OnItemPostFrame( CBaseTFPlayer *pDriver )
{
	int iOldButtons = pDriver->m_nButtons;

	// Manned shields have no gun, so map both attack buttons to laser designate
	if ( pDriver->m_nButtons & IN_ATTACK )
	{
		pDriver->m_nButtons &= ~IN_ATTACK;
		pDriver->m_nButtons |= IN_ATTACK2;
	}

	BaseClass::OnItemPostFrame( pDriver );

	pDriver->m_nButtons = iOldButtons;
}
