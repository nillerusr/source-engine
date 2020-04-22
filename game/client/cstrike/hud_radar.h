//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef HUD_RADAR_H
#define HUD_RADAR_H
#ifdef _WIN32
#pragma once
#endif

#include "hudelement.h"
#include <vgui_controls/Panel.h>
#include <vgui_controls/Label.h>

class C_CSPlayer;
void Radar_FlashPlayer( int iPlayer );

class CPlayerRadarFlash
{
public:
	CPlayerRadarFlash()
	{
		m_flNextRadarFlashTime = 0.0f;
		m_iNumRadarFlashes = 0;
		m_bRadarFlash = false;
	}
	float m_flNextRadarFlashTime;	// when to next toggle the flash on the radar
	int	m_iNumRadarFlashes;			// how many flashes more to do
	bool m_bRadarFlash;				// flash or do not, there is no try
};


class CHudRadar : public CHudElement, public vgui::Panel
{
public:
	DECLARE_CLASS_SIMPLE( CHudRadar, vgui::Panel );

	CHudRadar( const char *name );
	~CHudRadar();

	virtual void Paint();
	virtual void Init();
	virtual void LevelInit();
	virtual bool ShouldDraw();
	virtual void SetVisible(bool state);
	virtual void Reset();

	void DrawRadar(void) { m_bHideRadar = false; }
	void HideRadar(void) { m_bHideRadar = true; }
	void MsgFunc_UpdateRadar(bf_read &msg );

private:

	void WorldToRadar( const Vector location, const Vector origin, const QAngle angles, float &x, float &y, float &z_delta );

	void DrawPlayerOnRadar( int iPlayer, C_CSPlayer *pLocalPlayer );
	void DrawEntityOnRadar( CBaseEntity *pEnt, C_CSPlayer *pLocalPlayer, int flags, int r, int g, int b, int a );

	void FillRect( int x, int y, int w, int h );
	void DrawRadarDot( int x, int y, float z_diff, int iBaseDotSize, int flags, int r, int g, int b, int a );

	CHudTexture *m_pBackground;
	CHudTexture *m_pBackgroundTrans;

	float m_flNextBombFlashTime;
	bool m_bBombFlash;

	float m_flNextHostageFlashTime;
	bool m_bHostageFlash;
	bool m_bHideRadar;
};

class CHudLocation : public CHudElement, public vgui::Label
{
public:
	DECLARE_CLASS_SIMPLE( CHudLocation, vgui::Panel );

	CHudLocation( const char *name );

	virtual void Init();
	virtual void LevelInit();
	virtual bool ShouldDraw();

	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnTick( void );

private:
	Color m_fgColor;
};

#endif // HUD_RADAR_H
