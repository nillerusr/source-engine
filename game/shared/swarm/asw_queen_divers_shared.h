#ifndef _INCLUDE_ASW_QUEEN_DIVERS_SHARED_H
#define _INCLUDE_ASW_QUEEN_DIVERS_SHARED_H
#ifdef _WIN32
#pragma once
#endif

#ifdef CLIENT_DLL
	#include "c_asw_queen.h"
	#define CASW_Queen C_ASW_Queen
#else
	#include "asw_queen.h"
#endif


// Queen Divers are the curved tentacles that come out of the queen and burrow into the ground
//  in front of her (the Queen Grabbers are the other ends of these tentacles, that come up
//  out of the ground and grab a marine)

#ifdef CLIENT_DLL
class CASW_Queen_Divers : public CBaseAnimating, public IASW_Client_Aim_Target
#else
class CASW_Queen_Divers : public CBaseAnimating
#endif
{
public:
	DECLARE_CLASS( CASW_Queen_Divers, CBaseAnimating );
	DECLARE_NETWORKCLASS();

	CASW_Queen_Divers();
	virtual ~CASW_Queen_Divers();

	Class_T		Classify( void ) { return (Class_T) CLASS_ASW_QUEEN_DIVER; }

#ifndef CLIENT_DLL
	DECLARE_DATADESC();
	void Precache();
	void Spawn();
	
	virtual int OnTakeDamage( const CTakeDamageInfo &info );
	void SetBurrowing(bool bBurrowing);
	void AnimThink();
	
	void TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );
	void ASWTraceBleed( float flDamage, const Vector &vecDir, trace_t *ptr, int bitsDamageType );
	bool PassesDamageFilter( const CTakeDamageInfo &info );
	void SetVisible(bool bVisible);

	CHandle<CASW_Queen> m_hQueen;

	static CASW_Queen_Divers *Create_Queen_Divers( CASW_Queen *pQueen );	
#else
	// client only

	// aim entity stuff
	IMPLEMENT_AUTO_LIST_GET();
	virtual float GetRadius() { return 24; }
	virtual bool IsAimTarget() { return true; }
	virtual const Vector& GetAimTargetPos(const Vector &vecFiringSrc, bool bWeaponPrefersFlatAiming) { return WorldSpaceCenter(); }
	virtual const Vector& GetAimTargetRadiusPos(const Vector &vecFiringSrc) { return WorldSpaceCenter(); }
#endif

	virtual int BloodColor();
};


#endif /* _INCLUDE_ASW_QUEEN_DIVERS_SHARED_H */
