//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "dme_controls/DmeSourceSkinPanel.h"
#include "dme_controls/DmePanel.h"
#include "movieobjects/dmemdlmakefile.h"
#include "vgui_controls/TextEntry.h"
#include "vgui_controls/CheckButton.h"
#include "tier1/KeyValues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace vgui;


//-----------------------------------------------------------------------------
//
// Hook into the dme panel editor system
//
//-----------------------------------------------------------------------------
IMPLEMENT_DMEPANEL_FACTORY( CDmeSourceSkinPanel, DmeSourceSkin, "DmeSourceSkinDefault", "MDL Skin Editor", true );



//-----------------------------------------------------------------------------
// Purpose: Constructor, destructor
//-----------------------------------------------------------------------------
CDmeSourceSkinPanel::CDmeSourceSkinPanel( vgui::Panel *pParent, const char *pPanelName ) : 
	BaseClass( pParent, pPanelName )
{	
	m_pSkinName = new vgui::TextEntry( this, "SkinName" );
	m_pSkinName->AddActionSignalTarget( this );

	m_pScale = new vgui::TextEntry( this, "Scale" );
	m_pScale->AddActionSignalTarget( this );

	m_pFlipTriangles = new vgui::CheckButton( this, "FlipTriangles", "" );
	m_pFlipTriangles->AddActionSignalTarget( this );

	// Load layout settings; has to happen before pinning occurs in code
	LoadControlSettings( "resource/DmeSourceSkinPanel.res" );
}

CDmeSourceSkinPanel::~CDmeSourceSkinPanel()
{
}


//-----------------------------------------------------------------------------
// Marks the file as dirty (or not)
//-----------------------------------------------------------------------------
void CDmeSourceSkinPanel::SetDirty()
{
	PostActionSignal( new KeyValues( "DmeElementChanged" ) );
}


//-----------------------------------------------------------------------------
// Resets the state
//-----------------------------------------------------------------------------
void CDmeSourceSkinPanel::SetDmeElement( CDmeSourceSkin *pSourceSkin )
{
	m_hSourceSkin = pSourceSkin;

	bool bEnabled = (pSourceSkin != NULL);
	m_pSkinName->SetEnabled( bEnabled );
	m_pScale->SetEnabled( bEnabled );
	m_pFlipTriangles->SetEnabled( bEnabled );
	if ( !bEnabled )
	{
		m_pSkinName->SetText( "" );
		m_pScale->SetText( "" );
		m_pFlipTriangles->SetSelected( false );
		return;
	}

	char pBuf[32];
	Q_snprintf( pBuf, sizeof(pBuf), "%.3f", pSourceSkin->m_flScale.Get() );
	m_pSkinName->SetText( pSourceSkin->m_SkinName );
	m_pScale->SetText( pBuf );
	m_pFlipTriangles->SetSelected( pSourceSkin->m_bFlipTriangles );
}


//-----------------------------------------------------------------------------
// Command handler
//-----------------------------------------------------------------------------
void CDmeSourceSkinPanel::OnCheckButtonChecked( int nChecked )
{
	if ( !m_hSourceSkin.Get() )
		return;

	bool bFlipTriangles = ( nChecked != 0 );
	if ( bFlipTriangles != m_hSourceSkin->m_bFlipTriangles )
	{
		m_hSourceSkin->m_bFlipTriangles = bFlipTriangles;
		SetDirty();
	}
}


//-----------------------------------------------------------------------------
// Called when something is typed in a text entry field
//-----------------------------------------------------------------------------
void CDmeSourceSkinPanel::OnTextChanged( KeyValues *kv )
{
	if ( !m_hSourceSkin.Get() )
		return;

	Panel *pPanel = (Panel *)kv->GetPtr( "panel", NULL );
	if ( pPanel == m_pSkinName )
	{
		char pTextBuf[256];
		m_pSkinName->GetText( pTextBuf, sizeof( pTextBuf) );
		if ( Q_stricmp( pTextBuf, m_hSourceSkin->m_SkinName ) )
		{
			m_hSourceSkin->m_SkinName = pTextBuf;
			SetDirty();
		}
		return;
	}

	if ( pPanel == m_pScale )
	{
		char pTextBuf[256];
		m_pScale->GetText( pTextBuf, sizeof( pTextBuf) );
		float flScale = atoi( pTextBuf );
		if ( flScale != m_hSourceSkin->m_flScale )
		{
			m_hSourceSkin->m_flScale = flScale;
			SetDirty();
		}
		return;
	}
}



