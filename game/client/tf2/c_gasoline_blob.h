//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_GASOLINE_BLOB_H
#define C_GASOLINE_BLOB_H
#ifdef _WIN32
#pragma once
#endif


#include "c_baseentity.h"
#include "particles_simple.h"
#include "particle_util.h"


class C_GasolineBlob;

class CGasolineEmitter : public CSimpleEmitter
{
public:

	static CSmartPtr<CGasolineEmitter>	Create( C_GasolineBlob *pBlob );

	void				UpdateFire( float frametime );


private:

						CGasolineEmitter() : CSimpleEmitter( "Gasoline" ){}
						CGasolineEmitter( const CGasolineEmitter & );

	
	C_GasolineBlob		*m_pBlob;

	PMaterialHandle		m_hFireMaterial;
	PMaterialHandle		m_hUnlitMaterial;
	TimedEvent			m_Timer;
};


class C_GasolineBlob : public C_BaseEntity
{
friend class CGasolineEmitter;

public:
	DECLARE_CLASS( C_GasolineBlob, C_BaseEntity );
	DECLARE_CLIENTCLASS();


					C_GasolineBlob();
	virtual			~C_GasolineBlob();

	bool			IsLit() const;
	bool			IsStopped() const;
	const Vector&	GetSurfaceNormal() const;
	float			GetLitStartTime() const;


// Overrides.
public:

	virtual void	OnDataChanged( DataUpdateType_t type );
	virtual void	ClientThink();
	virtual int		DrawModel( int flags );
	virtual bool	ShouldDraw();


private:

	// Returns true if the two blobs relate their sound, meaning one blob won't play
	// its sound if the other one is playing it.
	bool			IsSoundRelatedTo( const C_GasolineBlob *pBlob ) const;

	bool			IsPlayingBurningSound() const;

	// Starts the burning sound if no other flames are playing the sound nearby.
	void			CheckStartSound();

	// Make the burning sound.
	void			StartSound();
	void			StopSound();


private:

	bool			m_bSoundOn;
	float			m_flPuddleSize;
	float			m_flPuddleFade;

	CSmartPtr<CGasolineEmitter>	m_pEmitter;

	float			m_flLitStartTime;

	float			m_flCreateTime;
	float			m_flMaxLifetime;

	Vector			m_vSurfaceNormal;

	int				m_BlobFlags;		// Combination of BLOBFLAG_ defines.
};


#endif // C_GASOLINE_BLOB_H
