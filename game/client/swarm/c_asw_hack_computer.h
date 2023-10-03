#ifndef _DEFINED_C_ASW_HACK_COMPUTER_H
#define _DEFINED_C_ASW_HACK_COMPUTER_H

#include "c_asw_hack.h"
#include "asw_shareddefs.h"
#include <vgui_controls/PHandle.h>

class C_ASW_Computer_Area;
class CASW_VGUI_Computer_Frame;
class CASW_VGUI_Frame;
class CASW_Player;

class C_ASW_Hack_Computer :public C_ASW_Hack
{
public:
	C_ASW_Hack_Computer();
	virtual ~C_ASW_Hack_Computer();

	DECLARE_CLASS( C_ASW_Hack_Computer, C_ASW_Hack );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	void OnDataChanged( DataUpdateType_t updateType );
	void ClientThink();
	bool ShouldPredict();
	C_ASW_Computer_Area* GetComputerArea();
	float GetNextMoveTime() { return m_fNextMoveTime.Get(); }
	float GetTumblerDiffTime();
	float m_fTumblerDiffTime;	// difference between curtime and movetime, calculated in our predicted function (since predicted code seems to run out of time with drawing code)

	virtual void ASWPostThink(C_ASW_Player *pPlayer, C_ASW_Marine *pMarine,  CUserCmd *ucmd, float fDeltaTime);
	virtual void ReverseTumbler(int i, C_ASW_Marine *pMarine);
	
	CNetworkVar( int,  m_iNumTumblers );		// how many tumblers this hack puzzle has
	CNetworkVar( int,  m_iEntriesPerTumbler );	
	CNetworkVar( float, m_fNextMoveTime );
	CNetworkVar( float, m_fMoveInterval );
	CNetworkArray( int, m_iTumblerPosition, ASW_HACK_COMPUTER_MAX_TUMBLERS );
	CNetworkArray( int, m_iTumblerCorrectNumber, ASW_HACK_COMPUTER_MAX_TUMBLERS );
	CNetworkArray( int, m_iTumblerDirection, ASW_HACK_COMPUTER_MAX_TUMBLERS );
	int m_iNewTumblerDirection[ASW_HACK_COMPUTER_MAX_TUMBLERS];
	int m_iNewTumblerPosition[ASW_HACK_COMPUTER_MAX_TUMBLERS];
	virtual bool CanOverrideHack();

	// returns the tumbler position, using the clientside ones (NewTumblerPosition) if they're set
	int GetTumblerPosition(int iIndex);

	virtual void FrameDeleted(vgui::Panel *pPanel);	

	virtual bool IsTumblerCorrect(int iTumbler);
	virtual float GetTumblerProgress();
	bool m_bLastAllCorrect;
	int m_iLastNumWrong;
	bool m_bLastHalfCorrect;
	void UpdateCorrectStatus(CASW_Player *pPlayer, C_ASW_Marine *pMarine, int iNumWrong);

	bool m_bLaunchedHackPanel;
	int m_iOldShowOption;

	float m_fStartedHackTime;
	float m_fFastFinishTime;

	vgui::DHANDLE<CASW_VGUI_Frame> m_hFrame;
	vgui::DHANDLE<CASW_VGUI_Computer_Frame> m_hComputerFrame;

private:
	C_ASW_Hack_Computer( const C_ASW_Hack_Computer & ); // not defined, not accessible
};

#endif /* _DEFINED_C_ASW_HACK_COMPUTER_H */