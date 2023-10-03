#ifndef _INCLUDED_C_ASW_QUEEN
#define _INCLUDED_C_ASW_QUEEN
#ifdef _WIN32
#pragma once
#endif

#include "c_asw_alien.h"

class C_ASW_Queen : public C_ASW_Alien
{
public:
	DECLARE_CLASS( C_ASW_Queen, C_ASW_Alien );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

					C_ASW_Queen();
	virtual			~C_ASW_Queen();

	Class_T		Classify( void ) { return (Class_T) CLASS_ASW_QUEEN; }

	virtual void ClientThink();
	virtual void OnDataChanged( DataUpdateType_t updateType );
	void UpdatePoseParams();
	Vector BodyDirection3D();
	float VecToYaw( const Vector &vecDir );
	virtual const Vector& GetAimTargetPos(const Vector &vecFiringSrc, bool bWeaponPrefersFlatAiming);
	virtual void GetPoseParameters( CStudioHdr *pStudioHdr, float poseParameter[MAXSTUDIOPOSEPARAM]);

	CNetworkHandle(C_BaseEntity, m_hQueenEnemy);
	CNetworkVar(bool, m_bChestOpen);

	virtual int	GetHealth() const { return m_iHealth; }
	int GetMaxHealth( void ) const { return m_iMaxHealth; }
	int  m_iMaxHealth;

	float m_fLastShieldPose;
	float m_fLastHeadYaw;

private:
	float m_flClientPoseParameter[MAXSTUDIOPOSEPARAM];

	C_ASW_Queen( const C_ASW_Queen & ); // not defined, not accessible
};

#endif /* _INCLUDED_C_ASW_QUEEN */