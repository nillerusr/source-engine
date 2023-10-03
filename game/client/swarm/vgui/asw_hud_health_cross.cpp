#include "cbase.h"
#include "asw_hud_portraits.h"
#include "asw_hud_marine_portrait.h"
#include "asw_hud_health_cross.h"
#include "c_asw_marine_resource.h"
#include "c_asw_marine.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CASW_HUD_Health_Cross::CASW_HUD_Health_Cross(vgui::Panel *pParent, const char *szPanelName ) : vgui::Panel(pParent, szPanelName)
{
	m_hPortrait = dynamic_cast<CASW_HUD_Marine_Portrait*>(pParent);
	m_iHealth = 0;
	m_pHealthLabelShadow = new vgui::Label( this, "HealthLabelShadow", "0");
	m_pHealthLabel = new vgui::Label( this, "HealthLabel", "0" );
}

void CASW_HUD_Health_Cross::PerformLayout()
{
	BaseClass::PerformLayout();

	m_pHealthLabel->SetBounds( health_xpos, health_ypos, GetWide(), GetTall() );
	m_pHealthLabelShadow->SetBounds( health_xpos + 1, health_ypos + 1, GetWide(), GetTall() );
}

void CASW_HUD_Health_Cross::ApplySchemeSettings( vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	m_pHealthLabel->SetFgColor(Color(255,255,255,255));
	m_pHealthLabel->SetFont(m_hNumberFont);
	m_pHealthLabel->SetContentAlignment( vgui::Label::a_center );

	m_pHealthLabelShadow->SetFgColor(Color(0,0,0,255));
	m_pHealthLabelShadow->SetFont(m_hNumberFont);
	m_pHealthLabelShadow->SetContentAlignment( vgui::Label::a_center );
}

void CASW_HUD_Health_Cross::OnThink()
{
	if ( !m_hPortrait.Get() )
		return;

	if ( !m_hPortrait->m_hMarineResource.Get() || !m_hPortrait->m_hMarineResource->GetMarineEntity() )
		return;

	int iHealth = MAX( 0, m_hPortrait->m_hMarineResource->GetMarineEntity()->GetHealth() );

	if ( iHealth != m_iHealth )
	{
		m_iHealth = iHealth;
		
		m_pHealthLabel->SetText( VarArgs( "%d", iHealth ) );
		m_pHealthLabelShadow->SetText( VarArgs( "%d", iHealth ) );
	}
}

void CASW_HUD_Health_Cross::Paint()
{
	if ( !m_hPortrait.Get() )
		return;

	if ( !m_hPortrait->m_hMarineResource.Get() )
		return;

	C_ASW_Marine *pMarine = m_hPortrait->m_hMarineResource->GetMarineEntity();
	if ( !pMarine )
		return;

	float fHealth = m_hPortrait->m_hMarineResource->GetHealthPercent();
	fHealth = clamp( fHealth, 0.0f, 1.0f );

	int r, g, b, a;
	r = g = b = a = 0;
	r = a = 255;
	float diffr = (128.0f - r) * pMarine->m_fRedNamePulse;
	float diffg = (0.0f - g) * pMarine->m_fRedNamePulse;
	float diffb = (0.0f - b) * pMarine->m_fRedNamePulse;
	r += diffr;
	g += diffg;
	b += diffb;
	/*
	if ( fHealth < 1.0f && fHealth >= 0.5f )
	{
		float fOrangeFade = ( ( fHealth - 0.5f ) / 0.5f );
		g = fOrangeFade * 128.0f + 128.0f;
		b = fOrangeFade * 255.0f;
	}
	else if ( fHealth < 0.5f )
	{
		float fRedFade = fHealth / 0.5f;
		g = fRedFade * 128.0f;
		b = 0;
	}*/

	// remap onto 18 px to 128-18px range of the health texture
	fHealth *= 0.71875f;
	fHealth += 0.140625f;
	fHealth = clamp( fHealth, 0.0f, 1.0f );
	
	if ( m_nFullHealthTexture == -1 || m_nNoHealthTexture == -1 )
		return;
	
	int empty_height = GetTall() * ( 1.0f - fHealth );
	int w = GetWide();
	int t = GetTall();
	vgui::surface()->DrawSetColor(Color(r,g,b,a));
	vgui::surface()->DrawSetTexture(m_nNoHealthTexture);
	vgui::Vertex_t nopoints[4] = 
	{ 
		vgui::Vertex_t( Vector2D(0, 0),					Vector2D(0, 0) ), 
		vgui::Vertex_t( Vector2D(w, 0),					Vector2D(1, 0) ), 
		vgui::Vertex_t( Vector2D(w, empty_height),		Vector2D(1, 1.0f - fHealth) ), 
		vgui::Vertex_t( Vector2D(0, empty_height),		Vector2D(0, 1.0f - fHealth) )
	}; 
	vgui::surface()->DrawTexturedPolygon( 4, nopoints );
	vgui::surface()->DrawSetColor(Color(r,g,b,a));
	vgui::surface()->DrawSetTexture(m_nFullHealthTexture);
	vgui::Vertex_t fullpoints[4] = 
	{ 
		vgui::Vertex_t( Vector2D(0, empty_height),	Vector2D(0, 1.0f - fHealth) ), 
		vgui::Vertex_t( Vector2D(w, empty_height),	Vector2D(1, 1.0f - fHealth) ), 
		vgui::Vertex_t( Vector2D(w, t),				Vector2D(1,1) ), 
		vgui::Vertex_t( Vector2D(0, t),				Vector2D(0,1) )
	}; 
	vgui::surface()->DrawTexturedPolygon( 4, fullpoints );
}
