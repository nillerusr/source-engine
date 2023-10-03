//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef FLASHLIGHTEFFECT_H
#define FLASHLIGHTEFFECT_H
#ifdef _WIN32
#pragma once
#endif

struct dlight_t;


class CFlashlightEffect
{
public:

	CFlashlightEffect(int nEntIndex = 0, const char *pszTextureName = NULL, float flFov = 0.0f, float flFarZ = 0.0f, float flLinearAtten = 0.0f );
	~CFlashlightEffect();

	void UpdateLight( int nEntIdx, const Vector &vecPos, const Vector &vecDir, const Vector &vecRight, const Vector &vecUp, float flFov, 
						float flFarZ, float flLinearAtten, bool castsShadows, const char* pTextureName );
	void UpdateLight(	int nEntIdx, const Vector &vecPos, const Vector &vecDir, const Vector &vecRight, const Vector &vecUp, float flFov,
						bool castsShadows, ITexture *pFlashlightTexture, const Vector &vecBrightness, bool bTracePlayers = true );

	void TurnOn();
	void TurnOff();
	void SetMuzzleFlashEnabled( bool bEnabled, float flBrightness );
	bool IsOn( void ) { return m_bIsOn;	}

	ClientShadowHandle_t GetFlashlightHandle( void ) { return m_FlashlightHandle; }
	void SetFlashlightHandle( ClientShadowHandle_t Handle ) { m_FlashlightHandle = Handle;	}

	const char *GetFlashlightTextureName( void ) const
	{
		return m_textureName;
	}

	int GetEntIndex( void ) const
	{
		return m_nEntIndex;
	}

protected:

	bool UpdateDefaultFlashlightState(	FlashlightState_t& state, const Vector &vecPos, const Vector &vecDir, const Vector &vecRight,
										const Vector &vecUp, bool castsShadows, bool bTracePlayers = true );
	bool ComputeLightPosAndOrientation( const Vector &vecPos, const Vector &vecDir, const Vector &vecRight, const Vector &vecUp,
										Vector& vecFinalPos, Quaternion& quatOrientation, bool bTracePlayers );
	void LightOff();

	void UpdateFlashlightTexture( const char* pTextureName );
	void UpdateLightTopDown(const Vector &vecPos, const Vector &vecDir, const Vector &vecRight, const Vector &vecUp);

	bool m_bIsOn;
	int m_nEntIndex;
	ClientShadowHandle_t m_FlashlightHandle;

	bool m_bMuzzleFlashEnabled;
	float m_flMuzzleFlashBrightness;

	float m_flFov;
	float m_flFarZ;
	float m_flLinearAtten;
	bool  m_bCastsShadows;

	float m_flCurrentPullBackDist;

	// Texture for flashlight
	CTextureReference m_FlashlightTexture;

	// Texture for muzzle flash
	CTextureReference m_MuzzleFlashTexture;

	char m_textureName[64];
};

class CHeadlightEffect : public CFlashlightEffect
{
public:
	
	CHeadlightEffect();
	~CHeadlightEffect();

	virtual void UpdateLight(const Vector &vecPos, const Vector &vecDir, const Vector &vecRight, const Vector &vecUp, int nDistance);
};

class CFlashlightEffectManager
{
private:
	CFlashlightEffect *m_pFlashlightEffect;
	const char *m_pFlashlightTextureName;
	int m_nFlashlightEntIndex;
	float m_flFov;
	float m_flFarZ;
	float m_flLinearAtten;
	int m_nMuzzleFlashFrameCountdown;
	CountdownTimer m_muzzleFlashTimer;
	float m_flMuzzleFlashBrightness;
	bool m_bFlashlightOn;
	int m_nFXComputeFrame;
	bool m_bFlashlightOverride;

public:
	CFlashlightEffectManager() : m_pFlashlightEffect( NULL ), m_pFlashlightTextureName( NULL ), m_nFlashlightEntIndex( -1 ), m_flFov( 0.0f ),
								m_flFarZ( 0.0f ), m_flLinearAtten( 0.0f ), m_nMuzzleFlashFrameCountdown( 0 ), m_flMuzzleFlashBrightness( 1.0f ),
								m_bFlashlightOn( false ), m_nFXComputeFrame( -1 ), m_bFlashlightOverride( false ) {}

	void TurnOnFlashlight( int nEntIndex = 0, const char *pszTextureName = NULL, float flFov = 0.0f, float flFarZ = 0.0f, float flLinearAtten = 0.0f )
	{
		m_pFlashlightTextureName = pszTextureName;
		m_nFlashlightEntIndex = nEntIndex;
		m_flFov = flFov;
		m_flFarZ = flFarZ;
		m_flLinearAtten = flLinearAtten;
		m_bFlashlightOn = true;

		if ( m_bFlashlightOverride )
		{
			// somebody is overriding the flashlight. We're keeping around the params to restore it later.
			return;
		}

		if ( !m_pFlashlightEffect )
		{
			if( pszTextureName )
			{
				m_pFlashlightEffect = new CFlashlightEffect( m_nFlashlightEntIndex, pszTextureName, flFov, flFarZ, flLinearAtten );
			}
			else
			{
				m_pFlashlightEffect = new CFlashlightEffect( m_nFlashlightEntIndex );
			}

			if( !m_pFlashlightEffect )
			{
				return;
			}
		}

		m_pFlashlightEffect->TurnOn();
	}

	void TurnOffFlashlight( bool bForce = false )
	{
		m_pFlashlightTextureName = NULL;
		m_bFlashlightOn = false;

		if ( bForce )
		{
			m_bFlashlightOverride = false;
			m_nMuzzleFlashFrameCountdown = 0;
			m_muzzleFlashTimer.Invalidate();
			delete m_pFlashlightEffect;
			m_pFlashlightEffect = NULL;
			return;
		}

		if ( m_bFlashlightOverride )
		{
			// don't mess with it while it's overridden
			return;
		}

		if( m_nMuzzleFlashFrameCountdown == 0 && m_muzzleFlashTimer.IsElapsed() )
		{
			delete m_pFlashlightEffect;
			m_pFlashlightEffect = NULL;
		}
	}

	bool IsFlashlightOn() const { return m_bFlashlightOn; }

	void UpdateFlashlight( const Vector &vecPos, const Vector &vecDir, const Vector &vecRight, const Vector &vecUp, float flFov, bool castsShadows,
		float flFarZ, float flLinearAtten, const char* pTextureName = NULL )
	{
		if ( m_bFlashlightOverride )
		{
			// don't mess with it while it's overridden
			return;
		}

		bool bMuzzleFlashActive = ( m_nMuzzleFlashFrameCountdown > 0 ) || !m_muzzleFlashTimer.IsElapsed();

		if ( m_pFlashlightEffect )
		{
			m_flFov = flFov;
			m_flFarZ = flFarZ;
			m_flLinearAtten = flLinearAtten;
			m_pFlashlightEffect->UpdateLight( m_nFlashlightEntIndex, vecPos, vecDir, vecRight, vecUp, flFov, flFarZ, flLinearAtten, castsShadows, pTextureName );
			m_pFlashlightEffect->SetMuzzleFlashEnabled( bMuzzleFlashActive, m_flMuzzleFlashBrightness );
		}

		if ( !bMuzzleFlashActive && !m_bFlashlightOn && m_pFlashlightEffect )
		{
			delete m_pFlashlightEffect;
			m_pFlashlightEffect = NULL;
		}

		if ( bMuzzleFlashActive && !m_bFlashlightOn && !m_pFlashlightEffect )
		{
			m_pFlashlightEffect = new CFlashlightEffect( m_nFlashlightEntIndex );
			m_pFlashlightEffect->SetMuzzleFlashEnabled( bMuzzleFlashActive, m_flMuzzleFlashBrightness );
		}

		if ( bMuzzleFlashActive && m_nFXComputeFrame != gpGlobals->framecount )
		{
			m_nFXComputeFrame = gpGlobals->framecount;
			m_nMuzzleFlashFrameCountdown--;
		}
	}

	void SetEntityIndex( int index )
	{
		m_nFlashlightEntIndex = index;
	}

	void TriggerMuzzleFlash()
	{
		m_nMuzzleFlashFrameCountdown = 2;
		m_muzzleFlashTimer.Start( 0.066f );		// show muzzleflash for 2 frames or 66ms, whichever is longer
		m_flMuzzleFlashBrightness = random->RandomFloat( 0.4f, 2.0f );
	}

	const char *GetFlashlightTextureName( void ) const
	{
		return m_pFlashlightTextureName;
	}

	int GetFlashlightEntIndex( void ) const
	{
		return m_nFlashlightEntIndex;
	}

	void EnableFlashlightOverride( bool bEnable )
	{
		m_bFlashlightOverride = bEnable;

		if ( !m_bFlashlightOverride )
		{
			// make sure flashlight is in its original state
			if ( m_bFlashlightOn && m_pFlashlightEffect == NULL )
			{
				TurnOnFlashlight( m_nFlashlightEntIndex, m_pFlashlightTextureName, m_flFov, m_flFarZ, m_flLinearAtten );
			}
			else if ( !m_bFlashlightOn && m_pFlashlightEffect )
			{
				delete m_pFlashlightEffect;
				m_pFlashlightEffect = NULL;
			}
		}
	}

	void UpdateFlashlightOverride(	bool bFlashlightOn, const Vector &vecPos, const Vector &vecDir, const Vector &vecRight, const Vector &vecUp,
									float flFov, bool castsShadows, ITexture *pFlashlightTexture, const Vector &vecBrightness )
	{
		Assert( m_bFlashlightOverride );
		if ( !m_bFlashlightOverride )
		{
			return;
		}

		if ( bFlashlightOn && !m_pFlashlightEffect )
		{
			m_pFlashlightEffect = new CFlashlightEffect( m_nFlashlightEntIndex );			
		}
		else if ( !bFlashlightOn && m_pFlashlightEffect )
		{
			delete m_pFlashlightEffect;
			m_pFlashlightEffect = NULL;
		}

		if( m_pFlashlightEffect )
		{
			m_pFlashlightEffect->UpdateLight( m_nFlashlightEntIndex, vecPos, vecDir, vecRight, vecUp, flFov, castsShadows, pFlashlightTexture, vecBrightness, false );
		}
	}
};

CFlashlightEffectManager & FlashlightEffectManager( int32 nSplitscreenPlayerOverride = -1 );

#endif // FLASHLIGHTEFFECT_H
