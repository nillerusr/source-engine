//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef C_ASW_PROP_PHYSICS_H
#define C_ASW_PROP_PHYSICS_H
#ifdef _WIN32
#pragma once
#endif

#include "c_physicsprop.h"
#include "iasw_client_aim_target.h"
#include "object_motion_blur_effect.h"
#include "asw_shareddefs.h"

class CNewParticleEffect;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_ASW_Prop_Physics : public C_PhysicsProp, public IASW_Client_Aim_Target	// asw make props potential aim targets
{
	typedef C_PhysicsProp BaseClass;
public:
	DECLARE_CLIENTCLASS();

	// Other public methods
public:
	C_ASW_Prop_Physics();
	virtual ~C_ASW_Prop_Physics();

	virtual void UpdateOnRemove();
	void UpdateFireEmitters();

	// asw
	virtual void OnDataChanged( DataUpdateType_t updateType );

	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_PHYSICS_PROP; }
	
	IMPLEMENT_AUTO_LIST_GET();
	virtual float GetRadius() { return 15; }
	virtual bool IsAimTarget() { return m_bMadeAimTarget; }
	virtual const Vector& GetAimTargetPos(const Vector &vecFiringSrc, bool bWeaponPrefersFlatAiming) { return WorldSpaceCenter(); }
	virtual const Vector& GetAimTargetRadiusPos(const Vector &vecFiringSrc) { return WorldSpaceCenter(); }
	bool m_bMadeAimTarget;
	bool m_bIsAimTarget;
	bool m_bOnFire, m_bClientOnFire;
	CNewParticleEffect	*m_pBurningEffect;
	CMotionBlurObject m_MotionBlurObject;
};

#endif // C_ASW_PROP_PHYSICS_H 
