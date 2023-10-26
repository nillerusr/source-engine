#include "cbase.h"
#include "c_baseentity.h"
#include "c_asw_ranger.h"
#include "c_asw_clientragdoll.h"
#include "asw_fx_shared.h"
#include "functionproxy.h"
#include "imaterialproxydict.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_ASW_Ranger, DT_ASW_Ranger, CASW_Ranger)
END_RECV_TABLE()

C_ASW_Ranger::C_ASW_Ranger()
{
 
}


C_ASW_Ranger::~C_ASW_Ranger()
{
	
}

void C_ASW_Ranger::SpawnClientSideEffects()
{
	//ParticleProp()->Create( "meatbug_death", PATTACH_POINT, "attach_explosion" );
}

C_BaseAnimating * C_ASW_Ranger::BecomeRagdollOnClient( void )
{
	//SpawnClientSideEffects();	
	return BaseClass::BecomeRagdollOnClient();
}

C_ClientRagdoll *C_ASW_Ranger::CreateClientRagdoll( bool bRestoring )
{
	return new C_ASW_ClientRagdoll( bRestoring );
}


// hardcoded to match with the gun offset in the marine's autoaim
//  this is to generally keep the marine's gun horizontal, so guns like the shotgun can more easily hit multiple enemies in one shot
const Vector& C_ASW_Ranger::GetAimTargetPos(const Vector &vecFiringSrc, bool bWeaponPrefersFlatAiming)
{ 
	static Vector aim_pos;
	aim_pos = m_vecLastRenderedPos - (WorldSpaceCenter() - GetAbsOrigin());	// last rendered stores our worldspacecenter, so convert to back origin
	aim_pos.z += ASW_MARINE_GUN_OFFSET_Z;
	return aim_pos;
}