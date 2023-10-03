#ifndef _INCLUDED_C_ASW_ROCKET_H
#define _INCLUDED_C_ASW_ROCKET_H

#include "c_basecombatcharacter.h"

class C_ASW_Emitter;
class Beam_t;

class C_ASW_Rocket : public C_BaseCombatCharacter
{
	DECLARE_CLASS( C_ASW_Rocket, C_BaseCombatCharacter );
	DECLARE_CLIENTCLASS();
public:

	Class_T		Classify( void ) { return CLASS_MISSILE; }

	C_ASW_Rocket();
	virtual ~C_ASW_Rocket();
	virtual void OnDataChanged(DataUpdateType_t updateType);
	virtual void ClientThink();
	virtual void UpdateOnRemove();
	virtual void SoundInit();
	void CreateSmokeTrail();

	CUtlReference<CNewParticleEffect> m_pSmokeTrail;
	CSoundPatch *m_pLoopingSound;
};

#endif // _INCLUDED_C_ASW_ROCKET_H