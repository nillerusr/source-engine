#ifndef _INCLUDED_ASW_TARGET_DUMMY_H
#define _INCLUDED_ASW_TARGET_DUMMY_H
#pragma once

#ifdef CLIENT_DLL
#include "iasw_client_aim_target.h"
#else

#endif

class CASW_Target_Dummy : public CBaseAnimating
#ifdef CLIENT_DLL
	, public IHealthTracked, public IASW_Client_Aim_Target
#endif
{
public:
	DECLARE_CLASS( CASW_Target_Dummy, CBaseAnimating );
	DECLARE_NETWORKCLASS();

#ifndef CLIENT_DLL
	CASW_Target_Dummy();
	~CASW_Target_Dummy();

	DECLARE_DATADESC();
	void Precache();
	void Spawn();

	void ResetThink();
	
	virtual int OnTakeDamage( const CTakeDamageInfo &info );
#else
	float GetDPS();

	// IHealthTracked
	virtual void PaintHealthBar( class CASWHud3DMarineNames *pSurface );

	// IASW_Client_Aim_Target
	IMPLEMENT_AUTO_LIST_GET();
	virtual float GetRadius() { return 40.0f; }
	virtual bool IsAimTarget() { return true; }
	virtual const Vector& GetAimTargetPos(const Vector &vecFiringSrc, bool bWeaponPrefersFlatAiming) { return WorldSpaceCenter(); }
	virtual const Vector& GetAimTargetRadiusPos(const Vector &vecFiringSrc) { return WorldSpaceCenter(); }
#endif

	float GetDamageTaken() { return m_flDamageTaken.Get(); }

	CNetworkVar( float, m_flDamageTaken );
	CNetworkVar( float, m_flLastHit );
	CNetworkVar( float, m_flStartDamageTime );
	CNetworkVar( float, m_flLastDamageTime );
};

#ifdef GAME_DLL
extern CUtlVector<CASW_Target_Dummy*> g_vecTargetDummies;
#endif

#endif /* _INCLUDED_ASW_TARGET_DUMMY_H */
