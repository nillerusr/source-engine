#ifndef _INCLUDED_ASW_OBJECTIVE_KILL_QUEEN_H
#define _INCLUDED_ASW_OBJECTIVE_KILL_QUEEN_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_objective.h"

class CASW_Queen;

class CASW_Objective_Kill_Queen : public CASW_Objective
{
public:
	DECLARE_CLASS( CASW_Objective_Kill_Queen, CASW_Objective );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CASW_Objective_Kill_Queen();
	virtual ~CASW_Objective_Kill_Queen();

	virtual void Spawn();

	CNetworkHandle(CASW_Queen, m_hQueen);
	string_t	m_QueenName;
};

#endif // _INCLUDED_ASW_OBJECTIVE_KILL_QUEEN_H