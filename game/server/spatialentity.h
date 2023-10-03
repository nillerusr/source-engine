//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Spatial entity.
//
// $NoKeywords: $
//===========================================================================//

#ifndef SPATIALENTITY_H
#define SPATIALENTITY_H

#ifdef _WIN32
#pragma once
#endif

//------------------------------------------------------------------------------
// Purpose : Spatial entity
//------------------------------------------------------------------------------
class CSpatialEntity : public CBaseEntity
{
	DECLARE_CLASS( CSpatialEntity, CBaseEntity );
public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CSpatialEntity();

	void Spawn( void );
	int  UpdateTransmitState();

	virtual int	ObjectCaps( void ) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

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



	float	m_flFadeInDuration;		// Duration for a full 0->MaxWeight transition
	float	m_flFadeOutDuration;	// Duration for a full Max->0 transition
	float	m_flStartFadeInWeight;
	float	m_flStartFadeOutWeight;
	float	m_flTimeStartFadeIn;
	float	m_flTimeStartFadeOut;

	float	m_flMaxWeight;

	bool	m_bStartDisabled;
	CNetworkVar( bool, m_bEnabled );

	CNetworkVar( float, m_MinFalloff );
	CNetworkVar( float, m_MaxFalloff );
	CNetworkVar( float, m_flCurWeight );

	string_t	m_lookupFilename;
};

#endif // SPATIALENTITY_H