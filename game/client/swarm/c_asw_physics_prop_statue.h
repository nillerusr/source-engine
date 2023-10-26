//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#ifndef C_ASW_PHYSICS_PROP_STATUE_H
#define C_ASW_PHYSICS_PROP_STATUE_H

#ifdef _WIN32
#pragma once
#endif

#include "asw_shareddefs.h"
#include "c_physics_prop_statue.h"
#include "iasw_client_aim_target.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_ASWStatueProp : public C_StatueProp, public IASW_Client_Aim_Target
{
public:
	DECLARE_CLASS( C_ASWStatueProp, C_StatueProp );
	DECLARE_CLIENTCLASS();

	C_ASWStatueProp();

	virtual void Spawn( void );

	// aim target interface
	IMPLEMENT_AUTO_LIST_GET();
	virtual float GetRadius() { return m_flAimTargetRadius; }
	virtual bool IsAimTarget() { return true; }
	virtual const Vector& GetAimTargetPos(const Vector &vecFiringSrc, bool bWeaponPrefersFlatAiming) { return WorldSpaceCenter(); }
	virtual const Vector& GetAimTargetRadiusPos(const Vector &vecFiringSrc) { return WorldSpaceCenter(); }

	virtual void ComputeWorldSpaceSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs );

	virtual void	OnDataChanged( DataUpdateType_t updateType );

	virtual void TakeDamage( const CTakeDamageInfo &info );		// called when entity is damaged by predicted attacks

	Class_T		Classify( void ) { return (Class_T) CLASS_ASW_STATUE; }

	bool m_bShattered;
	float m_flAimTargetRadius;
};


#endif // C_ASW_PHYSICS_PROP_STATUE_H