#ifndef _INCLUDED_ASW_SHOTGUN_PREDICTED_SHARED_H
#define _INCLUDED_ASW_SHOTGUN_PREDICTED_SHARED_H
#ifdef _WIN32
#pragma once
#endif

#if defined( CLIENT_DLL )
#define CASW_Shotgun_Pellet_Predicted C_ASW_Shotgun_Pellet_Predicted
#endif

class CSprite;

//-----------------------------------------------------------------------------
// Experimental predicted shotgun pellet.
//   (Not currently used as it didn't work very well)
//-----------------------------------------------------------------------------
class CASW_Shotgun_Pellet_Predicted : public CBaseGrenade
{
	DECLARE_CLASS( CASW_Shotgun_Pellet_Predicted, CBaseGrenade );
public:
	CASW_Shotgun_Pellet_Predicted();

	DECLARE_PREDICTABLE();
	DECLARE_NETWORKCLASS();

	virtual void	Spawn( void );
	virtual void	Precache( void );
	virtual void	UpdateOnRemove( void );
	virtual void	BounceTouch( CBaseEntity *pOther );
	virtual void PelletTouch( CBaseEntity *pOther );
	virtual void	BounceSound( void );
	virtual float	GetShakeAmplitude( void );
	virtual int		GetDamageType() const { return DMG_BLAST; }

	void	ApplyRadiusEMPEffect( CBaseEntity *pOwner, const Vector& vecCenter );
	
	static CASW_Shotgun_Pellet_Predicted *CASW_Shotgun_Pellet_Predicted::CreatePellet( const Vector &vecOrigin, const Vector &vecForward, CBasePlayer *pCommander, CBaseEntity *pMarine );

	// A derived class should return true here so that weapon sounds, etc, can
	//  apply the proper filter
	virtual bool			IsPredicted( void ) const
	{ 
		return true;
	}

#if defined( CLIENT_DLL )
	virtual bool	ShouldPredict( void )
	{
		if ( m_pCommander == C_BasePlayer::GetLocalPlayer() )
			return true;

		return BaseClass::ShouldPredict();
	}

	virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual void	ClientThink( void );

	TimedEvent		m_ParticleEvent;

	virtual int			DrawModel( int flags, const RenderableInstance_t &instance );

#endif

	void SetCommander(CBasePlayer *pPlayer) { m_pCommander = pPlayer; }

	float m_flDamage;
	CBaseEntity *m_pLastHit;
private:
	CNetworkHandle( CSprite, m_hLiveSprite );

	CBasePlayer *m_pCommander;

private:
	CASW_Shotgun_Pellet_Predicted( const CASW_Shotgun_Pellet_Predicted & );
};

#endif // _INCLUDED_ASW_SHOTGUN_PREDICTED_SHARED_H
