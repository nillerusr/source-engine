//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: Transition presentation.
//
//=====================================================================================//

#include "VTransitionScreen.h"
#include "EngineInterface.h"
#include "vgui/ISurface.h"
#include "vgui/ILocalize.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

//-----------------------------------------------------------------------------
// This is an opaque screen transition used to cover UI before the Xbox restarts
// The system polls for completion of the fade up before continuing the horrific
// restart process.
//-----------------------------------------------------------------------------

CTransitionScreen::CTransitionScreen( Panel *parent, const char *panelName ):
BaseClass( parent, panelName, true, true )
{
	SetPaintBackgroundEnabled( true );

	m_iImageID = -1;
	m_flTransitionStartTime = 0;
	m_hFont = NULL;
	m_bComplete = false;
}

CTransitionScreen::~CTransitionScreen()
{
	surface()->DestroyTextureID( m_iImageID );
}

void CTransitionScreen::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	int screenWide, screenTall;
	surface()->GetScreenSize( screenWide, screenTall );

	char filename[MAX_PATH];
	V_snprintf( filename, sizeof( filename ), "console/background01" ); // TODO: engine->GetStartupImage( filename, sizeof( filename ), screenWide, screenTall );
	m_iImageID = surface()->CreateNewTextureID();
	surface()->DrawSetTextureFile( m_iImageID, filename, true, false );

	m_hFont = pScheme->GetFont( "ScreenTitle", true );

	m_flTransitionStartTime = Plat_FloatTime();
	m_bComplete = false;

	SetPos( 0, 0 );
	SetSize( screenWide, screenTall );
}

void CTransitionScreen::PaintBackground()
{
	// an exiting process needs to opaquely cover everything
	// goes from [0..255]
	int alpha = RemapValClamped( Plat_FloatTime(), m_flTransitionStartTime, m_flTransitionStartTime + 0.5f, 0, 255 );
	if ( alpha >= 255 )
	{
		m_bComplete = true;
	}
	
	// fade the background music out
	CBaseModPanel::GetSingleton().UpdateBackgroundMusicVolume( ( 255.0f - alpha )/255.0f );

	int wide, tall;
	GetSize( wide, tall );
	surface()->DrawSetColor( 255, 255, 255, alpha );
	surface()->DrawSetTexture( m_iImageID );
	surface()->DrawTexturedRect( 0, 0, wide, tall );

	int xPos = 0.90f * wide;
	int yPos = 0.84f * tall;

	const wchar_t *pString = g_pVGuiLocalize->FindSafe( "#L4D360UI_Installer_Loading" );
	int textWide, textTall;
	surface()->GetTextSize( m_hFont, pString, textWide, textTall );
	surface()->DrawSetTextPos( xPos - textWide, yPos - textTall/2 );
	surface()->DrawSetTextFont( m_hFont );
	surface()->DrawSetTextColor( 255, 255, 255, alpha );
	surface()->DrawPrintText( pString, wcslen( pString ) );
}

bool CTransitionScreen::IsTransitionComplete()
{
	return m_bComplete;
}