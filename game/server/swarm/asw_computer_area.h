#ifndef _DEFINED_ASW_COMPUTER_AREA_H
#define _DEFINED_ASW_COMPUTER_AREA_H

#include "asw_use_area.h"
#include "asw_shareddefs.h"

class CASW_Player;
class CASW_Hack_Computer;
class CASW_Marine;

class CASW_Computer_Area : public CASW_Use_Area
{
	DECLARE_CLASS( CASW_Computer_Area, CASW_Use_Area );
public:
	CASW_Computer_Area();
	virtual void Spawn( void );
	virtual void Precache();
	void FindTurretsAndCams();
	void ActivateUnlockedComputer(CASW_Marine* pMarine);
	bool KeyValue( const char *szKeyName, const char *szValue );
	CASW_Hack_Computer* GetCurrentHack();
	virtual bool IsLocked() { return m_bIsLocked.Get(); }
	virtual bool HasDownloadObjective();

	Class_T		Classify( void ) { return (Class_T) CLASS_ASW_COMPUTER_AREA; }

	static bool WaitingForInputVismonEvaluator( CBaseEntity *pVisibleEntity, CBasePlayer *pViewingPlayer );
	static bool WaitingForInputVismonCallback( CBaseEntity *pVisibleEntity, CBasePlayer *pViewingPlayer );
	
	void OnComputerDataDownloaded( CASW_Marine *pMarine );		
	void Override( CASW_Marine *pMarine );

	// viewing mail
	void OnViewMail(CASW_Marine *pMarine, int iMail);
	bool m_bViewingMail;

	virtual void ActivateMultiTrigger(CBaseEntity *pActivator);

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CNetworkVar( int,  m_iHackLevel );	
	CNetworkVar( float, m_fDownloadTime );
	CNetworkVar( bool,  m_bIsLocked );	
	CNetworkVar( bool, m_bWaitingForInput );
	EHANDLE m_hComputerHack;

	string_t		m_SecurityCam1Name;
	string_t		m_SecurityCam2Name;
	string_t		m_SecurityCam3Name;
	string_t		m_Turret1Name;	

	CNetworkString( m_MailFile, 255 );
	CNetworkString( m_NewsFile, 255 );
	CNetworkString( m_StocksSeed, 255 );
	CNetworkString( m_WeatherSeed, 255 );
	CNetworkString( m_PlantFile, 255 );
	CNetworkString( m_PDAName, 255 );

	CNetworkString( m_SecurityCamLabel1, 255 );
	CNetworkString( m_SecurityCamLabel2, 255 );
	CNetworkString( m_SecurityCamLabel3, 255 );
	CNetworkString( m_TurretLabel1, 255 );
	CNetworkString( m_TurretLabel2, 255 );
	CNetworkString( m_TurretLabel3, 255 );

	CNetworkString( m_DownloadObjectiveName, 255 );
	CNetworkVar( bool, m_bDownloadedDocs );

	CNetworkVar( bool, m_bSecurityCam1Locked );
	CNetworkVar( bool, m_bSecurityCam2Locked );
	CNetworkVar( bool, m_bSecurityCam3Locked );
	CNetworkVar( bool, m_bTurret1Locked );	
	CNetworkVar( bool, m_bMailFileLocked );
	CNetworkVar( bool, m_bNewsFileLocked );
	CNetworkVar( bool, m_bStocksFileLocked );
	CNetworkVar( bool, m_bWeatherFileLocked );
	CNetworkVar( bool, m_bPlantFileLocked );
	
	CNetworkHandle( CBaseEntity, m_hSecurityCam1 );	
	CNetworkHandle( CBaseEntity, m_hTurret1 );	

	CNetworkVar( int, m_iActiveCam );

	// outputs
	COutputEvent m_OnComputerHackStarted;	
	COutputEvent m_OnComputerHackHalfway;
	COutputEvent m_OnComputerHackCompleted;
	COutputEvent m_OnComputerActivated;
	COutputEvent m_OnComputerDataDownloaded;
	COutputEvent m_OnComputerViewMail1;
	COutputEvent m_OnComputerViewMail2;
	COutputEvent m_OnComputerViewMail3;
	COutputEvent m_OnComputerViewMail4;

	// properties of the tumbler hack
	int m_iNumEntriesPerTumbler;
	float m_fMoveInterval;	
	
	virtual void ActivateUseIcon( CASW_Marine* pMarine, int nHoldType );
	virtual void MarineUsing(CASW_Marine* pMarine, float deltatime);
	virtual void MarineStartedUsing(CASW_Marine* pMarine);
	virtual void MarineStoppedUsing(CASW_Marine* pMarine);
	virtual void UnlockFromHack(CASW_Marine* pMarine);
	virtual void HackHalfway(CASW_Marine* pMarine);
	virtual bool IsWaitingForInput( void ) const { return m_bWaitingForInput; }

	virtual int GetNumMenuOptions();
	float GetHackProgress() { return m_fHackProgress; }
	
	CNetworkVar(bool, m_bIsInUse);
	CNetworkVar(float, m_fHackProgress);
	bool m_bWasLocked;
	bool m_bUseAfterHack;	
	float m_fAutoOverrideTime;
	float m_fLastButtonUseTime;
	int m_iAliensKilledBeforeHack;
	float m_flStopUsingTime;		// time at which to stop the marine using this computer

	// sound related
	void StopLoopingSounds();
	void PlayPositiveSound(CASW_Player *pHackingPlayer);
	void PlayNegativeSound(CASW_Player *pHackingPlayer);
	float m_fLastPositiveSoundTime;
	bool m_bPlayedHalfwayChatter;
	bool m_bDoSecureShout;
	float m_fNextSecureShoutCheck;
	void StartDownloadingSound();
	void StopDownloadingSound();
	CSoundPatch	*m_pDownloadingSound;
	CSoundPatch *m_pComputerInUseSound;

	virtual void UpdateWaitingForInput();
	virtual void UpdatePanelSkin();
};

#endif /* _DEFINED_ASW_COMPUTER_AREA_H */