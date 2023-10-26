#ifndef _INCLUDED_C_ASW_TRAIL_BEAM_H
#define _INCLUDED_C_ASW_TRAIL_BEAM_H
#ifdef _WIN32
#pragma once
#endif

#include "iviewrender_beams.h"

class C_ASW_Trail_Beam : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_ASW_Trail_Beam, C_BaseEntity );

	C_ASW_Trail_Beam();
	virtual ~C_ASW_Trail_Beam();
	void InitBeam(Vector vecStartPoint, C_BaseEntity *pTarget);
	virtual void ClientThink( void );							// Client-side think function for the entity
	C_BaseEntity* GetBeamTarget();
	
public:
	float m_fLifeLeft;	// how many seconds until we die		
	BeamInfo_t m_BeamInfo;
	Beam_t *m_pBeam;
	EHANDLE m_hTarget;
	Vector m_vecStartPoint;
};


#endif //_INCLUDED_C_ASW_TRAIL_BEAM_H