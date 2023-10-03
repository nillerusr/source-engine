#ifndef _INCLUDED_C_ASW_RANGER_H
#define _INCLUDED_C_ASW_RANGER_H

#include "c_asw_alien.h"

class C_ASW_Ranger : public C_ASW_Alien
{
public:
	DECLARE_CLASS( C_ASW_Ranger, C_ASW_Alien );
	DECLARE_CLIENTCLASS();

					C_ASW_Ranger();
	virtual			~C_ASW_Ranger();

	virtual Class_T	Classify() { return (Class_T) CLASS_ASW_RANGER; }
		
	// death;
	virtual const char *GetBigDeathParticleEffectName( void ) { return "ranger_death_big"; }
	virtual C_ClientRagdoll* CreateClientRagdoll( bool bRestoring = false );
	virtual C_BaseAnimating* BecomeRagdollOnClient( void );
	void	SpawnClientSideEffects();
	virtual const Vector& GetAimTargetPos(const Vector &vecFiringSrc, bool bWeaponPrefersFlatAiming);
private:
	C_ASW_Ranger( const C_ASW_Ranger & ); // not defined, not accessible
};

#endif /* _INCLUDED_C_ASW_RANGER_H */
