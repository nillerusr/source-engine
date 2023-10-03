//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_ASW_FIRE_H
#define C_ASW_FIRE_H

//#include "entityoutput.h"
//#include "fire_smoke.h"
//#include "plasma.h"
#include "c_baseentity.h"


//==================================================

class C_Fire : public CBaseEntity
{
public:
	DECLARE_CLASS( C_Fire, CBaseEntity );
	DECLARE_CLIENTCLASS();

	C_Fire();
	virtual			~C_Fire();

	virtual void ClientThink();
	void OnDataChanged( DataUpdateType_t updateType );
	void CreateFireParticles();
	void UpdateFireParticles();
	CUtlReference<CNewParticleEffect> m_hFire;
	CUtlReference<CNewParticleEffect> m_hFireTop;
	int m_nFireType;
	float m_flFireSize;
	float m_flHeatLevel;
	float m_flMaxHeat;
	bool m_bEnabled;
};

#endif // C_ASW_FIRE_H
