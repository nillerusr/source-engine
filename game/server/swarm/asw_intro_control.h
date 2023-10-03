#ifndef _INCLUDED_ASW_INTRO_CONTROL_H
#define _INCLUDED_ASW_INTRO_CONTROL_H
#pragma once

#include "baseentity.h"

// Used by the intro to launch the campaign map once the intro is over

class CASW_Player;

class CASW_Intro_Control : public CLogicalEntity
{
public:
	DECLARE_CLASS( CASW_Intro_Control, CLogicalEntity );
	DECLARE_DATADESC();

	void InputLaunchCampaignMap( inputdata_t &inputdata );
	void InputShowCredits( inputdata_t &inputdata );
	void InputShowCainMail( inputdata_t &inputdata );
	void InputCheckReconnect( inputdata_t &inputdata );	
	void OnIntroStarted();
	void CheckReconnect();
	void LaunchCampaignMap();
	void PlayerSpawned(CASW_Player *pPlayer);
	virtual void Spawn();

	bool m_bLaunchedCampaignMap;
	bool m_bShownCredits;
	COutputEvent m_IntroStarted;
};

#endif /* _INCLUDED_ASW_INTRO_CONTROL_H */