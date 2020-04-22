//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef C_TF_ITEMEFFECTMETER_H
#define C_TF_ITEMEFFECTMETER_H

#include "cbase.h"

#include "c_tf_player.h"
#include "c_tf_playerclass.h"
#include "hudelement.h"
#include "iclientmode.h"
#include "tf_weapon_invis.h"
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/ProgressBar.h>
#include <vgui_controls/Label.h>

using namespace vgui;

class CHudItemEffectMeter;
class CItemEffectMeterLogic;
class CItemEffectMeterManager;

extern CItemEffectMeterManager g_ItemEffectMeterManager;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CItemEffectMeterManager : public CGameEventListener
{
public:
	~CItemEffectMeterManager();

	void			ClearExistingMeters();
	void			SetPlayer( C_TFPlayer* pPlayer );
	void			Update( C_TFPlayer* pPlayer );
	virtual void	FireGameEvent( IGameEvent *event );
	int				GetNumEnabled( void );

private:
	CUtlVector< vgui::DHANDLE< CHudItemEffectMeter > >	m_Meters;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
DECLARE_AUTO_LIST( IHudItemEffectMeterAutoList );
class CHudItemEffectMeter : public CHudElement, public EditablePanel, public IHudItemEffectMeterAutoList
{
	DECLARE_CLASS_SIMPLE( CHudItemEffectMeter, EditablePanel );

public:
	CHudItemEffectMeter( const char *pszElementName, C_TFPlayer* pPlayer );
	~CHudItemEffectMeter();

	static void		CreateHudElementsForClass( C_TFPlayer* pPlayer, CUtlVector< vgui::DHANDLE< CHudItemEffectMeter > >& outMeters );

	// Hud Element
	virtual void	ApplySchemeSettings( IScheme *scheme );
	virtual void	PerformLayout();
	virtual bool	ShouldDraw( void );
	virtual void	Update( C_TFPlayer* pPlayer, const char* pSoundScript = "TFPlayer.ReCharged" );

	// Effect Meter Logic
	virtual bool		IsEnabled( void )		{ return m_bEnabled; }
	virtual const char*	GetLabelText( void );
	virtual const char*	GetIconName( void )		{ return "../hud/ico_stickybomb_red"; }
	virtual float		GetProgress( void );
	virtual bool		ShouldBeep( void )		
	{ 
		if ( m_pPlayer )
		{
			CTFWeaponInvis *pWpn = (CTFWeaponInvis *) m_pPlayer->Weapon_OwnsThisID( TF_WEAPON_INVIS );
			if ( pWpn && pWpn->HasFeignDeath() )
				return true;
		}
		
		return false;
	}
	virtual const char *GetResFile( void )			{ return "resource/UI/HudItemEffectMeter.res"; }
	virtual int			GetCount( void )			{ return -1; }
	virtual bool		ShouldFlash( void )			{ return false; }
	virtual bool		ShowPercentSymbol( void )	{ return false; }

	virtual Color		GetFgColor( void )		{ return Color( 255, 255, 255, 255 ); }
	virtual bool		IsKillstreakMeter( void ) { return false; }

protected:
	vgui::Label *m_pLabel;
	vgui::ContinuousProgressBar *m_pProgressBar;
	float				m_flOldProgress;

protected:
	CHandle<C_TFPlayer>	m_pPlayer;
	bool				m_bEnabled;

	CPanelAnimationVarAliasType( float, m_iXOffset, "x_offset", "0", "proportional_float" );
};

//-----------------------------------------------------------------------------
// Purpose: Template variation for weapon based meters.
//-----------------------------------------------------------------------------
template <class T>
class CHudItemEffectMeter_Weapon : public CHudItemEffectMeter
{
public:
	CHudItemEffectMeter_Weapon( const char *pszElementName, C_TFPlayer* pPlayer, int iWeaponID, bool bBeeps=true, const char* pszResFile=NULL );

	T*					GetWeapon( void );

	virtual void		Update( C_TFPlayer* pPlayer, const char* pSoundScript = "TFPlayer.ReCharged" );

	// Effect Meter Logic
	virtual bool		IsEnabled( void );
	virtual const char*	GetLabelText( void ) { return m_pWeapon ? m_pWeapon->GetEffectLabelText() : ""; }
	virtual const char*	GetIconName( void ) { return "../hud/ico_stickybomb_red"; }
	virtual float		GetProgress( void );
	virtual bool		ShouldBeep( void ) { return m_bBeeps; }
	virtual const char *GetResFile( void );
	virtual int			GetCount( void ) { return -1; }
	virtual bool		ShouldFlash( void ) { return false; }
	virtual Color		GetFgColor( void )		{ return Color( 255, 255, 255, 255 ); }
	virtual bool		ShouldDraw( void );
	virtual bool		ShowPercentSymbol( void )	{ return false; }
	virtual bool		IsKillstreakMeter( void )	{ return false; }

private:
	CHandle<T>			m_pWeapon;
	int					m_iWeaponID;
	bool				m_bBeeps;
	const char*			m_pszResFile;
};

class CHudItemEffectMeter_Rune : public CHudItemEffectMeter
{
public:

	CHudItemEffectMeter_Rune( const char *pszElementName, C_TFPlayer* pPlayer );

	// Effect Meter Logic
	virtual bool		IsEnabled( void );
	virtual float		GetProgress( void );
	virtual bool		ShouldDraw( void );

	virtual const char*	GetLabelText( void ) { return "Powerup"; }
	virtual const char *GetResFile( void )		{ return "resource/UI/HudPowerupEffectMeter.res"; }

	virtual int			GetCount( void )	{ return -1; }
	virtual bool		ShouldFlash( void );
	virtual Color		GetFgColor( void )	{ return Color( 255, 255, 255, 255 ); }
};

#ifdef STAGING_ONLY
class CHudItemEffectMeter_SpaceJump : public CHudItemEffectMeter
{
public:

	CHudItemEffectMeter_SpaceJump( const char *pszElementName, C_TFPlayer* pPlayer );

	// Effect Meter Logic
	virtual bool		IsEnabled( void );
	virtual float		GetProgress( void );
	virtual bool		ShouldDraw( void );

	virtual const char*	GetLabelText( void ) { return "Fuel"; }
	virtual const char *GetResFile( void )		{ return "resource/UI/HudItemEffectMeter.res"; }

	virtual int			GetCount( void )	{ return -1; }
	virtual bool		ShouldFlash( void ) { return false; }
	virtual Color		GetFgColor( void )	{ return Color( 255, 255, 255, 255 ); }
};

class CHudItemEffectMeter_Tranq : public CHudItemEffectMeter
{
public:

	CHudItemEffectMeter_Tranq( const char *pszElementName, C_TFPlayer* pPlayer );

	// Effect Meter Logic
	virtual bool		IsEnabled( void );
	virtual float		GetProgress( void );

	virtual const char*	GetLabelText( void )	{ return "Finesse"; }
	virtual const char *GetResFile( void )		{ return "resource/UI/HudItemEffectMeter_Tranq.res"; }

	virtual int			GetCount( void )	{ return -1; }
	virtual bool		ShouldFlash( void ) { return false; }
	virtual Color		GetFgColor( void )	{ return Color( 255, 255, 255, 255 ); }
};
#endif // STAGING_ONLY

#endif