//========= Copyright Valve Corporation, All rights reserved. ============//
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

#ifdef EZ
enum flashlighttype
{
	FLASHLIGHT,
	NVG,
	MUZZLEFLASH,
	HEADLAMP
};
#endif

class CFlashlightEffect
{
public:

#ifdef EZ
	CFlashlightEffect( int nEntIndex = 0, flashlighttype type = FLASHLIGHT );
#else
	CFlashlightEffect(int nEntIndex = 0);
#endif
	virtual ~CFlashlightEffect();

	virtual void UpdateLight(const Vector &vecPos, const Vector &vecDir, const Vector &vecRight, const Vector &vecUp, int nDistance);
	void TurnOn();
	void TurnOff();
	bool IsOn( void ) { return m_bIsOn;	}

	ClientShadowHandle_t GetFlashlightHandle( void ) { return m_FlashlightHandle; }
	void SetFlashlightHandle( ClientShadowHandle_t Handle ) { m_FlashlightHandle = Handle;	}
	
#ifdef EZ
	virtual bool IsNVG( void ) { return m_iFlashLightType == NVG; }
#endif
protected:

	void LightOff();
	void LightOffOld();
	void LightOffNew();

	void UpdateLightNew(const Vector &vecPos, const Vector &vecDir, const Vector &vecRight, const Vector &vecUp);
	void UpdateLightOld(const Vector &vecPos, const Vector &vecDir, int nDistance);

	bool m_bIsOn;
	int m_nEntIndex;
	ClientShadowHandle_t m_FlashlightHandle;

	// Vehicle headlight dynamic light pointer
	dlight_t *m_pPointLight;
	float m_flDistMod;

	// Texture for flashlight
	CTextureReference m_FlashlightTexture;

#ifdef EZ
	flashlighttype m_iFlashLightType;
#endif
};

class CHeadlightEffect : public CFlashlightEffect
{
public:
	
	CHeadlightEffect();
	~CHeadlightEffect();

	virtual void UpdateLight(const Vector &vecPos, const Vector &vecDir, const Vector &vecRight, const Vector &vecUp, int nDistance);
};

#ifdef EZ2
class CTurretLightEffect : public CFlashlightEffect
{
public:

	CTurretLightEffect();
	~CTurretLightEffect();

	virtual void UpdateLight( const Vector &vecPos, const Vector &vecDir, const Vector &vecRight, const Vector &vecUp, int nDistance );

	float	m_flBrightnessScale;
	float	m_Color[3];
	float	m_flFarZ;
	float	m_flFOV;
};

class CCrabSynthLightEffect : public CFlashlightEffect
{
public:

	CCrabSynthLightEffect();
	~CCrabSynthLightEffect();

	virtual void UpdateLight( const Vector &vecPos, const Vector &vecDir, const Vector &vecRight, const Vector &vecUp, int nDistance );

	bool	m_bShadowsEnabled;
	float	m_flBrightnessScale;
	float	m_flFOV;
	float	m_flHorzFOV;
	float	m_Color[3];
};
#endif

#endif // FLASHLIGHTEFFECT_H
