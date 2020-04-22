//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "cstrikebuysubmenu.h"
#include "cstrikebuymenu.h"
#include "cs_shareddefs.h"
#include "backgroundpanel.h"
#include "buy_presets/buy_presets.h"
#include "cstrike/bot/shared_util.h"
#include <vgui/ISurface.h>
#include <vgui/ILocalize.h>
#include "buypreset_weaponsetlabel.h"
#include "career_box.h"
#include "cs_gamerules.h"
#include "vgui_controls/RichText.h"
#include "cs_weapon_parse.h"
#include "c_cs_player.h"
#include "cs_ammodef.h"


using namespace vgui;

//-----------------------------------------------------------------------------
/**
 *  This button resizes any images to fit in the width/height constraints
 */
class BuyPresetButton : public vgui::Button
{
	typedef vgui::Button BaseClass;

public:
	BuyPresetButton(vgui::Panel *parent, const char *buttonName, const char *buttonText );
	virtual ~BuyPresetButton();

	virtual void PerformLayout( void );
	virtual void ClearImages( void );

	virtual void SetFgColor( Color c )
	{
		BaseClass::SetFgColor( c );
	}

	void SetAvailable( bool available )
	{
		m_available = available;
	}

	virtual int AddImage( vgui::IImage *image, int offset )
	{
		if ( image )
		{
			if ( !m_available )
			{
                image->SetColor( Color( 128, 128, 128, 255 ) );
			}
		}
		return BaseClass::AddImage( image, offset );
	}

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );
		m_availableColor = pScheme->GetColor( "Label.TextColor", Color( 0, 0, 0, 0 ) );
		m_unavailableColor = pScheme->GetColor( "Label.DisabledFgColor2", Color( 0, 0, 0, 0 ) );
	}

	virtual Color GetButtonFgColor( void )
	{
		return ( m_available ) ? m_availableColor : m_unavailableColor;
	}

private:

	bool m_available;
	Color m_availableColor;
	Color m_unavailableColor;
};

//-----------------------------------------------------------------------------
BuyPresetButton::BuyPresetButton(vgui::Panel *parent, const char *buttonName, const char *buttonText ) : Button( parent, buttonName, buttonText )
{
	m_available = false;
}

//-----------------------------------------------------------------------------
BuyPresetButton::~BuyPresetButton()
{
	ClearImages();
}

//-----------------------------------------------------------------------------
void BuyPresetButton::ClearImages( void )
{
	int imageCount = GetImageCount();
	for ( int i=0; i<imageCount; ++i )
	{
		BuyPresetImage *image = dynamic_cast< BuyPresetImage * >(GetImageAtIndex( i ));
		if ( image )
		{
			delete image;
		}
	}

	Button::ClearImages();
}

//-----------------------------------------------------------------------------
void BuyPresetButton::PerformLayout( void )
{
	// resize images
	int imageCount = GetImageCount();
	if ( imageCount > 1 )
	{
		int wide, tall;
		GetSize( wide, tall );

		for ( int i=1; i<imageCount; ++i )
		{
			IImage *image = GetImageAtIndex( i );
			if ( image )
			{
				int imageWide, imageTall;
				image->GetSize( imageWide, imageTall );

				float scaleX = 1.0f, scaleY = 1.0f;
				float widthPercent = 0.2f;
				if ( i == 1 )
				{
					widthPercent = 0.6f;
				}

				if ( imageWide > wide * widthPercent )
				{
					scaleX = (float)wide * widthPercent / (float)imageWide;
				}
				if ( imageTall > tall )
				{
					scaleY = (float)tall / (float)imageTall;
				}

				float scale = MIN( scaleX, scaleY );
				if ( scale < 1.0f )
				{
					imageWide *= scale;
					imageTall *= scale;
					image->SetSize( imageWide, imageTall );
				}
			}
		}
	}

	Button::PerformLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCSBuyMenu_CT::CCSBuyMenu_CT(IViewPort *pViewPort) : CCSBaseBuyMenu( pViewPort, "BuySubMenu_CT" )
{
	m_pMainMenu->LoadControlSettings( "Resource/UI/BuyMenu_CT.res" );
	m_pMainMenu->SetVisible( false );

	m_iTeam = TEAM_CT;

	CreateBackground( this );
	m_backgroundLayoutFinished = false;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCSBuyMenu_TER::CCSBuyMenu_TER(IViewPort *pViewPort) : CCSBaseBuyMenu( pViewPort, "BuySubMenu_TER" )
{
	m_pMainMenu->LoadControlSettings( "Resource/UI/BuyMenu_TER.res" );
	m_pMainMenu->SetVisible( false );

	m_iTeam = TEAM_TERRORIST;

	CreateBackground( this );
	m_backgroundLayoutFinished = false;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCSBaseBuyMenu::CCSBaseBuyMenu(IViewPort *pViewPort, const char *subPanelName) : CBuyMenu( pViewPort )
{
	SetTitle( "#Cstrike_Buy_Menu", true);

	SetProportional( true );

	m_pMainMenu = new CCSBuySubMenu( this, subPanelName );
	m_pMainMenu->SetSize( 10, 10 ); // Quiet "parent not sized yet" spew
#if USE_BUY_PRESETS
	for ( int i=0; i<NUM_BUY_PRESET_BUTTONS; ++i )
	{
		m_pBuyPresetButtons[i] = new BuyPresetButton( m_pMainMenu, VarArgs( "BuyPresetButton%c", 'A' + i ), "" );
	}
 	m_pMoney = new Label( m_pMainMenu, "money", "" );
	//=============================================================================
	// HPE_BEGIN:
	// [pfreese] mainBackground was the little orange box outside that buy window
	// that shouldn't have been there. Maybe this was left over from some
	// copied code.
	//=============================================================================
	
	m_pMainBackground = NULL;
//	m_pMainBackground = new Panel( m_pMainMenu, "mainBackground" );
	
	//=============================================================================
	// HPE_END
	//=============================================================================
	m_pLoadout = new BuyPresetEditPanel( m_pMainMenu, "loadoutPanel", "Resource/UI/Loadout.res", 0, false );
#else
	for ( int i=0; i<NUM_BUY_PRESET_BUTTONS; ++i )
	{
		m_pBuyPresetButtons[i] = NULL;
	}
	m_pMoney = NULL;
	m_pMainBackground = NULL;
#endif // USE_BUY_PRESETS
 	m_lastMoney = -1;

	m_pBlackMarket = new EditablePanel( m_pMainMenu, "BlackMarket_Bargains" );
	m_pBlackMarket->LoadControlSettings( "Resource/UI/BlackMarket_Bargains.res" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSBaseBuyMenu::SetVisible(bool state)
{
	BaseClass::SetVisible(state);

	if ( state )
	{
		Panel *defaultButton = FindChildByName( "CancelButton" );
		if ( defaultButton )
		{
			defaultButton->RequestFocus();
		}
		SetMouseInputEnabled( true );
		m_pMainMenu->SetMouseInputEnabled( true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: shows/hides the buy menu
//-----------------------------------------------------------------------------
void CCSBaseBuyMenu::ShowPanel(bool bShow)
{
	CBuyMenu::ShowPanel( bShow );

#if USE_BUY_PRESETS
	if ( bShow )
	{
		UpdateBuyPresets( true );
	}
#endif // USE_BUY_PRESETS
}

//-----------------------------------------------------------------------------
static void GetPanelBounds( Panel *pPanel, wrect_t& bounds )
{
	if ( !pPanel )
	{
		bounds.bottom = bounds.left = bounds.right = bounds.top = 0;
	}
	else
	{
		pPanel->GetBounds( bounds.left, bounds.top, bounds.right, bounds.bottom );
		bounds.right += bounds.left;
		bounds.bottom += bounds.top;
	}
}

//-----------------------------------------------------------------------------
void CCSBaseBuyMenu::Paint()
{
#if USE_BUY_PRESETS
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	int account = (pPlayer) ? pPlayer->GetAccount() : 0;

	if ( m_pMoney && m_lastMoney != account )
	{
		m_lastMoney = account;

		const int BufLen = 128;
		wchar_t wbuf[BufLen] = L"";
		const wchar_t *formatStr = g_pVGuiLocalize->Find("#Cstrike_Current_Money");
		if ( !formatStr )
			formatStr = L"%s1";
		g_pVGuiLocalize->ConstructString( wbuf, sizeof(wbuf), formatStr, 1, NumAsWString( m_lastMoney ) );
		m_pMoney->SetText( wbuf );
	}
#endif // USE_BUY_PRESETS

	CBuyMenu::Paint();
}

//-----------------------------------------------------------------------------
void CCSBaseBuyMenu::UpdateBuyPresets( bool showDefaultPanel )
{
	bool setPanelVisible = false;
	if ( !showDefaultPanel )
	{
		setPanelVisible = true;
	}

	if ( !TheBuyPresets )
		TheBuyPresets = new BuyPresetManager();

	int i;
	// show buy preset buttons
	int numPresets = MIN( TheBuyPresets->GetNumPresets(), NUM_BUY_PRESET_BUTTONS );
	for ( i=0; i<numPresets; ++i )
	{
		if ( !m_pBuyPresetButtons[i] )
			continue;

		const BuyPreset *preset = TheBuyPresets->GetPreset(i);

		int setIndex;
		int currentCost = -1;
		WeaponSet currentSet;
		const WeaponSet *fullSet = NULL;
		for ( setIndex = 0; setIndex < preset->GetNumSets(); ++setIndex )
		{
			const WeaponSet *itemSet = preset->GetSet( setIndex );
			if ( itemSet )
			{
				itemSet->GetCurrent( currentCost, currentSet );
				if ( currentCost >= 0 )
				{
					fullSet = itemSet;
					break;
				}
			}
		}

		if ( !fullSet && preset->GetNumSets() )
		{
			fullSet = preset->GetSet( 0 );
		}

		// set the button's images
		m_pBuyPresetButtons[i]->ClearImages();
		m_pBuyPresetButtons[i]->SetTextImageIndex( 0 );
		m_pBuyPresetButtons[i]->SetText( "" );

		m_pBuyPresetButtons[i]->SetAvailable( currentCost >= 0 );

		const char *imageName = "";
		if ( fullSet )
		{
			if ( fullSet->GetPrimaryWeapon().GetWeaponID() != WEAPON_NONE )
			{
				imageName = ImageFnameFromWeaponID( fullSet->GetPrimaryWeapon().GetWeaponID(), true );
				BuyPresetImage * image = new BuyPresetImage( scheme()->GetImage(imageName, true) );
				m_pBuyPresetButtons[i]->AddImage( image, 0 );
			}
			if ( fullSet->GetSecondaryWeapon().GetWeaponID() != WEAPON_NONE )
			{
				imageName = ImageFnameFromWeaponID( fullSet->GetSecondaryWeapon().GetWeaponID(), false );
				BuyPresetImage * image = new BuyPresetImage( scheme()->GetImage(imageName, true) );
				m_pBuyPresetButtons[i]->AddImage( image, 0 );
			}
		}

		int displayCost = currentCost;
		if ( displayCost < 0 )
			displayCost = 0;

		const int BufLen = 1024;
		char aBuf[BufLen];
		Q_snprintf(aBuf, BufLen, "#Cstrike_BuyMenuPreset%d", i + 1);
		m_pBuyPresetButtons[i]->SetText( g_pVGuiLocalize->Find(aBuf) );
		Q_snprintf(aBuf, BufLen, "cl_buy_favorite %d", i + 1);
		m_pBuyPresetButtons[i]->SetCommand( aBuf );
		m_pBuyPresetButtons[i]->SetVisible( true );
		m_pBuyPresetButtons[i]->SetEnabled( true );
	}

	// hide unused buy preset buttons
	for ( i=numPresets+1; i<NUM_BUY_PRESET_BUTTONS; ++i )
	{
		if ( m_pBuyPresetButtons[i] )
		{
			m_pBuyPresetButtons[i]->SetVisible( false );
			m_pBuyPresetButtons[i]->SetEnabled( true );
		}
	}

	HandleBlackMarket();
}

const char *g_pWeaponNames[] =
{
	" ",
	"#Cstrike_TitlesTXT_P228",
	"#Cstrike_TitlesTXT_Glock18",
	"#Cstrike_TitlesTXT_Scout",
	"#Cstrike_TitlesTXT_HE_Grenade",
	"#Cstrike_TitlesTXT_XM1014",
	" ",
	"#Cstrike_TitlesTXT_Mac10",
	"#Cstrike_TitlesTXT_Aug",
	"#Cstrike_TitlesTXT_Smoke_Grenade",
	"#Cstrike_TitlesTXT_Dual40",
	"#Cstrike_TitlesTXT_FiveSeven",
	"#Cstrike_TitlesTXT_UMP45",
	"#Cstrike_TitlesTXT_SG550",
	"#Cstrike_TitlesTXT_Galil",
	"#Cstrike_TitlesTXT_Famas",
	"#Cstrike_TitlesTXT_USP45",
	"#Cstrike_TitlesTXT_Magnum",
	"#Cstrike_TitlesTXT_mp5navy",
	"#Cstrike_TitlesTXT_ESM249",
	"#Cstrike_TitlesTXT_Leone12",
	"#Cstrike_TitlesTXT_M4A1",
	"#Cstrike_TitlesTXT_tmp",
	"#Cstrike_TitlesTXT_G3SG1",
	"#Cstrike_TitlesTXT_Flashbang",
	"#Cstrike_TitlesTXT_DesertEagle",
	"#Cstrike_TitlesTXT_SG552",
	"#Cstrike_TitlesTXT_AK47",
	" ",
	"#Cstrike_TitlesTXT_FNP90",
	" ",
	"#Cstrike_TitlesTXT_Kevlar_Vest",
	"#Cstrike_TitlesTXT_Kevlar_Vest_Ballistic_Helmet",
	"#Cstrike_TitlesTXT_Nightvision_Goggles"
};

int GetWeeklyBargain( void )
{
	if ( CSGameRules() == NULL || CSGameRules()->m_pPrices == NULL )
		return 0;

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( pPlayer == NULL )
		return 0;

	int iBestIndex = 0;
	int iBestBargain = 99999;

	for ( int i = 1; i < WEAPON_MAX; i++ )
	{
		if ( i == WEAPON_SHIELDGUN )
			continue;

		CCSWeaponInfo *info = GetWeaponInfo( (CSWeaponID)i );

		if ( info == NULL )
			continue;

		if ( info->m_iTeam == TEAM_UNASSIGNED || info->m_iTeam == pPlayer->m_iTeamNum )
		{		
			int iBargain = info->GetWeaponPrice() - info->GetPrevousPrice();

			if ( iBargain < iBestBargain )
			{
				iBestIndex = i;
				iBestBargain = iBargain;
			}
		}
	}

	return iBestIndex;
}

#ifdef _DEBUG
ConVar cs_testbargain( "cs_testbargain", "1" );
#endif

void CCSBaseBuyMenu::HandleBlackMarket( void )
{	
	if ( CSGameRules() == NULL )
		return;

	if ( m_pLoadout )
	{
		if ( CSGameRules()->IsBlackMarket() )
		{
			if ( CSGameRules()->m_pPrices == NULL )
				return;

			if ( m_pBlackMarket == NULL )
				return;

			int iBargain = GetWeeklyBargain();
			CCSWeaponInfo *info = GetWeaponInfo( (CSWeaponID)iBargain );

			wchar_t *wszWeaponName = g_pVGuiLocalize->Find( g_pWeaponNames[iBargain]);

			if ( wszWeaponName == NULL )
				return;

			if ( info == NULL )
				return;

			m_pLoadout->SetVisible( false );
			Label *pLabel = dynamic_cast< Label * >(m_pMainMenu->FindChildByName( "loadoutLabel" ));

			if ( pLabel )
			{
				pLabel->SetVisible( false );
			}

			pLabel = dynamic_cast< Label * >(m_pBlackMarket->FindChildByName( "MarketHeadline" ));

			if ( pLabel )
			{
				const int BufLen = 2048;
				
				wchar_t wbuf[BufLen] = L"";
				const wchar_t *formatStr = g_pVGuiLocalize->Find("#Cstrike_MarketHeadline");
	
				if ( !formatStr )
					formatStr = L"%s1";

				g_pVGuiLocalize->ConstructString( wbuf, sizeof(wbuf), formatStr, 1, wszWeaponName );
				pLabel->SetText( wbuf );
			}

			pLabel = dynamic_cast< Label * >(m_pBlackMarket->FindChildByName( "MarketBargain" ));

			if ( pLabel )
			{
				const int BufLen = 2048;
				wchar_t wbuf[BufLen] = L"";
				const wchar_t *formatStr = g_pVGuiLocalize->Find("#Cstrike_MarketBargain");
				
				if ( !formatStr )
					formatStr = L"%s1";

				g_pVGuiLocalize->ConstructString( wbuf, sizeof(wbuf), formatStr, 1, wszWeaponName );
				pLabel->SetText( wbuf );
			}

			pLabel = dynamic_cast< Label * >(m_pBlackMarket->FindChildByName( "MarketStickerPrice" ));

			if ( pLabel )
			{
				char wbuf[16];

				Q_snprintf( wbuf, 16, "%d", CSGameRules()->m_pPrices->iCurrentPrice[iBargain] );

				pLabel->SetText( wbuf );
			}

			RichText *pText = dynamic_cast< RichText * >(m_pBlackMarket->FindChildByName( "MarketDescription" ));

			if ( pText )
			{
				char wbuf[2048];
				g_pVGuiLocalize->ConvertUnicodeToANSI( g_pVGuiLocalize->Find("#Cstrike_MarketDescription"), wbuf, 2048 );

				pText->SetText( "" );
				pText->InsertPossibleURLString( wbuf, Color( 255, 255, 255, 255 ), Color( 255, 176, 0, 255 ) );
				pText->SetVerticalScrollbar( false );
				pText->SetPaintBorderEnabled( false );
				pText->SetUnderlineFont( m_hUnderlineFont );
			}

			pLabel = dynamic_cast< Label * >(m_pBlackMarket->FindChildByName( "MarketBargainIcon" ));
			
			if ( pLabel )
			{
				char wbuff[12];
				Q_snprintf( wbuff, 12, "%c", info->iconActive->cCharacterInFont );
				
				pLabel->SetText( wbuff );
			}

			Button *pButton = dynamic_cast< Button * >(m_pMainMenu->FindChildByName( "BargainbuyButton" ));

			if ( pButton )
			{
				char command[512];
				char *pWeaponName = Q_stristr( info->szClassName, "_" );

				if ( pWeaponName )
				{
					pWeaponName++;

					Q_snprintf( command, 512, "buy %s", pWeaponName );
				}

				pButton->SetCommand( command );
				pButton->SetVisible( true );
			}


			m_pBlackMarket->SetVisible( true );
			m_pBlackMarket->SetZPos( -2 );
		}
		else
		{
			WeaponSet ws;

			TheBuyPresets->GetCurrentLoadout( &ws );
			m_pLoadout->SetWeaponSet( &ws, true );

			m_pLoadout->SetVisible( true );
			Panel *pLabel = dynamic_cast< Label * >(m_pMainMenu->FindChildByName( "loadoutLabel" ));

			if ( pLabel )
			{
				pLabel->SetVisible( true );
			}

			if ( m_pBlackMarket )
			{
				m_pBlackMarket->SetVisible( false );

				Button *pButton = dynamic_cast< Button * >(m_pMainMenu->FindChildByName( "BargainbuyButton" ));

				if ( pButton )
				{
					pButton->SetVisible( false );
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: The CS background is painted by image panels, so we should do nothing
//-----------------------------------------------------------------------------
void CCSBaseBuyMenu::PaintBackground()
{
}

//-----------------------------------------------------------------------------
// Purpose: Scale / center the window
//-----------------------------------------------------------------------------
void CCSBaseBuyMenu::PerformLayout()
{
	BaseClass::PerformLayout();

	// stretch the window to fullscreen
	if ( !m_backgroundLayoutFinished )
		LayoutBackgroundPanel( this );
	m_backgroundLayoutFinished = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSBaseBuyMenu::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	ApplyBackgroundSchemeSettings( this, pScheme );

	if ( m_pMainBackground )
	{
		m_pMainBackground->SetBorder(pScheme->GetBorder("ButtonDepressedBorder"));
		m_pMainBackground->SetBgColor( GetSchemeColor( "Button.BgColor", GetBgColor(), pScheme ) );
	}

	m_hUnderlineFont = pScheme->GetFont( "CSUnderline", IsProportional() );

#if USE_BUY_PRESETS
	UpdateBuyPresets( true );
#endif // USE_BUY_PRESETS
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static bool IsWeaponInvalid( CSWeaponID weaponID )
{
	if ( weaponID == WEAPON_NONE )
		return false;

	return !CanBuyWeapon( WEAPON_NONE, WEAPON_NONE, weaponID );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSBuySubMenu::OnThink()
{
	UpdateVestHelmPrice();
	BaseClass::OnThink();
}

//-----------------------------------------------------------------------------
// Purpose: When buying vest+helmet, if you already have a vest with no damage
// then the price is reduced to just the helmet.  Because this can change during
// the game, we need to update the enable/disable state of the menu item dynamically.
//-----------------------------------------------------------------------------
void CCSBuySubMenu::UpdateVestHelmPrice()
{
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( pPlayer == NULL )
		return;

	BuyMouseOverPanelButton *pButton = dynamic_cast< BuyMouseOverPanelButton * > ( FindChildByName( "kevlar_helmet", false ) );
	if ( pButton )
	{
		// Set its price to the current value from the player.
		pButton->SetCurrentPrice( pPlayer->GetCurrentAssaultSuitPrice() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSBuySubMenu::OnCommand( const char *command )
{
#if USE_BUY_PRESETS
	const char *buyPresetSetString = "cl_buy_favorite_query_set ";
	if ( !strnicmp( command, buyPresetSetString, strlen( buyPresetSetString ) ) )
	{
		bool invalid = IsWeaponInvalid( GetClientWeaponID( true ) ) || IsWeaponInvalid( GetClientWeaponID( false ) );
		if ( invalid )
		{
			// can't save the favorite because it has an invalid weapon (colt for a T, etc)
			C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
			if ( pPlayer )
			{
				pPlayer->EmitSound( "BuyPreset.CantBuy" );
			}

			if ( cl_buy_favorite_nowarn.GetBool() )
			{
				BaseClass::OnCommand( "vguicancel" );
			}
			else
			{
				CCareerQueryBox *pBox = new CCareerQueryBox( this, "SetLoadoutError", "Resource/UI/SetLoadoutError.res" );
				pBox->AddActionSignalTarget( this );
				pBox->DoModal();
			}
		}
		else
		{
			// can save
			if ( cl_buy_favorite_quiet.GetBool() )
			{
				BaseClass::OnCommand( VarArgs( "cl_buy_favorite_set %d", atoi( command + strlen( buyPresetSetString ) ) ) );
			}
			else
			{
				CCareerQueryBox *pBox = new CCareerQueryBox( this, "SetLoadoutQuery", "Resource/UI/SetLoadoutQuery.res" );
				pBox->SetCancelButtonAsDefault();
				if ( pBox->GetOkButton() )
				{
					pBox->GetOkButton()->SetCommand( VarArgs( "cl_buy_favorite_set %d", atoi( command + strlen( buyPresetSetString ) ) ) );
				}
				pBox->AddActionSignalTarget( this );
				pBox->DoModal();
			}
		}
		return;
	}
#endif // USE_BUY_PRESETS

	if ( FStrEq( command, "buy_unavailable" ) )
	{
		C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
		if ( pPlayer )
		{
			pPlayer->EmitSound( "BuyPreset.CantBuy" );
		}
		BaseClass::OnCommand( "vguicancel" );
		return;
	}

	BaseClass::OnCommand( command );
}

void CCSBuySubMenu::OnSizeChanged(int newWide, int newTall)
{
	m_backgroundLayoutFinished = false;
	BaseClass::OnSizeChanged( newWide, newTall );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSBuySubMenu::PerformLayout()
{
	BaseClass::PerformLayout();

	// Buy submenus need to be shoved over for widescreen
	int screenW, screenH;
	GetHudSize( screenW, screenH );

	int fullW, fullH;
	fullW = scheme()->GetProportionalScaledValueEx( GetScheme(), 640 );
	fullH = scheme()->GetProportionalScaledValueEx( GetScheme(), 480 );

	fullW = GetAlternateProportionalValueFromScaled( GetScheme(), fullW );
	fullH = GetAlternateProportionalValueFromScaled( GetScheme(), fullH );

	int offsetX = (screenW - fullW)/2;
	int offsetY = (screenH - fullH)/2;

	if ( !m_backgroundLayoutFinished )
		ResizeWindowControls( this, GetWide(), GetTall(), offsetX, offsetY );
	m_backgroundLayoutFinished = true;

	HandleBlackMarket();
}

void CCSBuySubMenu::HandleBlackMarket( void )
{
	if ( CSGameRules() == NULL )
		return;

	int iBestBargain = 99999;
	BuyMouseOverPanelButton *pButtonBargain = NULL;

	for (int i = 0; i < GetChildCount(); i++)
	{
		BuyMouseOverPanelButton *pButton = dynamic_cast< BuyMouseOverPanelButton * > ( GetChild(i) );
		if (!pButton)
			continue;
		
		pButton->SetBargainButton( false );

		const char *pWeaponName = Q_stristr( pButton->GetBuyCommand(), " " );

		if ( pWeaponName )
		{
			pWeaponName++;

			int iWeaponID = AliasToWeaponID(GetTranslatedWeaponAlias(pWeaponName));

			if ( iWeaponID == 0 )
				continue;

			CCSWeaponInfo *info = GetWeaponInfo( (CSWeaponID)iWeaponID );

			if ( info == NULL )
				continue;

			if ( CSGameRules()->IsBlackMarket() == false )
			{
                //=============================================================================
                // HPE_BEGIN:
                // [dwenger] Removed to avoid clearing of default price when not in black market mode
                //=============================================================================

                // pButton->SetCurrentPrice( info->GetDefaultPrice() );

                //=============================================================================
                // HPE_END
                //=============================================================================
			}
			else
			{
				int iBargain = info->GetWeaponPrice() - info->GetPrevousPrice();

				pButton->SetCurrentPrice( info->GetWeaponPrice() );
				pButton->SetPreviousPrice( info->GetPrevousPrice() );

				if ( iBargain < iBestBargain )
				{
					iBestBargain = iBargain;
					pButtonBargain = pButton;
				}
			}
		}
	}

	if ( pButtonBargain )
	{
		pButtonBargain->SetBargainButton( true );
	}
}



