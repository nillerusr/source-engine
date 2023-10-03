#ifndef _INLCUDE_C_ASW_PARASITE_H
#define _INLCUDE_C_ASW_PARASITE_H

#include "c_asw_alien.h"

class C_ASW_Parasite : public C_ASW_Alien
{
public:
	DECLARE_CLASS( C_ASW_Parasite, C_ASW_Alien );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

					C_ASW_Parasite();
	virtual			~C_ASW_Parasite();

	void OnRestore();
	void UpdateOnRemove();
	void SoundInit();
	void SoundShutdown();
	void OnDataChanged( DataUpdateType_t updateType );
	virtual const char *GetBigDeathParticleEffectName( void ) { return "drone_death_sml"; }
	virtual float GetRadius() { return 12; }
	Class_T		Classify( void ) { return (Class_T) CLASS_ASW_PARASITE; }	
	virtual bool IsAimTarget();

private:
	C_ASW_Parasite( const C_ASW_Parasite & ); // not defined, not accessible
	CSoundPatch		*m_pLoopingSound;
	bool m_bStartIdleSound;
	bool m_bDoEggIdle;
	bool m_bInfesting;
};

#endif /* _INLCUDE_C_ASW_PARASITE_H */