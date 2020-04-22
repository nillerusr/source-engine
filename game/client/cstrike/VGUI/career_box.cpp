//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

// Author: Matthew D. Campbell (matt@turtlerockstudios.com), 2003

#include "cbase.h"

#include <vgui/KeyCode.h>
#include "career_box.h"
#include "career_button.h"
#include "buypreset_listbox.h"
#include <vgui_controls/TextImage.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/ComboBox.h>
#include "cs_ammodef.h"
#include "weapon_csbase.h"
#include "backgroundpanel.h"
#include "cs_gamerules.h"

#include <vgui/IInput.h>
#include "bot/shared_util.h"
#include <vgui_controls/BitmapImagePanel.h>
#include <vgui/ISurface.h>
#include <vgui/ILocalize.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

using namespace vgui;

//--------------------------------------------------------------------------------------------------------------
class ConVarToggleCheckButton : public vgui::CheckButton
{
	DECLARE_CLASS_SIMPLE( ConVarToggleCheckButton, vgui::CheckButton );

public:
	ConVarToggleCheckButton( vgui::Panel *parent, const char *panelName, const char *text );
	~ConVarToggleCheckButton();

	virtual void	SetSelected( bool state );

	virtual void	Paint();

	void			Reset();
	void			ApplyChanges();
	bool			HasBeenModified();
	void			SetConVar( const char *name );

	virtual void ApplySettings( KeyValues *inResourceData )
	{
		BaseClass::ApplySettings( inResourceData );

		const char *name = inResourceData->GetString( "convar", NULL );
		if ( name )
		{
			SetConVar( name );
		}
	}

private:
	MESSAGE_FUNC( OnButtonChecked, "CheckButtonChecked" );

	char			*m_pszCvarName;
	bool			m_bStartValue;
};


//--------------------------------------------------------------------------------------------------------------
ConVarToggleCheckButton::ConVarToggleCheckButton( Panel *parent, const char *panelName, const char *text )
 : CheckButton( parent, panelName, text )
{
	m_pszCvarName = NULL;
	AddActionSignalTarget( this );
}


//--------------------------------------------------------------------------------------------------------------
ConVarToggleCheckButton::~ConVarToggleCheckButton()
{
	if ( m_pszCvarName )
		delete[] m_pszCvarName;
}


//--------------------------------------------------------------------------------------------------------------
void ConVarToggleCheckButton::SetConVar( const char *name )
{
	if ( m_pszCvarName )
		delete[] m_pszCvarName;

	m_pszCvarName = CloneString( name );

	if (m_pszCvarName && *m_pszCvarName)
	{
		Reset();
	}
}


//--------------------------------------------------------------------------------------------------------------
void ConVarToggleCheckButton::Paint()
{
	if ( !m_pszCvarName || !m_pszCvarName[ 0 ] ) 
	{
		BaseClass::Paint();
		return;
	}

	// Look up current value
	ConVar const *var = cvar->FindVar( m_pszCvarName );
	if ( !var )
		return;
	bool value = var->GetBool();
	
	if (value != m_bStartValue)
	{
		SetSelected( value );
		m_bStartValue = value;
	}
	BaseClass::Paint();
}


//--------------------------------------------------------------------------------------------------------------
void ConVarToggleCheckButton::ApplyChanges()
{
	m_bStartValue = IsSelected();
	ConVar *var = (ConVar *)cvar->FindVar( m_pszCvarName );
	if ( !var )
		return;
	var->SetValue(m_bStartValue);

}


//--------------------------------------------------------------------------------------------------------------
void ConVarToggleCheckButton::Reset()
{
	ConVar const *var = cvar->FindVar( m_pszCvarName );
	if ( !var )
		return;
	m_bStartValue = var->GetBool();
	SetSelected(m_bStartValue);
}


//--------------------------------------------------------------------------------------------------------------
bool ConVarToggleCheckButton::HasBeenModified()
{
	return IsSelected() != m_bStartValue;
}


//--------------------------------------------------------------------------------------------------------------
void ConVarToggleCheckButton::SetSelected( bool state )
{
	BaseClass::SetSelected( state );

	if ( !m_pszCvarName || !m_pszCvarName[ 0 ] ) 
		return;
}


//--------------------------------------------------------------------------------------------------------------
void ConVarToggleCheckButton::OnButtonChecked()
{
	if (HasBeenModified())
	{
		PostActionSignal(new KeyValues("ControlModified"));
	}
}


//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
CCareerBaseBox::CCareerBaseBox(Panel *parent, const char *panelName, bool loadResources, bool useCareerButtons) : Frame(parent, panelName, false)
{
// @TODO:	SetScheme("CareerBoxScheme");
	SetScheme("ClientScheme");
	SetProportional( true );
	SetMoveable(false);
	SetSizeable(false);

	m_bgColor = Color( 0, 0, 0, 0 );
	m_borderColor = Color( 0, 0, 0, 0 );

	m_pTextLabel = new Label( this, "TextLabel", "" );

	if ( useCareerButtons )
	{
		m_pOkButton = new CCareerButton( this, "OkButton", "", "", false );
		m_pCancelButton = new CCareerButton( this, "CancelButton", "", "", false );
	}
	else
	{
		m_pOkButton = new Button( this, "OkButton", "" );
		m_pCancelButton = new Button( this, "CancelButton", "" );
	}
	m_pOkButton->SetVisible(false);
	if ( useCareerButtons )
	{
		m_buttons.PutElement( m_pOkButton );
		m_buttons.PutElement( m_pCancelButton );
	}
	m_cancelFocus = false;

	if (loadResources)
	{
		const int BufLen = strlen(panelName) + 32;
		char *buf = new char[BufLen];
		Q_snprintf( buf, BufLen, "Resource/Career/%s.res", panelName );
		LoadControlSettings( buf );
		delete[] buf;
	}
}

//--------------------------------------------------------------------------------------------------------------
vgui::Panel * CCareerBaseBox::CreateControlByName(const char *controlName)
{
	if ( Q_stricmp( controlName, "ConVarCheckButton" ) == 0 )
	{
		ConVarToggleCheckButton *button = new ConVarToggleCheckButton( NULL, controlName, "" );
		m_conVarCheckButtons.PutElement( button );
		return button;
	}

	return BaseClass::CreateControlByName( controlName );
}

//--------------------------------------------------------------------------------------------------------------
void CCareerBaseBox::SetCancelButtonAsDefault()
{
	m_cancelFocus = true;
}

//--------------------------------------------------------------------------------------------------------------
void CCareerBaseBox::SetLabelText( const wchar_t *text )
{
	m_pTextLabel->SetText(text);
}

//--------------------------------------------------------------------------------------------------------------
void CCareerBaseBox::SetLabelText( const char *text )
{
	m_pTextLabel->SetText(text);
}

//--------------------------------------------------------------------------------------------------------------
void CCareerBaseBox::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	for (int i=0; i<m_buttons.GetCount(); ++i)
	{
		m_buttons[i]->SetArmedSound("UI/buttonrollover.wav");
		m_buttons[i]->SetDepressedSound("UI/buttonclick.wav");
		m_buttons[i]->SetReleasedSound("UI/buttonclickrelease.wav");
	}

	m_bgColor = GetSchemeColor("Popup.BgColor", Color( 64, 64, 64, 255 ), pScheme);
	m_borderColor = GetSchemeColor("FgColor", Color( 64, 64, 64, 255 ), pScheme);

	SetBorder( pScheme->GetBorder( "BaseBorder" ) );
}

//--------------------------------------------------------------------------------------------------------------
void CCareerBaseBox::PerformLayout( )
{
	BaseClass::PerformLayout();

	int x, y, w, h;
	GetBounds(x, y, w, h);

	int screenWide, screenTall;
	GetHudSize( screenWide, screenTall );
	if ( x + w/2 != screenWide/2 )
	{
		SetPos( screenWide/2 - w/2, screenTall/2 - h/2 );
		GetBounds(x, y, w, h);
	}
}

//--------------------------------------------------------------------------------------------------------------
void CCareerBaseBox::PaintBackground( )
{
	int wide, tall;
	GetSize( wide, tall );

	DrawRoundedBackground( m_bgColor, wide, tall );
}

//--------------------------------------------------------------------------------------------------------------
void CCareerBaseBox::PaintBorder( )
{
	int wide, tall;
	GetSize( wide, tall );

	DrawRoundedBorder( m_borderColor, wide, tall );
}

//--------------------------------------------------------------------------------------------------------------
void CCareerBaseBox::ShowWindow()
{
	SetVisible( true );
	SetEnabled( true );
	MoveToFront();

	if ( m_pCancelButton->IsVisible() && m_cancelFocus )
	{
		m_pCancelButton->RequestFocus();
	}
	else if ( m_pOkButton->IsVisible() )
	{
		m_pOkButton->RequestFocus();
	}
	else	 // handle message boxes with no button
	{
		RequestFocus();
	}

	InvalidateLayout();
}

//--------------------------------------------------------------------------------------------------------------
void CCareerBaseBox::DoModal()
{
	ShowWindow();
	input()->SetAppModalSurface(GetVPanel());
}

//--------------------------------------------------------------------------------------------------------------
void CCareerBaseBox::OnKeyCodeTyped(KeyCode code)
{
	if (code == KEY_ESCAPE)
	{
		OnCommand("Cancel");
	}
	else if (code == KEY_ENTER)
	{
		BaseClass::OnKeyCodeTyped( KEY_SPACE );
	}
	else
	{
		BaseClass::OnKeyCodeTyped(code);
	}
}

//--------------------------------------------------------------------------------------------------------------
void CCareerBaseBox::OnCommand(const char *command)
{
	KeyValues *okSettings = new KeyValues( "GetCommand" );
	if ( m_pOkButton->RequestInfo( okSettings ) )
	{
		const char *okCommand = okSettings->GetString( "command", "Ok" );
		if ( stricmp(command, okCommand) == 0 )
		{
			for (int i=0; i<m_conVarCheckButtons.GetCount(); ++i)
			{
				m_conVarCheckButtons[i]->ApplyChanges();
			}
		}
	}

	if (stricmp(command, "close"))
	{
		PostActionSignal( new KeyValues("Command", "command", command) );
	}

	BaseClass::OnCommand(command);

	Close();
}

//--------------------------------------------------------------------------------------------------------------
void CCareerBaseBox::AddButton( vgui::Button *pButton )
{
	m_buttons.PutElement( pButton );
}

//--------------------------------------------------------------------------------------------------------------
CCareerQueryBox::CCareerQueryBox(vgui::Panel *parent, const char *panelName, const char *resourceName) : CCareerBaseBox(parent, panelName, (resourceName == NULL))
{
	if ( resourceName )
	{
		LoadControlSettings( resourceName );
	}
}

//--------------------------------------------------------------------------------------------------------------
CCareerQueryBox::CCareerQueryBox(const char *title, const char *labelText, const char *panelName, vgui::Panel *parent) : CCareerBaseBox(parent, panelName)
{
}

//--------------------------------------------------------------------------------------------------------------
CCareerQueryBox::CCareerQueryBox(const wchar_t *title, const wchar_t *labelText, const char *panelName, vgui::Panel *parent) : CCareerBaseBox(parent, panelName)
{
}

//--------------------------------------------------------------------------------------------------------------
CCareerQueryBox::~CCareerQueryBox()
{
}

// sorted order weapon list -----------------------------------------------------------------------
const int NUM_SECONDARY_WEAPONS = 7;
static CSWeaponID s_secondaryWeapons[NUM_SECONDARY_WEAPONS] =
{
	WEAPON_NONE,
	WEAPON_USP,
	WEAPON_GLOCK,
	WEAPON_DEAGLE,
	WEAPON_ELITE,
	WEAPON_P228,
	WEAPON_FIVESEVEN,
};

const int NUM_PRIMARY_WEAPONS = 23;
static CSWeaponID s_primaryWeapons[NUM_PRIMARY_WEAPONS] =
{
	WEAPON_NONE,

	// Assault Rifles
	CSWeaponID(-WEAPONTYPE_RIFLE),
	WEAPON_SG552,
	WEAPON_AUG,
	WEAPON_AK47,
	WEAPON_M4A1,
	WEAPON_GALIL,
	WEAPON_FAMAS,

	// Snipers
	CSWeaponID(-WEAPONTYPE_SNIPER_RIFLE),
	WEAPON_AWP,
	WEAPON_SG550,
	WEAPON_G3SG1,
	WEAPON_SCOUT,

	// SMG
	CSWeaponID(-WEAPONTYPE_SUBMACHINEGUN),
	WEAPON_P90,
	WEAPON_UMP45,
	WEAPON_MP5NAVY,
	WEAPON_MAC10,
	WEAPON_TMP,

	// Heavy
	CSWeaponID(-WEAPONTYPE_SHOTGUN),
	WEAPON_M249,
	WEAPON_XM1014,
	WEAPON_M3,
//	WEAPON_SHIELDGUN,
};

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
class CWeaponButton : public vgui::Button
{
	typedef vgui::Button BaseClass;

public:
	CWeaponButton( BuyPresetListBox *pParent, CSWeaponID weaponID, bool isPrimary ) : BaseClass( pParent->GetParent(), SharedVarArgs( "WeaponButton%d", weaponID ), "" )
	{
		m_isPrimary = isPrimary;
		m_pImage = NULL;
		m_pTitleImage = new TextImage( WeaponIDToDisplayName( weaponID ) );
		m_pCostImage = new TextImage( L"" );

		m_pListBox = pParent;
		BuyPresetWeapon weapon( weaponID );
		SetWeapon( weapon );
	}

	virtual ~CWeaponButton() { delete m_pTitleImage; delete m_pCostImage; }

	virtual void ApplySchemeSettings( IScheme *pScheme );

	virtual Color GetBgColor() { return (IsCurrent()) ? m_selectedBgColor : Button::GetBgColor(); }
	virtual Color GetFgColor() { return (IsCurrent()) ? m_selectedFgColor : Button::GetFgColor(); }

	void SetCurrent() { s_current = this; }
	bool IsCurrent() const { return s_current == this; }

	void SetWeapon( const BuyPresetWeapon& weapon );
	const BuyPresetWeapon& GetWeapon() { return m_weapon; }

	virtual void Paint();
	virtual void PerformLayout();
	virtual void PaintBorder() {}

	virtual void OnMousePressed( MouseCode code )
	{
		if ( code == MOUSE_LEFT )
		{
			SetCurrent();
			m_pListBox->GetParent()->OnCommand( "select_weapon" );
		}
		Button::OnMousePressed( code );
	}

	virtual void OnMouseDoublePressed( MouseCode code )
	{
		if ( code == MOUSE_LEFT )
		{
			SetCurrent();
			m_pListBox->GetParent()->OnCommand( "select_weapon" );
			m_pListBox->GetParent()->OnCommand( "popup_ok" );
		}
		Button::OnMouseDoublePressed( code );
	}

protected:
	static CWeaponButton * s_current;
	BuyPresetListBox * m_pListBox;
	BuyPresetWeapon m_weapon;

	IImage *m_pImage;
	TextImage *m_pTitleImage;
	TextImage *m_pCostImage;
	int m_imageWide;
	int m_imageTall;
	int m_imageX;
	int m_imageY;

	Color m_selectedBgColor;
	Color m_selectedFgColor;

	bool m_isPrimary;
};

//--------------------------------------------------------------------------------------------------------------
CWeaponButton * CWeaponButton::s_current = NULL;

//--------------------------------------------------------------------------------------------------------------
void CWeaponButton::ApplySchemeSettings( IScheme *pScheme )
{
	CWeaponButton * current = s_current; // save off current so Button::ApplySchemeSettings() can get the right colors
	s_current = NULL;
	Button::ApplySchemeSettings( pScheme );
	m_selectedBgColor = GetSchemeColor( "Button.ArmedBgColor", Button::GetBgColor(), pScheme );
	m_selectedFgColor = GetSchemeColor( "Button.ArmedTextColor", Button::GetFgColor(), pScheme );
	s_current = current;
	m_pTitleImage->SetColor( pScheme->GetColor( "Button.TextColor", Color() ) );
	m_pTitleImage->SetFont( pScheme->GetFont( "Default", IsProportional() ) );
	m_pTitleImage->SetWrap( true );
	m_pCostImage->SetColor( pScheme->GetColor( "Button.TextColor", Color() ) );
	m_pCostImage->SetFont( pScheme->GetFont( "Default", IsProportional() ) );
	m_pCostImage->SetWrap( true );
	SetWeapon( m_weapon );
	SetBorder( NULL );
}

//--------------------------------------------------------------------------------------------------------------
void CWeaponButton::SetWeapon( const BuyPresetWeapon& weapon )
{
	m_weapon = weapon;
	if ( m_weapon.GetName() )
	{
		// weapon string
		const wchar_t * name = m_weapon.GetName();
		m_pTitleImage->SetText( name );

		// cost string
		CCSWeaponInfo *info = GetWeaponInfo( m_weapon.GetWeaponID() );
		if ( info )
		{
			const int BufLen = 256;
			wchar_t wbuf[BufLen];
			g_pVGuiLocalize->ConstructString( wbuf, sizeof( wbuf ),
				g_pVGuiLocalize->Find( "#Cstrike_BuyPresetPlainCost" ),
				1, NumAsWString( info->GetWeaponPrice() ) );
			m_pCostImage->SetText( wbuf );
		}
		else
		{
			m_pCostImage->SetText( L"" );
		}
	}
	else
	{
		m_pTitleImage->SetText( L"" );
		m_pCostImage->SetText( L"" );
	}

	m_pTitleImage->ResizeImageToContent();
	m_pCostImage->ResizeImageToContent();

	InvalidateLayout( true, false );

	m_pImage = scheme()->GetImage( ImageFnameFromWeaponID( m_weapon.GetWeaponID(), m_isPrimary ), true );
}

//--------------------------------------------------------------------------------------------------------------
void CWeaponButton::PerformLayout()
{
	BaseClass::PerformLayout();

	// Calculate some sizes
	int oldWide, oldTall;
	GetSize( oldWide, oldTall );

	const float IMAGE_PERCENT = 0.42f;
	const float TITLE_PERCENT = 0.45f;
	const float COST_PERCENT = 1.0f - TITLE_PERCENT - IMAGE_PERCENT;

	int maxTitleWide = (int) oldWide * TITLE_PERCENT;
	int maxImageWide = (int) oldWide * IMAGE_PERCENT;
	int maxCostWide = (int) oldWide * COST_PERCENT;
	int textTall = surface()->GetFontTall( m_pTitleImage->GetFont() ) * 2;
	if ( oldTall != textTall )
	{
		oldTall = textTall;
		SetSize( oldWide, oldTall );
		m_pListBox->InvalidateLayout();
	}

	// Position the weapon name
	{
		m_pTitleImage->SetSize( maxTitleWide, oldTall );
		m_pTitleImage->RecalculateNewLinePositions();
		m_pTitleImage->ResizeImageToContent();

		int textContentWide, textContentTall;
		m_pTitleImage->GetSize( textContentWide, textContentTall );

		if ( textContentTall < textTall )
		{
			m_pTitleImage->SetSize( maxTitleWide, textContentTall );
			m_pTitleImage->SetPos( maxImageWide, (textTall - textContentTall) / 2 );
		}
		else
		{
			m_pTitleImage->SetPos( maxImageWide, 0 );
		}
	}

	// Position the weapon cost
	{
		m_pCostImage->SetSize( maxCostWide, oldTall );
		m_pCostImage->RecalculateNewLinePositions();
		m_pCostImage->ResizeImageToContent();

		int textContentWide, textContentTall;
		m_pCostImage->GetSize( textContentWide, textContentTall );

		if ( textContentTall < textTall )
		{
			m_pCostImage->SetSize( maxCostWide, textContentTall );
			m_pCostImage->SetPos( oldWide - maxCostWide, (textTall - textContentTall) / 2 );
		}
		else
		{
			m_pCostImage->SetPos( oldWide - maxCostWide, 0 );
		}
	}


	// Position the weapon image
	m_pImage = scheme()->GetImage( ImageFnameFromWeaponID( m_weapon.GetWeaponID(), m_isPrimary ), true );

	int maxImageTall = textTall;

	m_pImage->GetContentSize( m_imageWide, m_imageTall );
	if ( m_imageTall > maxImageTall )
	{
		m_imageWide = (int) m_imageWide * 1.0f * maxImageTall / m_imageTall;
		m_imageTall = maxImageTall;
	}
	if ( m_imageWide > maxImageWide )
	{
		m_imageTall = (int) m_imageTall * 1.0f * maxImageWide / m_imageWide;
		m_imageWide = maxImageWide;
	}
	m_imageY = (textTall - m_imageTall) / 2;
	m_imageX = (((int) oldWide * IMAGE_PERCENT) - m_imageWide) / 2;
}

//--------------------------------------------------------------------------------------------------------------
void CWeaponButton::Paint()
{
	if ( m_pImage )
	{
		m_pImage->SetSize( m_imageWide, m_imageTall );
		m_pImage->SetPos( m_imageX, m_imageY );
		m_pImage->Paint();
		m_pImage->SetSize( 0, 0 );
	}
	m_pTitleImage->Paint();
	m_pCostImage->Paint();

	BaseClass::Paint();
}

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
class WeaponComboBox : public vgui::ComboBox
{
public:
	WeaponComboBox( CWeaponSelectBox *parent, const char *name, int numEntries, bool editable )
		: ComboBox( parent, name, numEntries, editable )
	{
		m_pBox = parent;
	}

	virtual void OnSetText( const wchar_t *newText )
	{
		ComboBox::OnSetText( newText );
		if ( m_pBox )
			m_pBox->UpdateClips();
	}
	
private:
	CWeaponSelectBox *m_pBox;
};

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
CWeaponSelectBox::CWeaponSelectBox(vgui::Panel *parent, WeaponSet *pWeaponSet, bool isSecondary ) : CCareerBaseBox(parent, "BuyBoxSelectWeapon", false)
{
	m_pWeaponSet = pWeaponSet;
	m_isSecondary = isSecondary;

	m_numWeapons = NUM_PRIMARY_WEAPONS;
	m_weaponIDs = s_primaryWeapons;

	BuyPresetWeapon *weapon;
	if ( !m_isSecondary )
	{
		weapon = &m_pWeaponSet->m_primaryWeapon;
		if ( !IsPrimaryWeapon( weapon->GetWeaponID() ) && weapon->GetWeaponID() != WEAPON_NONE )
		{
			BuyPresetWeapon tmp( WEAPON_NONE );
			m_pWeaponSet->m_primaryWeapon = tmp;
		}
	}
	else
	{
		m_numWeapons = NUM_SECONDARY_WEAPONS;
		m_weaponIDs = s_secondaryWeapons;
		weapon = &m_pWeaponSet->m_secondaryWeapon;
		if ( !IsSecondaryWeapon( weapon->GetWeaponID() ) && weapon->GetWeaponID() != WEAPON_NONE )
		{
			BuyPresetWeapon tmp( WEAPON_NONE );
			*weapon = tmp;
		}
	}

	int maxClips = 0;
	const CCSWeaponInfo *info = GetWeaponInfo( weapon->GetWeaponID() );
	if ( info )
	{
		int maxRounds = GetCSAmmoDef()->MaxCarry( info->iAmmoType );
		int buyClipSize = GetCSAmmoDef()->GetBuySize( info->iAmmoType );
		maxClips = (buyClipSize > 0) ? ceil(maxRounds/(float)buyClipSize) : 0;
	}
	else
	{
		maxClips = NUM_CLIPS_FOR_CURRENT; // so we can buy ammo for our current gun
	}

	m_pClips = new WeaponComboBox( this, "Clips", 2*maxClips+1, false );
	m_pClips->SetOpenDirection( Menu::UP );
	m_pListBox = new BuyPresetListBox( this, "WeaponListBox" );
	m_pBullets = new Label( this, "bullets", "" );

	int selectedWeaponIndex = -1;
	int i;
	for ( i=0; i<m_numWeapons; ++i )
	{
		if ( m_weaponIDs[i] < 0 )
		{
			const char *text = "";
			switch ( (int)m_weaponIDs[i] )
			{
			case -WEAPONTYPE_RIFLE:
				text = "#Cstrike_BuyPresetCategoryRifle";
				break;
			case -WEAPONTYPE_SNIPER_RIFLE:
				text = "#Cstrike_BuyPresetCategorySniper";
				break;
			case -WEAPONTYPE_SUBMACHINEGUN:
				text = "#Cstrike_BuyPresetCategorySMG";
				break;
			case -WEAPONTYPE_SHOTGUN:
				text = "#Cstrike_BuyPresetCategoryHeavy";
				break;
			}
			Label *pLabel = new Label( m_pListBox, SharedVarArgs("weaponlabel%d", i), text );
			m_pListBox->AddItem( pLabel, NULL );
		}
		else
		{
			CWeaponButton *pWeaponButton = new CWeaponButton( m_pListBox, m_weaponIDs[i], !m_isSecondary );
			m_pListBox->AddItem( pWeaponButton, NULL );
			if ( m_weaponIDs[i] == weapon->GetWeaponID() )
			{
				pWeaponButton->SetCurrent();
				selectedWeaponIndex = i;
			}
		}
	}
	m_pListBox->MakeItemVisible( selectedWeaponIndex );

	LoadControlSettings( "resource/UI/BuyPreset/BoxSelectWeapon.res" );
	PopulateControls();
}

//--------------------------------------------------------------------------------------------------------------
CWeaponSelectBox::~CWeaponSelectBox()
{
}

//--------------------------------------------------------------------------------------------------------------
void CWeaponSelectBox::ActivateBuildMode()
{
	SetClipsVisible( true );
	m_pListBox->DeleteAllItems();
	BaseClass::ActivateBuildMode();
}

//--------------------------------------------------------------------------------------------------------------
void CWeaponSelectBox::SetClipsVisible( bool visible )
{
	SetLabelVisible( visible );
	m_pClips->SetVisible( visible );
}

//--------------------------------------------------------------------------------------------------------------
void CWeaponSelectBox::PopulateControls()
{
	BuyPresetWeapon *weapon;
	if (!m_isSecondary)
	{
		weapon = &m_pWeaponSet->m_primaryWeapon;
	}
	else
	{
		weapon = &m_pWeaponSet->m_secondaryWeapon;
	}

	int i;
	int maxClips = 0;
	const CCSWeaponInfo *info = GetWeaponInfo( weapon->GetWeaponID() );
	if ( info )
	{
		int maxRounds = GetCSAmmoDef()->MaxCarry( info->iAmmoType );
		int buyClipSize = GetCSAmmoDef()->GetBuySize( info->iAmmoType );
		maxClips = (buyClipSize > 0) ? ceil(maxRounds/(float)buyClipSize) : 0;
	}
	else
	{
		maxClips = NUM_CLIPS_FOR_CURRENT;
	}
	m_pClips->SetNumberOfEditLines( 2*maxClips+1 );

	SetClipsVisible( maxClips != 0 );

	m_pClips->DeleteAllItems();

	// populate clips combo box
	m_pClips->AddItem( "#Cstrike_BuyPresetEditWeaponFullClips", NULL );

	const int BufLen = 64;
	wchar_t buf[BufLen];
	for ( i=maxClips-1; i>=0; --i )
	{
		const char* clipsOrMore = "#Cstrike_BuyPresetEditClipsOrMore";
		const char* clips = "#Cstrike_BuyPresetEditClips";
		if ( i == 1 )
		{
			clipsOrMore = "#Cstrike_BuyPresetEditClipOrMore";
			clips = "#Cstrike_BuyPresetEditClip";
		}
		g_pVGuiLocalize->ConstructString( buf, sizeof(buf),
			g_pVGuiLocalize->Find( clipsOrMore ),
			1, NumAsWString( i ));
		m_pClips->AddItem( buf, NULL );
		g_pVGuiLocalize->ConstructString( buf, sizeof(buf),
			g_pVGuiLocalize->Find( clips ),
			1, NumAsWString( i ));
		m_pClips->AddItem( buf, NULL );
	}

	// now select the proper entry
	int clipIndexToSelect = weapon->GetFillAmmo();
	clipIndexToSelect += 2 * weapon->GetAmmoAmount();
	clipIndexToSelect = maxClips*2 - clipIndexToSelect;
	m_pClips->ActivateItemByRow( clipIndexToSelect );

	if ( m_isSecondary )
	{
		Panel *pPanel = FindChildByName( "TitleLabel" );
		if ( pPanel )
		{
			const wchar_t *title = g_pVGuiLocalize->Find( "#Cstrike_BuyPresetWizardSecondary" );
			if ( title )
				PostMessage(pPanel, new KeyValues("SetText", "text", title));
		}
	}

	UpdateClips();
}

//--------------------------------------------------------------------------------------------------------------
void CWeaponSelectBox::UpdateClips()
{
	if ( !m_pClips )
		return;

	int numEntries = m_pClips->GetItemCount();
	int activeID = m_pClips->GetActiveItem();
	int combined = numEntries - activeID + 1;
	int numClips = combined/2 - 1;
	//bool isFill = (combined%2) != 0;

	BuyPresetWeapon *weapon;
	if (!m_isSecondary)
	{
		weapon = &m_pWeaponSet->m_primaryWeapon;
	}
	else
	{
		weapon = &m_pWeaponSet->m_secondaryWeapon;
	}

	const CCSWeaponInfo *info = GetWeaponInfo( weapon->GetWeaponID() );
	if ( info )
	{
		int maxRounds = GetCSAmmoDef()->MaxCarry( info->iAmmoType );
		int buyClipSize = GetCSAmmoDef()->GetBuySize( info->iAmmoType );
		m_pBullets->SetVisible( true );

		const int BufLen = 64;
		wchar_t buf[BufLen];
		g_pVGuiLocalize->ConstructString( buf, sizeof(buf),
			g_pVGuiLocalize->Find( "#Cstrike_BuyPresetsBullets" ),
			2, NumAsWString( MIN( maxRounds, numClips * buyClipSize ) ), NumAsWString( maxRounds ) );
		m_pBullets->SetText( buf );
	}
	else
	{
		m_pBullets->SetVisible( false );
	}
}

//--------------------------------------------------------------------------------------------------------------
void CWeaponSelectBox::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings(pScheme);

	HFont font = pScheme->GetFont("Default", IsProportional());
	int tall = surface()->GetFontTall(font) + 2;
	if ( font != INVALID_FONT )
	{
		Menu *pMenu = m_pClips->GetMenu();
		pMenu->SetMenuItemHeight( tall );
	}

	for ( int i=0; i<m_pListBox->GetItemCount(); ++i )
	{
		if ( m_weaponIDs[i] >= 0 )
		{
			CWeaponButton *pButton = static_cast< CWeaponButton * >(m_pListBox->GetItemPanel(i));
			if ( pButton && pButton->IsCurrent() )
			{
				m_pListBox->MakeItemVisible( i );
			}
		}
		else
		{
			// it's a caption label
			Label *pLabel = static_cast< Label * >(m_pListBox->GetItemPanel(i));
			if ( pLabel )
			{
				pLabel->SetContentAlignment( Label::a_center );
				pLabel->SetBorder( pScheme->GetBorder( "ButtonBorder" ) );
				pLabel->SetBgColor( pScheme->GetColor( "Frame.BgColor", Color( 0, 0, 0, 255 ) ) );
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
CSWeaponID CWeaponSelectBox::GetSelectedWeaponID()
{
	int selectedIndex = -1;
	for ( int i=0; i<m_pListBox->GetItemCount(); ++i )
	{
		if ( m_weaponIDs[selectedIndex] >= 0 )
		{
			CWeaponButton *pButton = static_cast< CWeaponButton * >(m_pListBox->GetItemPanel(i));
			if ( pButton && pButton->IsCurrent() )
			{
				selectedIndex = i;
			}
		}
	}
	if ( selectedIndex >= 0 )
	{
		return m_weaponIDs[selectedIndex];
	}

	return WEAPON_NONE;
}

//--------------------------------------------------------------------------------------------------------------
void CWeaponSelectBox::OnCommand(const char *command)
{
	if (!stricmp(command, "select_weapon"))
	{
		CSWeaponID weaponID = GetSelectedWeaponID();
		BuyPresetWeapon weapon( weaponID );
		if ( m_isSecondary )
		{
			m_pWeaponSet->m_secondaryWeapon = weapon;
		}
		else
		{
			m_pWeaponSet->m_primaryWeapon = weapon;
		}

		PopulateControls();
		return;
	}

	if (!stricmp(command, "popup_ok"))
	{
		if ( !m_pClips )
			return;

		int numEntries = m_pClips->GetItemCount();
		int activeID = m_pClips->GetActiveItem();
		int combined = numEntries - activeID + 1;
		int numClips = combined/2 - 1;
		bool isFill = (combined%2) != 0;

		BuyPresetWeapon weapon( GetSelectedWeaponID() );
		weapon.SetAmmoType( AMMO_CLIPS );
		weapon.SetAmmoAmount( numClips );
		weapon.SetFillAmmo( isFill );

		if ( m_isSecondary )
		{
			m_pWeaponSet->m_secondaryWeapon = weapon;
		}
		else
		{
			m_pWeaponSet->m_primaryWeapon = weapon;
		}
		BaseClass::OnCommand( command );
	}
	else
	{
		BaseClass::OnCommand(command);
	}
}

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
class EquipmentComboBox : public ComboBox
{
public:
	EquipmentComboBox( CBaseSelectBox *parent, const char *name, int numEntries, bool editable )
		: ComboBox( parent, name, numEntries, editable )
	{
		m_pBox = parent;
	}

	virtual void OnSetText( const wchar_t *newText )
	{
		ComboBox::OnSetText( newText );
		m_pBox->OnControlChanged();
	}
	
private:
	CBaseSelectBox *m_pBox;
};

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
CGrenadeSelectBox::CGrenadeSelectBox( vgui::Panel *parent, WeaponSet *pWeaponSet ) : BaseClass( parent, "BuyBoxSelectGrenades", false )
{
	m_pWeaponSet = pWeaponSet;

	// Equipment controls
	m_pHEGrenade = new EquipmentComboBox( this, "hegrenade", 2, false );
	m_pSmokeGrenade = new EquipmentComboBox( this, "smokegrenade", 2, false );
	m_pFlashbangs = new EquipmentComboBox( this, "flashbangs", 3, false );

	// Equipment images
	m_pHEGrenadeImage = new EquipmentLabel( this, "HEGrenadeImage" );
	m_pSmokeGrenadeImage = new EquipmentLabel( this, "SmokeGrenadeImage" );
	m_pFlashbangImage = new EquipmentLabel( this, "FlashbangImage" );

	m_pHELabel    = new Label( this, "HECost", "" );
	m_pSmokeLabel = new Label( this, "SmokeCost", "" );
	m_pFlashLabel = new Label( this, "FlashCost", "" );

	LoadControlSettings( "Resource/UI/BuyPreset/BoxSelectGrenades.res" );

	// Add entries to the combo boxes
	m_pHEGrenade->AddItem( "0", NULL );
	m_pHEGrenade->AddItem( "1", NULL );

	m_pSmokeGrenade->AddItem( "0", NULL );
	m_pSmokeGrenade->AddItem( "1", NULL );

	m_pFlashbangs->AddItem( "0", NULL );
	m_pFlashbangs->AddItem( "1", NULL );
	m_pFlashbangs->AddItem( "2", NULL );

	// populate the data
	m_pHEGrenade->ActivateItemByRow( m_pWeaponSet->m_HEGrenade );
	m_pSmokeGrenade->ActivateItemByRow( m_pWeaponSet->m_smokeGrenade );
	m_pFlashbangs->ActivateItemByRow( m_pWeaponSet->m_flashbangs );

	m_pHEGrenadeImage->SetItem( "gfx/vgui/hegrenade_square", 1 );
	m_pSmokeGrenadeImage->SetItem( "gfx/vgui/smokegrenade_square", 1 );
	m_pFlashbangImage->SetItem( "gfx/vgui/flashbang_square", 1 );

	OnControlChanged();
}

//--------------------------------------------------------------------------------------------------------------
void CGrenadeSelectBox::OnControlChanged()
{
	const int BufLen = 256;
	wchar_t wbuf[BufLen];

	CCSWeaponInfo *info;

	int numGrenades;
	info = GetWeaponInfo( WEAPON_HEGRENADE );
	if ( info )
	{
		numGrenades = m_pHEGrenade->GetActiveItem();
		g_pVGuiLocalize->ConstructString( wbuf, sizeof( wbuf ),
			g_pVGuiLocalize->Find( "#Cstrike_BuyPresetPlainCost" ),
			1, NumAsWString( info->GetWeaponPrice() * numGrenades ) );
		m_pHELabel->SetText( wbuf );
	}

	info = GetWeaponInfo( WEAPON_SMOKEGRENADE );
	if ( info )
	{
		numGrenades = m_pSmokeGrenade->GetActiveItem();
		g_pVGuiLocalize->ConstructString( wbuf, sizeof( wbuf ),
			g_pVGuiLocalize->Find( "#Cstrike_BuyPresetPlainCost" ),
			1, NumAsWString( info->GetWeaponPrice() * numGrenades ) );
		m_pSmokeLabel->SetText( wbuf );
	}

	info = GetWeaponInfo( WEAPON_FLASHBANG );
	if ( info )
	{
		numGrenades = m_pFlashbangs->GetActiveItem();
		g_pVGuiLocalize->ConstructString( wbuf, sizeof( wbuf ),
			g_pVGuiLocalize->Find( "#Cstrike_BuyPresetPlainCost" ),
			1, NumAsWString( info->GetWeaponPrice() * numGrenades ) );
		m_pFlashLabel->SetText( wbuf );
	}
}

//--------------------------------------------------------------------------------------------------------------
void CGrenadeSelectBox::OnCommand( const char *command )
{
	if (!stricmp(command, "popup_ok"))
	{
		// stuff values back in m_pWeaponSet
		m_pWeaponSet->m_HEGrenade    = m_pHEGrenade->GetActiveItem();
		m_pWeaponSet->m_smokeGrenade = m_pSmokeGrenade->GetActiveItem();
		m_pWeaponSet->m_flashbangs   = m_pFlashbangs->GetActiveItem();
	}
	BaseClass::OnCommand( command );
}

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
CEquipmentSelectBox::CEquipmentSelectBox( vgui::Panel *parent, WeaponSet *pWeaponSet ) : BaseClass( parent, "BuyBoxSelectEquipment", false )
{
	m_pWeaponSet = pWeaponSet;

	// Equipment controls
	m_pKevlar      = new EquipmentComboBox( this, "kevlar", 2, false );
	m_pHelmet      = new EquipmentComboBox( this, "helmet", 2, false );
	m_pDefuser     = new EquipmentComboBox( this, "defuser", 2, false );
	m_pNightvision = new EquipmentComboBox( this, "nightvision", 2, false );

	// Equipment labels
	m_pKevlarLabel      = new Label( this, "kevlarCost", "" );
	m_pHelmetLabel      = new Label( this, "helmetCost", "" );
	m_pDefuserLabel     = new Label( this, "defuserCost", "" );
	m_pNightvisionLabel = new Label( this, "nightvisionCost", "" );

	// Equipment images
	m_pKevlarImage      = new EquipmentLabel( this, "KevlarImage" );
	m_pHelmetImage      = new EquipmentLabel( this, "HelmetImage" );
	m_pDefuserImage     = new EquipmentLabel( this, "DefuserImage" );
	m_pNightvisionImage = new EquipmentLabel( this, "NightvisionImage" );

	LoadControlSettings( "Resource/UI/BuyPreset/BoxSelectEquipment.res" );

	// Add entries to the combo boxes
	m_pKevlar->AddItem( "0", NULL );
	m_pKevlar->AddItem( "1", NULL );

	m_pHelmet->AddItem( "0", NULL );
	m_pHelmet->AddItem( "1", NULL );

	m_pDefuser->AddItem( "0", NULL );
	m_pDefuser->AddItem( "1", NULL );

	m_pNightvision->AddItem( "0", NULL );
	m_pNightvision->AddItem( "1", NULL );

	// populate the data
	m_pKevlar->ActivateItemByRow( ( m_pWeaponSet->m_armor > 0 ) );
	m_pHelmet->ActivateItemByRow( ( m_pWeaponSet->m_armor && m_pWeaponSet->m_helmet ) );
	m_pDefuser->ActivateItemByRow( m_pWeaponSet->m_defuser );
	m_pNightvision->ActivateItemByRow( m_pWeaponSet->m_nightvision );

	m_pKevlarImage->SetItem( "gfx/vgui/kevlar", 1 );
	m_pHelmetImage->SetItem( "gfx/vgui/helmet", 1 );
	m_pDefuserImage->SetItem( "gfx/vgui/defuser", 1 );
	m_pNightvisionImage->SetItem( "gfx/vgui/nightvision", 1 );

	OnControlChanged();
}

//--------------------------------------------------------------------------------------------------------------
void CEquipmentSelectBox::OnControlChanged()
{
	const int BufLen = 256;
	wchar_t wbuf[BufLen];

	int iHelmetPrice = HELMET_PRICE;
	int iKevlarPrice = KEVLAR_PRICE;
	int iNVGPrice = NVG_PRICE;

	if ( CSGameRules()->IsBlackMarket() )
	{
		iHelmetPrice = CSGameRules()->GetBlackMarketPriceForWeapon( WEAPON_ASSAULTSUIT ) - CSGameRules()->GetBlackMarketPriceForWeapon( WEAPON_KEVLAR );
		iKevlarPrice = CSGameRules()->GetBlackMarketPriceForWeapon( WEAPON_KEVLAR );
		iNVGPrice = CSGameRules()->GetBlackMarketPriceForWeapon( WEAPON_NVG );
	}

	int count = m_pKevlar->GetActiveItem();
	g_pVGuiLocalize->ConstructString( wbuf, sizeof( wbuf ),
		g_pVGuiLocalize->Find( "#Cstrike_BuyPresetPlainCost" ),
		1, NumAsWString( iKevlarPrice * count ) );
	m_pKevlarLabel->SetText( wbuf );

	m_pHelmet->SetEnabled( count );
	if ( !count && m_pHelmet->GetActiveItem() )
		m_pHelmet->ActivateItemByRow( 0 );

	count = m_pHelmet->GetActiveItem();
	g_pVGuiLocalize->ConstructString( wbuf, sizeof( wbuf ),
		g_pVGuiLocalize->Find( "#Cstrike_BuyPresetPlainCost" ),
		1, NumAsWString( iHelmetPrice * count ) );
	m_pHelmetLabel->SetText( wbuf );

	count = m_pDefuser->GetActiveItem();
	g_pVGuiLocalize->ConstructString( wbuf, sizeof( wbuf ),
		g_pVGuiLocalize->Find( "#Cstrike_BuyPresetPlainCost" ),
		1, NumAsWString( DEFUSEKIT_PRICE * count ) );
	m_pDefuserLabel->SetText( wbuf );

	count = m_pNightvision->GetActiveItem();
	g_pVGuiLocalize->ConstructString( wbuf, sizeof( wbuf ),
		g_pVGuiLocalize->Find( "#Cstrike_BuyPresetPlainCost" ),
		1, NumAsWString( NVG_PRICE * count ) );
	m_pNightvisionLabel->SetText( wbuf );
}

//--------------------------------------------------------------------------------------------------------------
void CEquipmentSelectBox::OnCommand( const char *command )
{
	if (!stricmp(command, "popup_ok"))
	{
		// stuff values back in m_pWeaponSet
		m_pWeaponSet->m_armor       = m_pKevlar->GetActiveItem() ? 100 : 0;
		m_pWeaponSet->m_helmet      = m_pWeaponSet->m_armor && m_pHelmet->GetActiveItem();
		m_pWeaponSet->m_defuser     = m_pDefuser->GetActiveItem();
		m_pWeaponSet->m_nightvision = m_pNightvision->GetActiveItem();
	}
	BaseClass::OnCommand( command );
}

//--------------------------------------------------------------------------------------------------------------
