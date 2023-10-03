#ifndef _INCLUDED_C_ASW_GUN_SMOKE_EMITTER_H
#define _INCLUDED_C_ASW_GUN_SMOKE_EMITTER_H

#include "c_asw_generic_emitter_entity.h"

class C_ASW_Gun_Smoke_Emitter : public C_ASW_Emitter
{
public:
	DECLARE_CLASS( C_ASW_Gun_Smoke_Emitter, C_ASW_Emitter );

	C_ASW_Gun_Smoke_Emitter();
	virtual void ClientThink();
	void OnFired();	// our gun calls this when it fires
	void StartSmoking();
	void StopSmoking();

	float m_fFireCount;
	float m_fLastFireTime;
};

#endif /* _INCLUDED_C_ASW_GUN_SMOKE_EMITTER_H */