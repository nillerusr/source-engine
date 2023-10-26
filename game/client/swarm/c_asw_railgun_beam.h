#ifndef _INCLUDED_C_ASW_RAILGUN_BEAM_H
#define _INCLUDED_C_ASW_RAILGUN_BEAM_H
#ifdef _WIN32
#pragma once
#endif

#include "iviewrender_beams.h"

class C_ASW_Railgun_Beam : public C_BaseEntity
{
public:
	//DECLARE_CLIENTCLASS();
	DECLARE_CLASS( C_ASW_Railgun_Beam, C_BaseEntity );

	C_ASW_Railgun_Beam();
	virtual ~C_ASW_Railgun_Beam();
	void InitBeam(Vector vecStartPoint, Vector vecEndPoint);
	virtual void ClientThink( void );							// Client-side think function for the entity
	
public:
	float m_fLifeLeft;	// how many seconds until we die		
	BeamInfo_t m_BeamInfo;
	Beam_t *m_pBeam;
};

#endif // _INCLUDED_C_ASW_RAILGUN_BEAM_H