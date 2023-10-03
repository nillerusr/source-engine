//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Color correction entity.
//
// $NoKeywords: $
//===========================================================================//

#ifndef COLOR_CORRECTION_H
#define COLOR_CORRECTION_H
#ifdef _WIN32
#pragma once
#endif

#include <string.h>
#include "cbase.h"
#include "GameEventListener.h"

// Spawn Flags
#define SF_COLORCORRECTION_MASTER		0x0001
#define SF_COLORCORRECTION_CLIENTSIDE	0x0002

//------------------------------------------------------------------------------
// FIXME: This really should inherit from something	more lightweight
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// Purpose : Shadow control entity
//------------------------------------------------------------------------------
class CColorCorrection : public CBaseEntity
{
	DECLARE_CLASS( CColorCorrection, CBaseEntity );
public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CColorCorrection();

	void Spawn( void );
	int  UpdateTransmitState();
	void Activate( void );

	virtual int	ObjectCaps( void ) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	bool IsMaster( void ) const { return HasSpawnFlags( SF_COLORCORRECTION_MASTER ); }

	bool IsClientSide( void ) const { return HasSpawnFlags( SF_COLORCORRECTION_CLIENTSIDE ); }

	bool IsExclusive( void ) const { return m_bExclusive; }

	// Inputs
	void	InputEnable( inputdata_t &inputdata );
	void	InputDisable( inputdata_t &inputdata );
	void	InputSetFadeInDuration ( inputdata_t &inputdata );
	void	InputSetFadeOutDuration ( inputdata_t &inputdata );

private:
	void	FadeIn ( void );
	void	FadeOut ( void );

	void FadeInThink( void );	// Fades lookup weight from Cur->MaxWeight 
	void FadeOutThink( void );	// Fades lookup weight from CurWeight->0.0

	
	
	CNetworkVar( float, m_flFadeInDuration );	// Duration for a full 0->MaxWeight transition
	CNetworkVar( float, m_flFadeOutDuration );	// Duration for a full Max->0 transition
	float	m_flStartFadeInWeight;
	float	m_flStartFadeOutWeight;
	float	m_flTimeStartFadeIn;
	float	m_flTimeStartFadeOut;
	
	CNetworkVar( float, m_flMaxWeight );

	bool	m_bStartDisabled;
	CNetworkVar( bool, m_bEnabled );
	CNetworkVar( bool, m_bMaster );
	CNetworkVar( bool, m_bClientSide );
	CNetworkVar( bool, m_bExclusive );

	CNetworkVar( float, m_MinFalloff );
	CNetworkVar( float, m_MaxFalloff );
	CNetworkVar( float, m_flCurWeight );
	CNetworkString( m_netlookupFilename, MAX_PATH );

	string_t	m_lookupFilename;
};

//=============================================================================
//
// ColorCorrection Controller System. Just a place to store a master controller
//
class CColorCorrectionSystem : public CAutoGameSystem, public CGameEventListener
{
public:

	// Creation/Init.
	CColorCorrectionSystem( char const *name ) : CAutoGameSystem( name ) 
	{
		m_hMasterController = NULL;
	}

	~CColorCorrectionSystem()
	{
		m_hMasterController = NULL;
	}

	virtual void LevelInitPreEntity();
	virtual void LevelInitPostEntity();
	virtual void FireGameEvent( IGameEvent *pEvent );
	CColorCorrection *GetMasterColorCorrection( void )			{ return m_hMasterController; }

private:

	void InitMasterController( void );
	CHandle< CColorCorrection > m_hMasterController;
};

CColorCorrectionSystem *ColorCorrectionSystem( void );

#endif // COLOR_CORRECTION_H