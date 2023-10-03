//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef C_ASW_TESLATRAP_H
#define C_ASW_TESLATRAP_H

#include "iasw_client_usable_entity.h"

#ifdef _WIN32
#pragma once
#endif

//---------------------------------------------------------
//---------------------------------------------------------

class C_ASW_TeslaTrap : public C_BaseCombatCharacter, public IASW_Client_Usable_Entity
{
	DECLARE_CLASS( C_ASW_TeslaTrap, C_BaseCombatCharacter );

public:
	DECLARE_CLIENTCLASS();
	C_ASW_TeslaTrap();
	virtual			~C_ASW_TeslaTrap();

	virtual void Precache() {}
	virtual void OnDataChanged( DataUpdateType_t type );

	void SetRadius( float flRadius ) { m_flRadius = flRadius; }
	void SetDamage( float flDamage ) { m_flDamage = flDamage; }
	void SetNextFullChargeTime( float flNextFullChargeTime ) { m_flNextFullChargeTime = flNextFullChargeTime; }
	int GetTrapState() { return m_iTrapState; }
	int GetAmmo() { return m_iAmmo; }
	int GetMaxAmmo() { return m_iMaxAmmo; }

	//void UpdateLight( bool bTurnOn, unsigned int r, unsigned int g, unsigned int b, unsigned int a );
	virtual void ClientThink();
	void UpdateEffects( void );

	DECLARE_DATADESC();

	// IASW_Client_Usable_Entity
	virtual C_BaseEntity* GetEntity() { return this; }
	virtual bool IsUsable(C_BaseEntity *pUser);
	virtual bool GetUseAction(ASWUseAction &action, C_ASW_Marine *pUser);
	virtual void CustomPaint(int ix, int iy, int alpha, vgui::Panel *pUseIcon);
	virtual bool ShouldPaintBoxAround() { return false; }
	virtual bool NeedsLOSCheck() { return true; }
	int GetTeslaIconTextureID();

	static bool s_bLoadedTeslaIcon;
	static int s_nTeslaIconTextureID;

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_TESLA_TRAP_PROJECTILE; }
private:
	int m_iTrapState;
	int m_iAmmo;
	int m_iMaxAmmo;
	float m_flRadius;
	float m_flDamage;
	float m_flNextFullChargeTime;
	bool m_bAssembled;

	CUtlReference<CNewParticleEffect> m_hEffects;
};

#endif // C_ASW_TESLATRAP_H
