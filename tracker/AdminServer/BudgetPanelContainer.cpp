//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "BudgetPanelContainer.h"
#include "mathlib/mathlib.h"
#include "vgui/vgui_budgetpanelshared.h"
#include "AdminServer.h"
#include "ivprofexport.h"
#include "vgui/ilocalize.h"
#include "vgui/ISurface.h"
#include "vgui/vgui_BaseBudgetPanel.h"


// -------------------------------------------------------------------------------------------------------------------- //
// CBudgetPanelAdmin declaration.
// -------------------------------------------------------------------------------------------------------------------- //

class CBudgetPanelAdmin : public CBudgetPanelShared
{
	typedef CBudgetPanelShared BaseClass;

public:

	CBudgetPanelAdmin( vgui::Panel *pParent, const char *pElementName );

	virtual void SetupCustomConfigData( CBudgetPanelConfigData &data );
	virtual void PostChildPaint();
	virtual void OnTick();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	void DrawColoredText( 
		vgui::HFont font, 
		int x, int y, 
		int r, int g, int b, int a,
		const char *pText,
		... );

private:
	Color m_budgetTextColor;
};



CBudgetPanelAdmin::CBudgetPanelAdmin( vgui::Panel *pParent, const char *pElementName ) : 
	BaseClass( pParent, pElementName, BUDGETFLAG_SERVER )
{
	MarkAsDedicatedServer();
}


void CBudgetPanelAdmin::SetupCustomConfigData( CBudgetPanelConfigData &data )
{
	GetBounds( data.m_xCoord, data.m_yCoord, data.m_Width, data.m_Height );
}


void CBudgetPanelAdmin::DrawColoredText( 
	vgui::HFont font, 
	int x, int y, 
	int r, int g, int b, int a,
	const char *pText,
	... )
{
	char msg[4096];
	va_list marker;
	va_start( marker, pText );
	_vsnprintf( msg, sizeof( msg ), pText, marker );
	va_end( marker );

	wchar_t unicodeStr[4096];
	int nChars = g_pVGuiLocalize->ConvertANSIToUnicode( msg, unicodeStr, sizeof( unicodeStr ) );

	vgui::surface()->DrawSetTextFont( font );
	vgui::surface()->DrawSetTextColor( r, g, b, a );
	vgui::surface()->DrawSetTextPos( x, y );
	vgui::surface()->DrawPrintText( unicodeStr, nChars-1 );
}


void CBudgetPanelAdmin::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	m_budgetTextColor = pScheme->GetColor( "BrightControlText", Color( 0, 255, 0, 255 ) );
}


void CBudgetPanelAdmin::PostChildPaint()
{
	DrawColoredText( m_hFont, 0, 0, m_budgetTextColor[0], m_budgetTextColor[1], m_budgetTextColor[2], m_budgetTextColor[3], "%i fps (showbudget 3D driver time included)", RoundFloatToInt(g_fFrameRate) );
	DrawColoredText( m_hFont, 0, 16, m_budgetTextColor[0], m_budgetTextColor[1], m_budgetTextColor[2], m_budgetTextColor[3], "%.1f ms", g_fFrameTimeLessBudget*1000.0f );

	BaseClass::PostChildPaint();
}


void CBudgetPanelAdmin::OnTick()
{
	// Don't do all the work if we're not being drawn.
	if ( IsVisible() )
	{
		SnapshotVProfHistory( 0 );
		MarkForFullRepaint();
	}
	
	BaseClass::OnTick();
}



// ------------------------------------------------------------------------------------------------------------------------------------------------ //
// The budget panel container class. Just holds CBudgetPanelAdmin.
// ------------------------------------------------------------------------------------------------------------------------------------------------ //

CBudgetPanelContainer::CBudgetPanelContainer(vgui::Panel *parent, const char *name) : PropertyPage(parent, name)
{
	LoadControlSettings("Admin/BudgetPanel.res", "PLATFORM");
	m_pBudgetPanelAdmin = new CBudgetPanelAdmin( this, "AdminBudgetPanel" );
	m_pBudgetPanelAdmin->SetVisible( false );
	InvalidateLayout();
}


CBudgetPanelContainer::~CBudgetPanelContainer()
{
}


void CBudgetPanelContainer::OnServerDataResponse( const char *value, const char *response )
{
}


void CBudgetPanelContainer::Paint()
{
	
}


void CBudgetPanelContainer::PerformLayout()
{
	BaseClass::PerformLayout();
	int x, y, wide, tall;
	GetBounds( x, y, wide, tall );
	m_pBudgetPanelAdmin->SetBounds( 12, 12, wide - 24, tall - 24 );
	m_pBudgetPanelAdmin->SendConfigDataToBase();
}


//-----------------------------------------------------------------------------
// Purpose: Activates the page
//-----------------------------------------------------------------------------
void CBudgetPanelContainer::OnPageShow()
{
	if ( g_pVProfExport )
		g_pVProfExport->AddListener();

	m_pBudgetPanelAdmin->SetVisible( true );
	BaseClass::OnPageShow();
}


//-----------------------------------------------------------------------------
// Purpose: Hides the page
//-----------------------------------------------------------------------------
void CBudgetPanelContainer::OnPageHide()
{
	if ( g_pVProfExport )
		g_pVProfExport->RemoveListener();

	m_pBudgetPanelAdmin->SetVisible( false );
	BaseClass::OnPageHide();
}
