//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef ENV_LASERDESIGNATION_H
#define ENV_LASERDESIGNATION_H
#pragma once

#if defined( CLIENT_DLL )
#define CEnvLaserDesignation C_EnvLaserDesignation
#endif

//-----------------------------------------------------------------------------
// Purpose: A laser designation point
//-----------------------------------------------------------------------------
class CEnvLaserDesignation : public CBaseAnimating
{
public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_CLASS( CEnvLaserDesignation, CBaseAnimating );

	CEnvLaserDesignation( void );
	~CEnvLaserDesignation( void );

	virtual void Spawn( void );
	virtual void ChangeTeam( int iTeamNum );

	// Designation
	void	SetActive( bool bActive );
	bool	IsActive( void ) { return m_bActive; }

		// All predicted weapons need to implement and return true
	virtual bool			IsPredicted( void ) const
	{ 
		return true;
	}


#if defined( CLIENT_DLL )
	virtual bool	ShouldPredict( void )
	{
		if ( GetOwnerEntity() == C_BasePlayer::GetLocalPlayer() )
			return true;

		return BaseClass::ShouldPredict();
	}

	DECLARE_ENTITY_PANEL();

	virtual void OnDataChanged( DataUpdateType_t updateType );
	virtual void SetDormant( bool bDormant );

	virtual int		DrawModel( int flags );
#else
	virtual int		UpdateTransmitState();	
	virtual int		ShouldTransmit( const CCheckTransmitInfo *pInfo );
#endif

	// Global Designator access
	static CEnvLaserDesignation *Create( CBasePlayer *pOwner );
	static CEnvLaserDesignation *CreatePredicted( CBasePlayer *pOwner );
	static int		GetNumLaserDesignators( int iTeamNumber );
	static bool		GetLaserDesignation( int iTeamNumber, int iDesignator, Vector *vecOrigin );

protected:
	static CUtlVector< EHANDLE >	m_LaserDesignatorsTeam1;
	static CUtlVector< EHANDLE >	m_LaserDesignatorsTeam2;

	CNetworkVar( bool, m_bActive );

	bool	m_bPrevActive;
private:
	CEnvLaserDesignation( const CEnvLaserDesignation& src );
};

#endif // ENV_LASERDESIGNATION_H
