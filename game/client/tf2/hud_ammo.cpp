//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hud_numeric.h"
#include "hud_ammo.h"
#include "hud.h"
#include "iclientmode.h"
#include <vgui_controls/AnimationController.h>
#include <KeyValues.h>

//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
static CHudAmmo g_HudAmmo;
CHudAmmo* GetHudAmmo()
{
	return &g_HudAmmo;
}


//-----------------------------------------------------------------------------
// Accessor methods to set various state associated with the ammo display
//-----------------------------------------------------------------------------
void CHudAmmo::SetPrimaryAmmo( int nAmmoType, int nTotalAmmo, int nClipCount, int nMaxClipCount )
{
	m_nAmmoType1 = nAmmoType;
	m_nTotalAmmo1 = nTotalAmmo;
	m_nMaxClip1 = nMaxClipCount;
	m_nClip1 = nClipCount;
}

void CHudAmmo::SetSecondaryAmmo( int nAmmoType, int nTotalAmmo, int nClipCount, int nMaxClipCount )
{
	m_nAmmoType2 = nAmmoType;
	m_nTotalAmmo2 = nTotalAmmo;
	m_nMaxClip2 = nMaxClipCount;
	m_nClip2 = nClipCount;
}

bool CHudAmmo::ShouldShowPrimaryClip() const
{
	if ( m_nAmmoType1 <= 0 )
		return false;

	if ( m_nClip1 < 0 )
		return false;

	return true;
}

bool CHudAmmo::ShouldShowSecondary() const 
{
	if ( m_nAmmoType2 <= 0 )
		return false;

	if ( m_nTotalAmmo2 <= 0 )
		return false;

	return true;
}

void CHudAmmo::ShowHideHudControls()
{
	bool showClip = ShouldShowPrimaryClip();
	bool showSecondary = ShouldShowSecondary();

	if ( showClip )
	{
		if ( showSecondary )
		{
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "ShowPrimaryAmmoClipShowSecondaryAmmo" );
		}
		else
		{
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "ShowPrimaryAmmoClipHideSecondaryAmmo" );
		}
	}
	else
	{
		if ( showSecondary )
		{
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "HidePrimaryAmmoClipShowSecondaryAmmo" );
		}
		else
		{
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "HidePrimaryAmmoClipHideSecondaryAmmo" );
		}
	}
}

class CHudAmmoPrimary : public CHudNumeric
{
	DECLARE_CLASS_SIMPLE( CHudAmmoPrimary, CHudNumeric );
public:
	CHudAmmoPrimary( const char *pElementName ) : CHudNumeric( pElementName, "HudAmmoPrimary" )
	{
		SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD );
	}

	virtual const char *GetLabelText() { return m_szAmmoLabel; }
	virtual const char *GetPulseEvent( bool increment ) { return increment ? "PrimaryAmmoIncrement" : "PrimaryAmmoDecrement"; }

	virtual bool		GetValue( char *val, int maxlen )
	{
		if ( GetHudAmmo()->m_nAmmoType1 <= 0 )
			return false;

		int count = ( GetHudAmmo()->m_nClip1 >= 0 ) ? GetHudAmmo()->m_nClip1 : GetHudAmmo()->m_nTotalAmmo1;
		Q_snprintf( val, maxlen, "%i", count );
		return true;
	}

	virtual Color GetColor()
	{
		// Get our ratio bar information
		float	ammoPerc = 1.0f - ( (float) GetHudAmmo()->m_nClip1 ) / ( (float) GetHudAmmo()->m_nMaxClip1 );
		bool	ammoCaution = ( ammoPerc >= CLIP_PERC_THRESHOLD );

		if ( ammoCaution )
			return m_TextColorCritical;

		return m_TextColor;
	}

	virtual void ApplySchemeSettings(vgui::IScheme *scheme)
	{
		BaseClass::ApplySchemeSettings( scheme );

		SetPaintBackgroundEnabled( true );
	}

private:
	CPanelAnimationStringVar( 128, m_szAmmoLabel, "AmmoLabel", "Ammo" );
};

DECLARE_HUDELEMENT( CHudAmmoPrimary );

class CHudAmmoPrimaryClip : public CHudNumeric
{
	DECLARE_CLASS_SIMPLE( CHudAmmoPrimaryClip, CHudNumeric );

public:
	CHudAmmoPrimaryClip( const char *pElementName ) : BaseClass( pElementName, "HudAmmoPrimaryClip" )
	{
		SetDrawLabel( false );

		m_nPrevVisible = -1;

		SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD );
	}

	virtual const char *GetLabelText() { return m_szAmmoClipLabel; }
	virtual const char *GetPulseEvent( bool increment ) { return increment ? "PrimaryAmmoClipIncrement" : "PrimaryAmmoClipDecrement"; }

	virtual bool		GetValue( char *val, int maxlen )
	{
		int iret = _GetValue( val, maxlen ) ? 1 : 0;

		if ( iret != m_nPrevVisible )
		{
			GetHudAmmo()->ShowHideHudControls();
			m_nPrevVisible = iret;
		}

		return true;
	}

	virtual bool		_GetValue( char *val, int maxlen )
	{
		Q_snprintf( val, maxlen, "" );

		if ( !GetHudAmmo()->ShouldShowPrimaryClip() )
			return false;

		int count = GetHudAmmo()->m_nTotalAmmo1;
		Q_snprintf( val, maxlen, "%i", count );
		return true;
	}

	virtual Color GetColor()
	{
		if ( GetHudAmmo()->ShouldShowPrimaryClip() )
		{
			if ( GetHudAmmo()->m_nTotalAmmo1 <= GetHudAmmo()->m_nMaxClip1 )
			{
				return m_TextColorCritical;
			}
		}

		return m_TextColor;
	}

private:
	int		m_nPrevVisible;
	CPanelAnimationStringVar( 128, m_szAmmoClipLabel, "AmmoClipLabel", "PrimaryAmmoClip" );
};

DECLARE_HUDELEMENT( CHudAmmoPrimaryClip );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CHudAmmoSecondary : public CHudNumeric
{
	DECLARE_CLASS_SIMPLE( CHudAmmoSecondary, CHudNumeric );

public:
	CHudAmmoSecondary( const char *pElementName ) : CHudNumeric( pElementName, "HudAmmoSecondary" )
	{
		SetDrawLabel( false );

		m_nPrevVisible = -1;

		SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD );
	}

	virtual const char *GetLabelText() { return m_szAmmoSecondaryLabel; }
	virtual const char *GetPulseEvent( bool increment ) { return increment ? "SecondaryAmmoIncrement" : "SecondaryAmmoDecrement"; }

	virtual bool		GetValue( char *val, int maxlen )
	{
		int iret = _GetValue( val, maxlen ) ? 1 : 0;

		if ( iret != m_nPrevVisible )
		{
			// Shift primary and clip left/right as needed
			GetHudAmmo()->ShowHideHudControls();
			m_nPrevVisible = iret;
		}

		return iret ? true : false;
	}

	virtual bool		_GetValue( char *val, int maxlen )
	{
		if ( !GetHudAmmo()->ShouldShowSecondary() )
			return false;

		int count = GetHudAmmo()->m_nTotalAmmo2;
		Q_snprintf( val, maxlen, "%i", count );
		return true;
	}

	virtual Color GetColor()
	{
		if ( GetHudAmmo()->m_nAmmoType2 > 0 &&
			 GetHudAmmo()->m_nTotalAmmo2 == 1 )
		{
			return m_TextColorCritical;
		}

		return m_TextColor;
	}

private:
	int		m_nPrevVisible;
	CPanelAnimationStringVar( 128, m_szAmmoSecondaryLabel, "AmmoSecondaryLabel", "AmmoSecondary" );
};

DECLARE_HUDELEMENT( CHudAmmoSecondary );
