#ifndef _INCLUDED_C_ASW_OBJECTIVE_KILL_QUEEN_H
#define _INCLUDED_C_ASW_OBJECTIVE_KILL_QUEEN_H

#include "c_asw_objective.h"

class CASW_VGUI_Queen_Health_Panel;
class C_ASW_Queen;

class C_ASW_Objective_Kill_Queen : public C_ASW_Objective
{
public:
	DECLARE_CLASS( C_ASW_Objective_Kill_Queen, C_ASW_Objective );
	DECLARE_CLIENTCLASS();

					C_ASW_Objective_Kill_Queen();
	virtual			~C_ASW_Objective_Kill_Queen();

	virtual void OnDataChanged( DataUpdateType_t updateType );

	bool m_bLaunchedPanel;
	CASW_VGUI_Queen_Health_Panel* m_pQueenHealthPanel;
	C_ASW_Queen* GetQueen() { return m_hQueen.Get(); }

	CNetworkHandle(C_ASW_Queen, m_hQueen);

private:
	C_ASW_Objective_Kill_Queen( const C_ASW_Objective_Kill_Queen & ); // not defined, not accessible
};


#endif // _INCLUDED_C_ASW_OBJECTIVE_KILL_QUEEN_H