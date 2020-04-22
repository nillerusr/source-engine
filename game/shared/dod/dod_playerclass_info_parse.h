//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef DOD_PLAYERCLASS_INFO_PARSE_H
#define DOD_PLAYERCLASS_INFO_PARSE_H
#ifdef _WIN32
#pragma once
#endif


#include "playerclass_info_parse.h"
#include "networkvar.h"
#include "dod_shareddefs.h"

//--------------------------------------------------------------------------------------------------------
class CDODPlayerClassInfo : public FilePlayerClassInfo_t
{
public:
	DECLARE_CLASS_GAMEROOT( CDODPlayerClassInfo, FilePlayerClassInfo_t );
	
	CDODPlayerClassInfo();
	
	virtual void Parse( ::KeyValues *pKeyValuesData, const char *szWeaponName );

	int m_iTeam;		//which team. 2 == allies, 3 == axis

	int m_iPrimaryWeapon;
	int m_iSecondaryWeapon;
	int m_iMeleeWeapon;

	int m_iNumGrensType1;
	int m_iGrenType1;

	int m_iNumGrensType2;
	int m_iGrenType2;

	int m_iNumBandages;

	int m_iHelmetGroup;
	int m_iHairGroup;	//what helmet group to switch to when the helmet comes off

	int m_iDropHelmet;

	char m_szLimitCvar[64];	//which cvar controls the class limit for this class
	bool m_bClassLimitMGMerge;	// merge class limits with this set to true

	char m_szClassHealthImage[DOD_HUD_HEALTH_IMAGE_LENGTH];
	char m_szClassHealthImageBG[DOD_HUD_HEALTH_IMAGE_LENGTH];
};


#endif // DOD_PLAYERCLASS_INFO_PARSE_H
