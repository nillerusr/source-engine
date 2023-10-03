#include "cbase.h"
#include "c_asw_weapon_rifle.h"
#include "c_asw_marine.h"
#include "c_asw_player.h"
#include "asw_weapon_parse.h"
#define CASW_Marine C_ASW_Marine
#define CASW_Player C_ASW_Player

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_ASW_Weapon_Rifle, DT_ASW_Weapon_Rifle, CASW_Weapon_Rifle)

END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_ASW_Weapon_Rifle )

END_PREDICTION_DATA()

// need this so weapon script gets the right class name clientside
LINK_ENTITY_TO_CLASS( asw_weapon_rifle, C_ASW_Weapon_Rifle );

C_ASW_Weapon_Rifle::C_ASW_Weapon_Rifle()
{

}


C_ASW_Weapon_Rifle::~C_ASW_Weapon_Rifle()
{

}

void C_ASW_Weapon_Rifle::SecondaryAttack( void )
{
	CASW_Player *pPlayer = GetCommander();
	if (!pPlayer)
		return;

	CASW_Marine *pMarine = GetMarine();

	if (!pMarine)
		return;

	//Must have ammo
	bool bUsesSecondary = UsesSecondaryAmmo();
	bool bUsesClips = UsesClipsForAmmo2();
	int iAmmoCount = pMarine->GetAmmoCount(m_iSecondaryAmmoType);
	bool bInWater = ( pMarine->GetWaterLevel() == 3 );
	if ( (bUsesSecondary &&  
			(   ( bUsesClips && m_iClip1 <= 0) ||
			    ( !bUsesClips && iAmmoCount<=0 )
				) )
				 || bInWater || m_bInReload )
	//if ( ( pMarine->GetAmmoCount( m_iSecondaryAmmoType ) <= 0 ) || ( pMarine->GetWaterLevel() == 3 ) )
	{
		SendWeaponAnim( ACT_VM_DRYFIRE );
		BaseClass::WeaponSound( EMPTY );
		m_flNextSecondaryAttack = gpGlobals->curtime + 0.5f;
		return;
	}
	
	BaseClass::WeaponSound( SPECIAL1 );

	Vector vecSrc = pMarine->Weapon_ShootPosition();
	Vector	vecThrow;	
	vecThrow = pPlayer->GetAutoaimVectorForMarine(pMarine, GetAutoAimAmount(), GetVerticalAdjustOnlyAutoAimAmount());	// 45 degrees = 0.707106781187
	VectorScale( vecThrow, 1000.0f, vecThrow );
	
	SendWeaponAnim( ACT_VM_SECONDARYATTACK );
	if ( bUsesClips )
	{
		m_iClip2 -= 1;
	}
	else
	{
		pMarine->RemoveAmmo( 1, m_iSecondaryAmmoType );
	}

	// Can shoot again immediately
	m_flNextPrimaryAttack = gpGlobals->curtime + 0.5f;

	// Can blow up after a short delay (so have time to release mouse button)
	m_flNextSecondaryAttack = gpGlobals->curtime + 1.0f;
}

bool C_ASW_Weapon_Rifle::SupportsBayonet()
{
	return true;
}

float C_ASW_Weapon_Rifle::GetFireRate()
{
	//float flRate = 0.07f;
	float flRate = GetWeaponInfo()->m_flFireRate;

	//CALL_ATTRIB_HOOK_FLOAT( flRate, mod_fire_rate );

	return flRate;
}