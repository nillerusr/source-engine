#ifndef _INCLUDED_C_ASW_LEVEL_EXPLODER_H
#define _INCLUDED_C_ASW_LEVEL_EXPLODER_H
#ifdef _WIN32
#pragma once
#endif

class C_ASW_Level_Exploder : public C_BaseEntity
{
	DECLARE_CLASS( C_ASW_Level_Exploder, C_BaseEntity );	

public:
	C_ASW_Level_Exploder();

	virtual void ClientThink();
	
	static C_ASW_Level_Exploder *CreateClientsideLevelExploder();

	bool m_bFlashed;
	float m_fDieTime;
};

#endif //_INCLUDED_C_ASW_LEVEL_EXPLODER_H