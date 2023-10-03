#ifndef _INCLUDED_C_ASW_EGG_H
#define _INCLUDED_C_ASW_EGG_H

#include "iasw_client_aim_target.h"
#include "asw_shareddefs.h"
#include "glow_outline_effect.h"

class CNewParticleEffect;

class C_ASW_Egg : public C_BaseFlex, public IASW_Client_Aim_Target
{
public:
	DECLARE_CLASS( C_ASW_Egg, C_BaseFlex );
	DECLARE_CLIENTCLASS();

					C_ASW_Egg();
	virtual			~C_ASW_Egg();

	// aim target interface
	IMPLEMENT_AUTO_LIST_GET();
	virtual float GetRadius() { return 20; }
	virtual bool IsAimTarget() { return true; }
	virtual const Vector& GetAimTargetPos(const Vector &vecFiringSrc, bool bWeaponPrefersFlatAiming) { return WorldSpaceCenter(); }
	virtual const Vector& GetAimTargetRadiusPos(const Vector &vecFiringSrc) { return WorldSpaceCenter(); }

	virtual void TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );
	virtual void Bleed( const CTakeDamageInfo &info, const Vector &vecPos, const Vector &vecDir, trace_t *ptr );

	Class_T		Classify( void ) { return (Class_T) CLASS_ASW_EGG; }
	virtual void OnDataChanged( DataUpdateType_t type );
	virtual void UpdateFireEmitters();
	virtual void UpdateOnRemove();
	virtual void ClientThink();

	CGlowObject m_GlowObject;
	bool m_bClientOnFire;
	CNetworkVar(bool, m_bOnFire);
	CNewParticleEffect	*m_pBurningEffect;
	float m_fEggAwake;	// controls green lines on the outside

private:
	C_ASW_Egg( const C_ASW_Egg & ); // not defined, not accessible
};

#endif // _INCLUDED_C_ASW_EGG_H