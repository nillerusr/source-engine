#ifndef _INCLUDED_C_ASW_OBJECTIVE_COUNTDOWN_H
#define _INCLUDED_C_ASW_OBJECTIVE_COUNTDOWN_H

#include "c_asw_objective.h"

class CASW_VGUI_Countdown_Panel;

class C_ASW_Objective_Countdown : public C_ASW_Objective
{
public:
	DECLARE_CLASS( C_ASW_Objective_Countdown, C_ASW_Objective );
	DECLARE_CLIENTCLASS();

					C_ASW_Objective_Countdown();
	virtual			~C_ASW_Objective_Countdown();

	virtual void OnDataChanged( DataUpdateType_t updateType );
	virtual float GetCountdownFinishTime() { return m_fCountdownFinishTime;  }
	CNetworkVar(float, m_fCountdownFinishTime);

	bool m_bLaunchedPanel;
	CASW_VGUI_Countdown_Panel* m_pCountdownPanel;

private:
	C_ASW_Objective_Countdown( const C_ASW_Objective_Countdown & ); // not defined, not accessible
};


#endif // _INCLUDED_C_ASW_OBJECTIVE_COUNTDOWN_H