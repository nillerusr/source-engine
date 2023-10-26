#ifndef _INCLUDED_C_ASW_SHAMAN_H
#define _INCLUDED_C_ASW_SHAMAN_H

#include "c_asw_alien.h"

class C_ASW_Shaman : public C_ASW_Alien
{
public:
	DECLARE_CLASS( C_ASW_Shaman, C_ASW_Alien );
	DECLARE_CLIENTCLASS();

					C_ASW_Shaman();
	virtual			~C_ASW_Shaman();
		
	// death;
	virtual C_ClientRagdoll* CreateClientRagdoll( bool bRestoring = false );
	virtual C_BaseAnimating* BecomeRagdollOnClient( void );
	virtual const char *GetBigDeathParticleEffectName( void ) { return "drone_death_sml"; }
	void	SpawnClientSideEffects();

	virtual void ClientThink();
	virtual void UpdateOnRemove();
	void UpdateEffects();

	Class_T		Classify( void ) { return (Class_T) CLASS_ASW_SHAMAN; }

	CNetworkVar( EHANDLE, m_hHealingTarget );

	CUtlReference<CNewParticleEffect>	m_pHealEffect;

private:
	C_ASW_Shaman( const C_ASW_Shaman & ); // not defined, not accessible
};

#endif /* _INCLUDED_C_ASW_SHAMAN_H */