//===== Copyright © 1996-2008, Valve Corporation, All rights reserved. ======//
//
// Purpose: Tip display during level loads.
//
//===========================================================================//

#include "loadingtippanel.h"
#include "filesystem.h"
#include "keyvalues.h"
#include "vgui/isurface.h"
#include "EngineInterface.h"
#include "vstdlib/random.h"
#include "fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

ConVar ui_loading_tip_refresh( "ui_loading_tip_refresh", "5", FCVAR_DEVELOPMENTONLY );
ConVar ui_loading_tip_f1( "ui_loading_tip_f1", "0.05", FCVAR_DEVELOPMENTONLY );
ConVar ui_loading_tip_f2( "ui_loading_tip_f2", "0.40", FCVAR_DEVELOPMENTONLY );

//--------------------------------------------------------------------------------------------------------
CLoadingTipPanel::CLoadingTipPanel( Panel *pParent ) : EditablePanel( pParent, "loadingtippanel" )
{
	m_flLastTipTime = 0.f;
	m_iCurrentTip = 0;
	m_pTipIcon = NULL;

	m_smearColor = Color( 0, 0, 0, 255 );

	SetupTips();
}

//--------------------------------------------------------------------------------------------------------
CLoadingTipPanel::~CLoadingTipPanel()
{
}

//--------------------------------------------------------------------------------------------------------
void CLoadingTipPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_smearColor = pScheme->GetColor( "Frame.SmearColor", Color( 0, 0, 0, 225 ) );

	ReloadScheme();
}

//--------------------------------------------------------------------------------------------------------
void CLoadingTipPanel::ReloadScheme( void )
{
	LoadControlSettings( "Resource/UI/loadingtippanel.res" );

	m_pTipIcon = dynamic_cast< vgui::ImagePanel* >( FindChildByName( "TipIcon" ) );

	NextTip();
}

//--------------------------------------------------------------------------------------------------------
void CLoadingTipPanel::SetupTips( void )
{
#ifdef _DEMO
	KeyValues *pKV = new KeyValues( "Tips" );
	KeyValues::AutoDelete autodelete( pKV );
	if ( !pKV->LoadFromFile( g_pFullFileSystem, "scripts/tips.txt", "GAME" ) )
	{
		AssertMsg( false, "failed to load tips!" );
		return;
	}

	for ( KeyValues *pKey = pKV->FindKey( "SurvivorTips" )->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey() )
	{
		sTipInfo info;
		V_strncpy( info.szTipTitle, "", MAX_TIP_LENGTH );
		V_strncpy( info.szTipString, pKey->GetName(), MAX_TIP_LENGTH );
		V_strncpy( info.szTipImage, "achievements/ACH_SURVIVE_BRIDGE", MAX_TIP_LENGTH );
		m_Tips.AddToTail( info );
	}
#else
	TitleAchievementsDescription_t const *desc = g_pMatchFramework->GetMatchTitle()->DescribeTitleAchievements();
	for ( ; desc->m_szAchievementName; ++desc )
	{
		sTipInfo info;
		V_snprintf( info.szTipTitle, MAX_TIP_LENGTH, "#%s_NAME", desc->m_szAchievementName );
		V_snprintf( info.szTipString, MAX_TIP_LENGTH, "#%s_DESC", desc->m_szAchievementName );
		V_snprintf( info.szTipImage, MAX_TIP_LENGTH, "achievements/%s", desc->m_szAchievementName );
		m_Tips.AddToTail( info );
	}
#endif
}

//--------------------------------------------------------------------------------------------------------
void CLoadingTipPanel::NextTip( void )
{
	if ( !IsEnabled() )
		return;

	if ( !m_Tips.Count() )
		return;

	if ( !m_flLastTipTime )
	{
		// Initialize timer on first render
		m_flLastTipTime = Plat_FloatTime();
		return;
	}

	if ( Plat_FloatTime() - m_flLastTipTime < ui_loading_tip_refresh.GetFloat() )
		return;

	m_flLastTipTime = Plat_FloatTime();

	m_iCurrentTip = RandomInt( 0, m_Tips.Count() - 1 );
	if ( !m_Tips.IsValidIndex( m_iCurrentTip ) )
		return;

	sTipInfo info = m_Tips[m_iCurrentTip];

	if ( m_pTipIcon )
	{
		m_pTipIcon->SetImage( info.szTipImage );
	}
	SetControlString( "TipTitle", info.szTipTitle );
	SetControlString( "TipText", info.szTipString );

	// Set our control visible
	SetVisible( true );
}


#define TOP_BORDER_HEIGHT		21
#define BOTTOM_BORDER_HEIGHT	21
int CLoadingTipPanel::DrawSmearBackgroundFade( int x0, int y0, int x1, int y1 )
{
	int wide = x1 - x0;
	int tall = y1 - y0;

	int topTall = scheme()->GetProportionalScaledValue( TOP_BORDER_HEIGHT );
	int bottomTall = scheme()->GetProportionalScaledValue( BOTTOM_BORDER_HEIGHT );

	float f1 = ui_loading_tip_f1.GetFloat();
	float f2 = ui_loading_tip_f2.GetFloat();

	topTall  = 1.00f * topTall;
	bottomTall = 1.00f * bottomTall;

	int middleTall = tall - ( topTall + bottomTall );
	if ( middleTall < 0 )
	{
		middleTall = 0;
	}

	surface()->DrawSetColor( m_smearColor );

	y0 += topTall;

	if ( middleTall )
	{
		// middle
		surface()->DrawFilledRectFade( x0, y0, x0 + f1*wide, y0 + middleTall, 0, 255, true );
		surface()->DrawFilledRectFade( x0 + f1*wide, y0, x0 + f2*wide, y0 + middleTall, 255, 255, true );
		surface()->DrawFilledRectFade( x0 + f2*wide, y0, x0 + wide, y0 + middleTall, 255, 0, true );
		y0 += middleTall;
	}

	return topTall + middleTall + bottomTall;
}

//--------------------------------------------------------------------------------------------------------
void CLoadingTipPanel::PaintBackground( void )
{
	BaseClass::PaintBackground();

	DrawSmearBackgroundFade( 
		0, 
		-scheme()->GetProportionalScaledValue( 20 ), 
		GetWide(), 
		GetTall() ); 

}

void PrecacheLoadingTipIcons()
{
	TitleAchievementsDescription_t const *desc = g_pMatchFramework->GetMatchTitle()->DescribeTitleAchievements();
	for ( ; desc->m_szAchievementName; ++desc )
	{
		CFmtStr imageString( "vgui/achievements/%s", desc->m_szAchievementName );
		int nImageId = vgui::surface()->DrawGetTextureId( imageString );
		if ( nImageId == -1 )
		{
			nImageId = vgui::surface()->CreateNewTextureID();
			vgui::surface()->DrawSetTextureFile( nImageId, imageString, true, false );	
		}
	}
}