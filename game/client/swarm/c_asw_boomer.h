#ifndef __INCLUDE_C_ASW_BOOMER_H
#define __INCLUDE_C_ASW_BOOMER_H

#include "c_asw_alien.h"

class C_ASW_Boomer : public C_ASW_Alien
{
public:
	DECLARE_CLASS( C_ASW_Boomer, C_ASW_Alien )
	DECLARE_CLIENTCLASS();

					C_ASW_Boomer();
	virtual			~C_ASW_Boomer();

	virtual Class_T	Classify() { return (Class_T) CLASS_ASW_BOOMER; }
		
	// death;
	virtual C_ClientRagdoll* CreateClientRagdoll( bool bRestoring = false );
	virtual C_BaseAnimating* BecomeRagdollOnClient( void );
	virtual const char *GetDeathParticleEffectName( void ) { return "boomer_death"; }
	virtual const char *GetBigDeathParticleEffectName( void ) { return "boomer_explode"; }

	virtual const Vector& GetAimTargetPos(const Vector &vecFiringSrc, bool bWeaponPrefersFlatAiming) { return WorldSpaceCenter(); }
	virtual const Vector& GetAimTargetRadiusPos(const Vector &vecFiringSrc) { return WorldSpaceCenter(); }

	// did i explode?
	CNetworkVar(bool, m_bBoomerExplode);
	//void SpawnClientSideEffects();
	CNetworkVar( bool, m_bInflated );
	CNetworkVar( bool, m_bInflating );

private:
	C_ASW_Boomer ( const C_ASW_Boomer & ); // not defined, not accessible
};

#endif // __INCLUDE_C_ASW_BOOMER_H