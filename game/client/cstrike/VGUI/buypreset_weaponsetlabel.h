//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef BUYPRESET_WEAPONSETLABEL_H
#define BUYPRESET_WEAPONSETLABEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>
#include <vgui/IImage.h>

namespace vgui
{
	class TextImage;
	class TextEntry;
};

//--------------------------------------------------------------------------------------------------------------
/// Helper function: draws a simple dashed line
void DrawDashedLine(int x0, int y0, int x1, int y1, int dashLen, int gapLen);

//--------------------------------------------------------------------------------------------------------------
// Purpose: Wraps an IImage to perform resizes properly
class BuyPresetImage : public vgui::IImage
{
public:
	BuyPresetImage( vgui::IImage *realImage )
	{
		m_image = realImage;
		if ( m_image )
		{
			m_image->GetSize( m_wide, m_tall );
		}
		else
		{
			m_wide = m_tall = 0;
		}
	}

	// Call to Paint the image
	// Image will draw within the current panel context at the specified position
	virtual void Paint()
	{
		if ( !m_image )
			return;

		m_image->Paint();
	}

	// Set the position of the image
	virtual void SetPos(int x, int y)
	{
		if ( !m_image )
			return;

		m_image->SetPos( x, y );
	}

	// Gets the size of the content
	virtual void GetContentSize(int &wide, int &tall)
	{
		if ( !m_image )
			return;

		m_image->GetSize( wide, tall );
	}

	// Get the size the image will actually draw in (usually defaults to the content size)
	virtual void GetSize(int &wide, int &tall)
	{
		if ( !m_image )
		{
			wide = tall = 0;
			return;
		}

		wide = m_wide;
		tall = m_tall;
	}

	// Sets the size of the image
	virtual void SetSize(int wide, int tall)
	{
		m_wide = wide;
		m_tall = tall;
		if ( !m_image )
			return;

		m_image->SetSize( wide, tall );
	}

	// Set the draw color 
	virtual void SetColor(Color col)
	{
		if ( !m_image )
			return;

		m_image->SetColor( col );
	}

	virtual bool Evict()
	{
		return false;
	}

	virtual int GetNumFrames()
	{
		return 0;
	}

	virtual void SetFrame( int nFrame )
	{
	}

	virtual vgui::HTexture GetID()
	{
		return 0;
	}

	virtual void SetRotation( int iRotation )
	{
		return;
	}

private:
	vgui::IImage *m_image;
	int m_wide, m_tall;
};

//--------------------------------------------------------------------------------------------------------------
struct ImageInfo {
	vgui::IImage *image;
	int w;
	int h;
	int x;
	int y;
	int fullW;
	int fullH;

	void FitInBounds( int baseX, int baseY, int width, int height, bool center, int scaleAt1024, bool halfHeight = false );
	void Paint();
};

//--------------------------------------------------------------------------------------------------------------
class WeaponImageInfo
{
public:
	WeaponImageInfo();
	~WeaponImageInfo();

	void SetBounds( int left, int top, int wide, int tall );
	void SetCentered( bool isCentered );
	void SetScaleAt1024( int weaponScale, int ammoScale );
	void SetWeapon( const BuyPresetWeapon *pWeapon, bool isPrimary, bool useCurrentAmmoType );

	void ApplyTextSettings( vgui::IScheme *pScheme, bool isProportional );

	void Paint();
	void PaintText();

private:
	void PerformLayout();

	int m_left;
	int m_top;
	int m_wide;
	int m_tall;

	bool m_isPrimary;

	int m_weaponScale;
	int m_ammoScale;

	bool m_needLayout;
	bool m_isCentered;
	ImageInfo m_weapon;
	ImageInfo m_ammo;

	vgui::TextImage *m_pAmmoText;
};

//--------------------------------------------------------------------------------------------------------------
class ItemImageInfo
{
public:
	ItemImageInfo();
	~ItemImageInfo();

	void SetBounds( int left, int top, int wide, int tall );
	void SetItem( const char *imageFname, int count );
	void ApplyTextSettings( vgui::IScheme *pScheme, bool isProportional );

	void Paint();
	void PaintText();

private:
	void PerformLayout();

	int m_left;
	int m_top;
	int m_wide;
	int m_tall;

	int m_count;

	bool m_needLayout;
	ImageInfo m_image;

	vgui::TextImage *m_pText;
};

//--------------------------------------------------------------------------------------------------------------
class WeaponLabel : public vgui::Panel
{
	typedef vgui::Panel BaseClass;
public:
	WeaponLabel(vgui::Panel *parent, const char *panelName);
	~WeaponLabel();

	void SetWeapon( const BuyPresetWeapon *pWeapon, bool isPrimary, bool showAmmo = false );

	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void PerformLayout();
	virtual void Paint();

protected:
	WeaponImageInfo m_weapon;
};

//--------------------------------------------------------------------------------------------------------------
class EquipmentLabel : public vgui::Panel
{
	typedef vgui::Panel BaseClass;
public:
	EquipmentLabel(vgui::Panel *parent, const char *panelName, const char *imageFname = NULL);
	~EquipmentLabel();

	void SetItem( const char *imageFname, int count );

	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void PerformLayout();
	virtual void Paint();

protected:
	ItemImageInfo m_item;
};

//--------------------------------------------------------------------------------------------------------------
/**
 * BuyPresetEditPanel is a panel displaying a graphical representation of a buy preset.
 */
class BuyPresetEditPanel : public vgui::EditablePanel
{
	typedef vgui::EditablePanel BaseClass;
public:
	BuyPresetEditPanel( vgui::Panel *parent, const char *panelName, const char *resourceFilename, int fallbackIndex, bool editableName );
	virtual ~BuyPresetEditPanel();

	void SetWeaponSet( const WeaponSet *pWeaponSet, bool current );
	virtual void SetText( const wchar_t *text );

	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	void OnCommand( const char *command);					///< Handle command callbacks

	virtual void OnSizeChanged( int wide, int tall );

	void SetPanelBgColor( Color color ) { if (m_pBgPanel) m_pBgPanel->SetBgColor( color ); }

protected:
	void Reset();

	vgui::Panel *m_pBgPanel;

	vgui::TextEntry *m_pTitleEntry;
	vgui::Label *m_pTitleLabel;
	vgui::Label *m_pCostLabel;

	WeaponLabel *m_pPrimaryWeapon;
	WeaponLabel *m_pSecondaryWeapon;

	EquipmentLabel *m_pHEGrenade;
	EquipmentLabel *m_pSmokeGrenade;
	EquipmentLabel *m_pFlashbangs;

	EquipmentLabel *m_pDefuser;
	EquipmentLabel *m_pNightvision;

	EquipmentLabel *m_pArmor;

	int m_baseWide;
	int m_baseTall;

	int m_fallbackIndex;
};


#endif // BUYPRESET_WEAPONSETLABEL_H
