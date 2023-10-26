#include "cbase.h"
#include "hud.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include <KeyValues.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/AnimationController.h>
#include <vgui/ILocalize.h>
#include "asw_hudelement.h"
#include "c_asw_game_resource.h"
#include "hud_numericdisplay.h"
#include "hudelement.h"
#include <vgui_controls/Label.h>
#include <vgui_controls/EditablePanel.h>

using namespace vgui;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar asw_draw_hud;
extern ConVar asw_money;

class CASWHudMoney : public CASW_HudElement, public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CASWHudMoney, vgui::EditablePanel );

public:
	CASWHudMoney( const char *pElementName );
	virtual void Init( void );
	virtual void VidInit( void );
	virtual void Reset( void );
	virtual void OnThink();
	virtual bool ShouldDraw();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

	vgui::Label *m_pMoneyLabel;
	int m_iMoney;
};	

DECLARE_HUDELEMENT( CASWHudMoney );

CASWHudMoney::CASWHudMoney( const char *pElementName ) : CASW_HudElement( pElementName ), vgui::EditablePanel(NULL, "ASWHudMoney")
{
	SetParent( GetClientMode()->GetViewport() );
	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD );
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
	SetScheme(scheme);	
	m_pMoneyLabel = NULL;
}

void CASWHudMoney::Init()
{
	Reset();
}

void CASWHudMoney::Reset()
{
	m_iMoney = 0;
}

void CASWHudMoney::VidInit()
{
	Reset();
}

void CASWHudMoney::OnThink()
{
	if ( !ASWGameResource() )
		return;

	int iMoney = ASWGameResource()->GetMoney();
	if ( iMoney != m_iMoney )
	{
		if ( !m_pMoneyLabel )
		{
			m_pMoneyLabel = dynamic_cast<vgui::Label*>(FindChildByName( "MoneyLabel" ));
		}
		m_iMoney = iMoney;

		if ( m_pMoneyLabel )
		{
			m_pMoneyLabel->SetText( VarArgs( "$%d", m_iMoney ) );
		}
	}
}

void CASWHudMoney::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "resource/UI/HudMoney.res" );
}

bool CASWHudMoney::ShouldDraw()
{
	return asw_money.GetBool() && CASW_HudElement::ShouldDraw();
}