//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"

#include "weapon_csbase.h"
#include "cs_ammodef.h"

#include <vgui/IVGui.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Label.h>
#include <vgui/ILocalize.h>
#include "vgui_controls/BuildGroup.h"
#include "vgui_controls/BitmapImagePanel.h"
#include "vgui_controls/TextEntry.h"
#include "vgui_controls/TextImage.h"
#include "vgui_controls/RichText.h"
#include "vgui_controls/QueryBox.h"
#include "career_box.h"
#include "buypreset_listbox.h"
#include "buypreset_weaponsetlabel.h"

#include "cstrike/bot/shared_util.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
WeaponImageInfo::WeaponImageInfo()
{
	m_needLayout = m_isCentered = false;
	m_left = m_top = m_wide = m_tall = 0;
	m_isPrimary = false;
	memset( &m_weapon,	0, sizeof(ImageInfo) );
	memset( &m_ammo,	0, sizeof(ImageInfo) );
	m_weaponScale = m_ammoScale = 0;
	m_pAmmoText = new TextImage( "" );
}

//--------------------------------------------------------------------------------------------------------------
WeaponImageInfo::~WeaponImageInfo()
{
	delete m_pAmmoText;
}

//--------------------------------------------------------------------------------------------------------------
void WeaponImageInfo::ApplyTextSettings( vgui::IScheme *pScheme, bool isProportional )
{
	Color color = pScheme->GetColor( "FgColor", Color( 0, 0, 0, 0 ) );

	m_pAmmoText->SetColor( color );
	m_pAmmoText->SetFont( pScheme->GetFont( "Default", isProportional ) );
	m_pAmmoText->SetWrap( false );
}

//--------------------------------------------------------------------------------------------------------------
void WeaponImageInfo::SetBounds( int left, int top, int wide, int tall )
{
	m_left = left;
	m_top = top;
	m_wide = wide;
	m_tall = tall;
	m_needLayout = true;
}

//--------------------------------------------------------------------------------------------------------------
void WeaponImageInfo::SetCentered( bool isCentered )
{
	m_isCentered = isCentered;
	m_needLayout = true;
}

//--------------------------------------------------------------------------------------------------------------
void WeaponImageInfo::SetScaleAt1024( int weaponScale, int ammoScale )
{
	m_weaponScale = weaponScale;
	m_ammoScale = ammoScale;
	m_needLayout = true;
}

//--------------------------------------------------------------------------------------------------------------
void WeaponImageInfo::SetWeapon( const BuyPresetWeapon *pWeapon, bool isPrimary, bool useCurrentAmmoType )
{
	m_pAmmoText->SetText( L"" );
	m_weapon.image = NULL;
	m_ammo.image = NULL;
	m_isPrimary = isPrimary;

	if ( !pWeapon )
		return;

	wchar_t *multiplierString = g_pVGuiLocalize->Find("#Cstrike_BuyMenuPresetMultiplier");
	if ( !multiplierString )
		multiplierString = L"";
	const int BufLen = 32;
	wchar_t buf[BufLen];

	if ( pWeapon->GetAmmoType() == AMMO_CLIPS )
	{
		CSWeaponID weaponID = pWeapon->GetWeaponID();
		const CCSWeaponInfo *info = GetWeaponInfo( weaponID );
		int numClips = pWeapon->GetAmmoAmount();
		if ( info )
		{
			int maxRounds = GetCSAmmoDef()->MaxCarry( info->iAmmoType );
			int buyClipSize = GetCSAmmoDef()->GetBuySize( info->iAmmoType );

			int maxClips = (buyClipSize > 0) ? ceil(maxRounds/(float)buyClipSize) : 0;
			numClips = MIN( numClips, maxClips );
			m_weapon.image = scheme()->GetImage( ImageFnameFromWeaponID( weaponID, m_isPrimary ), true );
			if ( numClips == 0 )
			{
				m_ammo.image = NULL;
			}
			else if ( info->m_WeaponType == WEAPONTYPE_SHOTGUN )
			{
				m_ammo.image = scheme()->GetImage( "gfx/vgui/shell", true );
			}
			else if ( isPrimary )
			{
				m_ammo.image = scheme()->GetImage( "gfx/vgui/bullet", true );
			}
			else
			{
				m_ammo.image = scheme()->GetImage( "gfx/vgui/cartridge", true );
			}

			if ( numClips > 1 )
			{
				g_pVGuiLocalize->ConstructString( buf, sizeof(buf), multiplierString, 1, NumAsWString( numClips ) );
				m_pAmmoText->SetText( buf );
			}
			else
			{
				m_pAmmoText->SetText( L"" );
			}
		}
		else if ( numClips > 0 || !useCurrentAmmoType )
		{
			if ( useCurrentAmmoType )
			{
				CSWeaponID currentID = GetClientWeaponID( isPrimary );
				m_weapon.image = scheme()->GetImage( ImageFnameFromWeaponID( currentID, m_isPrimary ), true );
				info = GetWeaponInfo( currentID );
				if ( !info )
				{
					m_weapon.image = NULL;
					numClips = 0;
				}
				else if ( info->m_WeaponType == WEAPONTYPE_SHOTGUN )
				{
					m_ammo.image = scheme()->GetImage( "gfx/vgui/shell", true );
				}
				else if ( isPrimary )
				{
					m_ammo.image = scheme()->GetImage( "gfx/vgui/bullet", true );
				}
				else
				{
					m_ammo.image = scheme()->GetImage( "gfx/vgui/cartridge", true );
				}
			}
			else
			{
				m_weapon.image = scheme()->GetImage( ImageFnameFromWeaponID( weaponID, m_isPrimary ), true );
				if ( numClips == 0 )
				{
					m_ammo.image = NULL;
				}
				else if ( isPrimary )
				{
					m_ammo.image = scheme()->GetImage( "gfx/vgui/bullet", true );
				}
				else
				{
					m_ammo.image = scheme()->GetImage( "gfx/vgui/cartridge", true );
				}
			}
			if ( numClips > 1 )
			{
				g_pVGuiLocalize->ConstructString( buf, sizeof(buf), multiplierString, 1, NumAsWString( numClips ) );
				m_pAmmoText->SetText( buf );
			}
			else
			{
				m_pAmmoText->SetText( L"" );
			}
		}
	}
	m_needLayout = true;
}

//--------------------------------------------------------------------------------------------------------------
void WeaponImageInfo::Paint()
{
	if ( m_needLayout )
		PerformLayout();

	m_weapon.Paint();
	m_ammo.Paint();
}

//--------------------------------------------------------------------------------------------------------------
void WeaponImageInfo::PaintText()
{
	if ( m_needLayout )
		PerformLayout();

	m_pAmmoText->Paint();
}

//--------------------------------------------------------------------------------------------------------------
void WeaponImageInfo::PerformLayout()
{
	m_needLayout = false;

	m_weapon.FitInBounds( m_left, m_top, m_wide*0.8, m_tall, m_isCentered, m_weaponScale );
	int ammoX = MIN( m_wide*5/6, m_weapon.w );
	int ammoSize = m_tall * 9 / 16;
	if ( !m_isPrimary )
	{
		ammoSize = ammoSize * 25 / 40;
		ammoX = MIN( m_wide*5/6, m_weapon.w*3/4 );
	}
	if ( ammoX + ammoSize > m_wide )
	{
		ammoX = m_wide - ammoSize;
	}
	m_ammo.FitInBounds( m_left + ammoX, m_top + m_tall - ammoSize, ammoSize, ammoSize, false, m_ammoScale );

	int w, h;
	m_pAmmoText->ResizeImageToContent();
	m_pAmmoText->GetSize( w, h );
	if ( m_isPrimary )
	{
		if ( m_ammoScale < 75 )
		{
			m_pAmmoText->SetPos( m_left + ammoX + ammoSize*1.25f - w, m_top + m_tall - h );
		}
		else
		{
			m_pAmmoText->SetPos( m_left + ammoX + ammoSize - w, m_top + m_tall - h );
		}
	}
	else
	{
		m_pAmmoText->SetPos( m_left + ammoX + ammoSize, m_top + m_tall - h );
	}
}

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
WeaponLabel::WeaponLabel(Panel *parent, const char *panelName) : BaseClass( parent, panelName )
{
	SetSize( 10, 10 );
	SetMouseInputEnabled( false );
}

//--------------------------------------------------------------------------------------------------------------
WeaponLabel::~WeaponLabel()
{
}

//--------------------------------------------------------------------------------------------------------------
void WeaponLabel::SetWeapon( const BuyPresetWeapon *pWeapon, bool isPrimary, bool showAmmo )
{
	BuyPresetWeapon weapon(WEAPON_NONE);
	if ( pWeapon )
		weapon = *pWeapon;
	if ( !showAmmo )
		weapon.SetAmmoAmount( 0 );
	m_weapon.SetWeapon( &weapon, isPrimary, false );
}

//--------------------------------------------------------------------------------------------------------------
void WeaponLabel::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_weapon.ApplyTextSettings( pScheme, IsProportional() );
}

//--------------------------------------------------------------------------------------------------------------
void WeaponLabel::PerformLayout()
{
	BaseClass::PerformLayout();

	int wide, tall;
	GetSize( wide, tall );
	m_weapon.SetBounds( 0, 0, wide, tall );
}

//--------------------------------------------------------------------------------------------------------------
void WeaponLabel::Paint()
{
	BaseClass::Paint();

	m_weapon.Paint();
	m_weapon.PaintText();
}

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
ItemImageInfo::ItemImageInfo()
{
	m_needLayout = false;
	m_left = m_top = m_wide = m_tall = 0;
	m_count = 0;
	memset( &m_image, 0, sizeof(ImageInfo) );
	m_pText = new TextImage( "" );

	SetBounds( 0, 0, 100, 100 );
}

//--------------------------------------------------------------------------------------------------------------
ItemImageInfo::~ItemImageInfo()
{
	delete m_pText;
}

//--------------------------------------------------------------------------------------------------------------
void ItemImageInfo::ApplyTextSettings( vgui::IScheme *pScheme, bool isProportional )
{
	Color color = pScheme->GetColor( "Label.TextColor", Color( 0, 0, 0, 0 ) );

	m_pText->SetColor( color );
	m_pText->SetFont( pScheme->GetFont( "Default", isProportional ) );
	m_pText->SetWrap( false );
}

//--------------------------------------------------------------------------------------------------------------
void ItemImageInfo::SetBounds( int left, int top, int wide, int tall )
{
	m_left = left;
	m_top = top;
	m_wide = wide;
	m_tall = tall;
	m_needLayout = true;
}

//--------------------------------------------------------------------------------------------------------------
void ItemImageInfo::SetItem( const char *imageFname, int count )
{
	m_pText->SetText( L"" );
	m_count = count;

	if ( imageFname )
		m_image.image = scheme()->GetImage( imageFname, true );
	else
		m_image.image = NULL;

	if ( count > 1 )
	{
		wchar_t *multiplierString = g_pVGuiLocalize->Find("#Cstrike_BuyMenuPresetMultiplier");
		if ( !multiplierString )
			multiplierString = L"";
		const int BufLen = 32;
		wchar_t buf[BufLen];

		g_pVGuiLocalize->ConstructString( buf, sizeof(buf), multiplierString, 1, NumAsWString( count ) );
		m_pText->SetText( buf );
	}
	m_needLayout = true;
}

//--------------------------------------------------------------------------------------------------------------
void ItemImageInfo::Paint()
{
	if ( m_needLayout )
		PerformLayout();

	if ( m_count )
		m_image.Paint();
}

//--------------------------------------------------------------------------------------------------------------
void ItemImageInfo::PaintText()
{
	if ( m_needLayout )
		PerformLayout();

	m_pText->Paint();
}

//--------------------------------------------------------------------------------------------------------------
void ItemImageInfo::PerformLayout()
{
	m_needLayout = false;

	m_image.FitInBounds( m_left, m_top, m_wide, m_tall, false, 0 );

	int w, h;
	m_pText->ResizeImageToContent();
	m_pText->GetSize( w, h );
	m_pText->SetPos( m_left + m_image.w - w, m_top + m_tall - h );
}

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
EquipmentLabel::EquipmentLabel(Panel *parent, const char *panelName, const char *imageFname) : BaseClass( parent, panelName )
{
	SetSize( 10, 10 );
	m_item.SetItem( imageFname, 0 );
	SetMouseInputEnabled( false );
}

//--------------------------------------------------------------------------------------------------------------
EquipmentLabel::~EquipmentLabel()
{
}

//--------------------------------------------------------------------------------------------------------------
void EquipmentLabel::SetItem( const char *imageFname, int count )
{
	m_item.SetItem( imageFname, count );
}

//--------------------------------------------------------------------------------------------------------------
void EquipmentLabel::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_item.ApplyTextSettings( pScheme, IsProportional() );
}

//--------------------------------------------------------------------------------------------------------------
void EquipmentLabel::PerformLayout()
{
	BaseClass::PerformLayout();

	int wide, tall;
	GetSize( wide, tall );
	m_item.SetBounds( 0, 0, wide, tall );
}

//--------------------------------------------------------------------------------------------------------------
void EquipmentLabel::Paint()
{
	BaseClass::Paint();

	m_item.Paint();
	m_item.PaintText();
}


//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
/// Helper function: draws a simple dashed line
void DrawDashedLine(int x0, int y0, int x1, int y1, int dashLen, int gapLen)
{
	// work out which way the line goes
	if ((x1 - x0) > (y1 - y0))
	{
		// x direction line
		while (1)
		{
			if (x0 + dashLen > x1)
			{
				// draw partial
				surface()->DrawFilledRect(x0, y0, x1, y1+1);
			}
			else
			{
				surface()->DrawFilledRect(x0, y0, x0 + dashLen, y1+1);
			}

			x0 += dashLen;

			if (x0 + gapLen > x1)
				break;

			x0 += gapLen;
		}
	}
	else
	{
		// y direction
		while (1)
		{
			if (y0 + dashLen > y1)
			{
				// draw partial
				surface()->DrawFilledRect(x0, y0, x1+1, y1);
			}
			else
			{
				surface()->DrawFilledRect(x0, y0, x1+1, y0 + dashLen);
			}

			y0 += dashLen;

			if (y0 + gapLen > y1)
				break;

			y0 += gapLen;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
void ImageInfo::Paint()
{
	if ( !image )
		return;

	image->SetSize( w, h );
	image->SetPos( x, y );
	image->Paint();
	image->SetSize( 0, 0 );	// restore image size to content size to not mess up other places that use the same image
}

//--------------------------------------------------------------------------------------------------------------
void ImageInfo::FitInBounds( int baseX, int baseY, int width, int height, bool center, int scaleAt1024, bool halfHeight )
{
	if ( !image )
	{
		x = y = w = h = 0;
		return;
	}

	image->GetContentSize(fullW, fullH);

	if ( scaleAt1024 )
	{
		int screenW, screenH;
		GetHudSize( screenW, screenH );

		w = fullW * screenW / 1024 * scaleAt1024 / 100;
		h = fullH * screenW / 1024 * scaleAt1024 / 100;

		if ( fullH > 64 && scaleAt1024 == 100 )
		{
			w = w * 64 / fullH;
			h = h * 64 / fullH;
		}

		if ( h > height * 1.2 )
			scaleAt1024 = 0;
	}
	if ( !scaleAt1024 )
	{
		w = fullW;
		h = fullH;

		if ( h != height )
		{
			w = (int) w * 1.0f * height / h;
			h = height;
		}

		if ( w > width )
		{
			h = (int) h * 1.0f * width / w;
			w = width;
		}
	}

	if ( center )
	{
		x = baseX + (width - w)/2;
	}
	else
	{
		x = baseX;
	}
	y = baseY + (height - h)/2;
}

//--------------------------------------------------------------------------------------------------------------
