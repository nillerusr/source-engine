#ifndef _INCLUDED_C_ASW_OBJECTIVE_KILL_ALIENS_H
#define _INCLUDED_C_ASW_OBJECTIVE_KILL_ALIENS_H

#include "c_asw_objective.h"

class C_ASW_Objective_Kill_Aliens : public C_ASW_Objective
{
public:
	DECLARE_CLASS( C_ASW_Objective_Kill_Aliens, C_ASW_Objective );
	DECLARE_CLIENTCLASS();

	C_ASW_Objective_Kill_Aliens();

	virtual void OnDataChanged(DataUpdateType_t updateType);
	virtual const wchar_t* GetObjectiveTitle();
	virtual bool NeedsTitleUpdate();
	virtual float GetObjectiveProgress();
	void FindText();
	wchar_t *GetPluralText();
	wchar_t *GetSingularText();

	CNetworkVar(int, m_iTargetKills);
	CNetworkVar(int, m_iCurrentKills);
	CNetworkVar(int, m_AlienClassNum);

	wchar_t *m_pKillText;
	wchar_t *m_pAlienPluralText;
	wchar_t *m_pAlienSingularText;
	bool m_bFoundText;

	wchar_t m_dest_buffer[64];
	int m_iKillsLeft;

private:
	C_ASW_Objective_Kill_Aliens( const C_ASW_Objective_Kill_Aliens & ); // not defined, not accessible
};


#endif // _INCLUDED_C_ASW_OBJECTIVE_KILL_ALIENS_H