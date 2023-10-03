#ifndef _INCLUDED_ASW_QUEEN_RETREAT_SPOT_H
#define _INCLUDED_ASW_QUEEN_RETREAT_SPOT_H
#ifdef _WIN32
#pragma once
#endif

#include "baseentity.h"

class CASW_Queen_Retreat_Spot : public CPointEntity
{
public:
	DECLARE_CLASS( CASW_Queen_Retreat_Spot, CPointEntity );
	DECLARE_DATADESC();
};
#endif /* _INCLUDED_ASW_QUEEN_RETREAT_SPOT_H */