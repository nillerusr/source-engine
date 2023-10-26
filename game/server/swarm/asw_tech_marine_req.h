#ifndef _INCLUDED_ASW_TECH_MARINE_REQ_H
#define _INCLUDED_ASW_TECH_MARINE_REQ_H
#ifdef _WIN32
#pragma once
#endif

#include "baseentity.h"
// Hold debriefing messages

class CASW_Tech_Marine_Req : public CLogicalEntity
{
public:
	DECLARE_CLASS( CASW_Tech_Marine_Req, CLogicalEntity );
	DECLARE_DATADESC(); 

	virtual void Spawn();
	void InputDisableTechMarineReq( inputdata_t &inputdata );
	void InputEnableTechMarineReq( inputdata_t &inputdata );
};

#endif /* _INCLUDED_ASW_TECH_MARINE_REQ_H */