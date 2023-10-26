#include "cbase.h"
#include "C_ASW_Shieldbug.h"
#include "engine/IVDebugOverlay.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


ConVar asw_shieldbug_death_force( "asw_shieldbug_death_force", "65000" , 0, "this sets the custom death force for the exploding shieldbug");

IMPLEMENT_CLIENTCLASS_DT(C_ASW_Shieldbug, DT_ASW_Shieldbug, CASW_Shieldbug)
	
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_ASW_Shieldbug )

END_PREDICTION_DATA()

C_ASW_Shieldbug::C_ASW_Shieldbug()
{
}

C_ASW_Shieldbug::~C_ASW_Shieldbug()
{
}

Vector C_ASW_Shieldbug::GetCustomDeathForce()
{
	Vector deathForce;
	deathForce.z = asw_shieldbug_death_force.GetInt();
	return deathForce;
}

// plays alien type specific footstep sound
void C_ASW_Shieldbug::DoAlienFootstep(Vector &vecOrigin, float fvol)
{
	CSoundParameters params;
	if ( !CBaseEntity::GetParametersForSound( "ASW_ShieldBug.StepLight", params, NULL ) )
		return;

	CLocalPlayerFilter filter;

	// do the alienfleshy foot sound
	EmitSound_t ep2;
	ep2.m_nChannel = CHAN_AUTO;
	ep2.m_pSoundName = params.soundname;
	ep2.m_flVolume = fvol * 0.2f;
	ep2.m_SoundLevel = params.soundlevel;
	ep2.m_nFlags = 0;
	ep2.m_nPitch = params.pitch;
	ep2.m_pOrigin = &vecOrigin;

	EmitSound( filter, entindex(), ep2 );
}

// hardcoded to match with the gun offset in the marine's autoaim
//  this is to generally keep the marine's gun horizontal, so guns like the shotgun can more easily hit multiple enemies in one shot
const Vector& C_ASW_Shieldbug::GetAimTargetPos(const Vector &vecFiringSrc, bool bWeaponPrefersFlatAiming)
{ 
	static Vector aim_pos;
	aim_pos = m_vecLastRenderedPos - (WorldSpaceCenter() - GetAbsOrigin());	// last rendered stores our worldspacecenter, so convert to back origin
	aim_pos.z += ASW_MARINE_GUN_OFFSET_Z;
	return aim_pos;
}