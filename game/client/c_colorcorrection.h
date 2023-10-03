//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Color correction entity with simple radial falloff
//
// $NoKeywords: $
//===========================================================================//

#ifndef C_COLORCORRECTION_H
#define C_COLORCORRECTION_H
#ifdef _WIN32
#pragma once
#endif

#include "colorcorrectionmgr.h"

//------------------------------------------------------------------------------
// Purpose : Color correction entity with radial falloff
//------------------------------------------------------------------------------
class C_ColorCorrection : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_ColorCorrection, C_BaseEntity );

	DECLARE_CLIENTCLASS();

	C_ColorCorrection();
	virtual ~C_ColorCorrection();

	void OnDataChanged(DataUpdateType_t updateType);
	bool ShouldDraw();

	virtual void Update(C_BasePlayer *pPlayer, float ccScale);
	
	bool IsMaster() const { return m_bMaster; }
	bool IsClientSide() const;
	bool IsExclusive() const { return m_bExclusive; }

	void EnableOnClient( bool bEnable, bool bSkipFade = false );

	Vector GetOrigin();
	float  GetMinFalloff();
	float  GetMaxFalloff();

	void   SetWeight( float fWeight );

protected:
	void StartFade( int nSplitScreenSlot, float flDuration );
	float GetFadeRatio( int nSplitScreenSlot ) const;
	bool IsFadeTimeElapsed( int nSplitScreenSlot ) const;

	Vector	m_vecOrigin;

	float	m_minFalloff;
	float	m_maxFalloff;
	float	m_flFadeInDuration;
	float	m_flFadeOutDuration;
	float	m_flMaxWeight;
	float	m_flCurWeight;		// networked from server
	char	m_netLookupFilename[MAX_PATH];

	bool	m_bEnabled;			// networked from server
	bool	m_bMaster;
	bool	m_bClientSide;
	bool	m_bExclusive;

	bool	m_bEnabledOnClient[MAX_SPLITSCREEN_PLAYERS];
	float	m_flCurWeightOnClient[MAX_SPLITSCREEN_PLAYERS];
	bool	m_bFadingIn[MAX_SPLITSCREEN_PLAYERS];
	float	m_flFadeStartWeight[MAX_SPLITSCREEN_PLAYERS];
	float	m_flFadeStartTime[MAX_SPLITSCREEN_PLAYERS];
	float	m_flFadeDuration[MAX_SPLITSCREEN_PLAYERS];

	ClientCCHandle_t m_CCHandle;
};

#endif