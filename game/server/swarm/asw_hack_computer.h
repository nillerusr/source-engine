#ifndef _DEFINED_ASW_HACK_COMPUTER_H
#define _DEFINED_ASW_HACK_COMPUTER_H

#include "asw_hack.h"
#include "asw_shareddefs.h"

class CASW_Computer_Area;

class CASW_Hack_Computer : public CASW_Hack
{
public:
	CASW_Hack_Computer();

	DECLARE_CLASS( CASW_Hack_Computer, CASW_Hack );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();
	
	CNetworkVar( int,  m_iNumTumblers );		// how many tumblers this hack puzzle has
	CNetworkVar( int,  m_iEntriesPerTumbler );		// the height of each tumbler

	CNetworkVar( float, m_fNextMoveTime );
	CNetworkVar( float, m_fMoveInterval );
	CNetworkArray( int, m_iTumblerPosition, ASW_HACK_COMPUTER_MAX_TUMBLERS );	
	CNetworkArray( int, m_iTumblerCorrectNumber, ASW_HACK_COMPUTER_MAX_TUMBLERS );
	CNetworkArray( int, m_iTumblerDirection, ASW_HACK_COMPUTER_MAX_TUMBLERS );

	virtual void MarineStoppedUsing(CASW_Marine* pMarine);
	int m_iNewTumblerDirection[ASW_HACK_COMPUTER_MAX_TUMBLERS];

	virtual void ASWPostThink(CASW_Player *pPlayer, CASW_Marine *pMarine,  CUserCmd *ucmd, float fDeltaTime);
	virtual void ReverseTumbler(int i, CASW_Marine *pMarine);
	virtual bool IsTumblerCorrect(int iTumbler);
	virtual float GetTumblerProgress();
	CNetworkVar(bool, m_bLastAllCorrect);
	bool m_bLastHalfCorrect;

	virtual void SelectHackOption(int i); 	// the currently hacking marine has chosen option i on this hack
	virtual int GetOptionTypeForEntry(int iOption);
	CASW_Computer_Area* GetComputerArea();

	virtual bool IsDownloadingFiles();
	virtual bool IsPDA();
	virtual int GetMailOption();
	virtual void SetDefaultHackOption();
	virtual void OnHackUnlocked( CASW_Marine *pMarine );

	virtual bool InitHack(CASW_Player* pHackingPlayer, CASW_Marine* pHackingMarine, CBaseEntity* pHackTarget);
	bool m_bSetupComputer;
	int m_iLastNumWrong;
	void UpdateCorrectStatus(CASW_Player *pPlayer, CASW_Marine *pMarine, int iNumWrong);

	CNetworkVar(float, m_fFastFinishTime);
	bool m_bPlayedTimeOutSound;
};

// hack option types

enum {
	ASW_COMPUTER_OPTION_TYPE_NONE = 0,
	ASW_COMPUTER_OPTION_TYPE_DOWNLOAD_DOCS,
	ASW_COMPUTER_OPTION_TYPE_SECURITY_CAM_1,
	ASW_COMPUTER_OPTION_TYPE_SECURITY_CAM_2,
	ASW_COMPUTER_OPTION_TYPE_SECURITY_CAM_3,
	ASW_COMPUTER_OPTION_TYPE_TURRET_1,
	ASW_COMPUTER_OPTION_TYPE_TURRET_2,
	ASW_COMPUTER_OPTION_TYPE_TURRET_3,
	ASW_COMPUTER_OPTION_TYPE_MAIL,
	ASW_COMPUTER_OPTION_TYPE_NEWS,
	ASW_COMPUTER_OPTION_TYPE_STOCKS,
	ASW_COMPUTER_OPTION_TYPE_WEATHER,
	ASW_COMPUTER_OPTION_TYPE_PLANT,	
	ASW_COMPUTER_OPTION_TYPE_OVERRIDE,
};

#endif /* _DEFINED_ASW_HACK_COMPUTER_H */