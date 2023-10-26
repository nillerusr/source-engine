#include "cbase.h"
#include "asw_vgui_ammo_list.h"
#include "vgui/ISurface.h"
#include "vgui_controls/TextImage.h"
#include "vgui_controls/ImagePanel.h"
#include <vgui/IInput.h>
#include "c_asw_player.h"
#include "c_asw_marine.h"
#include "asw_weapon_ammo_bag_shared.h"
#include "asw_hud_objective.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ASW_AMMO_LIST_TITLE "#asw_ammo_bag_contents"

extern ConVar asw_hud_alpha;
extern ConVar asw_hud_scale;

CASW_VGUI_Ammo_List::CASW_VGUI_Ammo_List( vgui::Panel *pParent, const char *pElementName ) 
:	vgui::Panel( pParent, pElementName ),
	CASW_VGUI_Ingame_Panel()
{	
	m_pTitleGlow = new vgui::Label(this, "AmmoBagTitleGlow", ASW_AMMO_LIST_TITLE);
	m_pTitleGlow->SetContentAlignment(vgui::Label::a_northwest);
	m_pTitle = new vgui::Label(this, "AmmoBagTitle", ASW_AMMO_LIST_TITLE);
	m_pTitle->SetContentAlignment(vgui::Label::a_northwest);
	for (int i=0;i<ASW_AMMO_BAG_SLOTS;i++)
	{		
		m_pAmmoCount[i] = new vgui::Label(this, "AmmoCountLabel", "0");
		m_pAmmoCount[i]->SetContentAlignment(vgui::Label::a_northeast);
		m_pAmmoIcon[i] = new vgui::ImagePanel( this, "AmmoIcon" );
		m_pAmmoIcon[i]->SetShouldScaleImage(true);
		m_pAmmoIcon[i]->SetImage( "hud/GameUI/hud_ammo_clip_empty" );
		m_iAmmoCount[i] = 0;
	}
	// set names and images
	m_pAmmoName[0] = new vgui::Label(this, "AmmoNameLabel", "#asw_weapon_rifle");
	//m_pAmmoIcon[0]->SetImage( "swarm/AmmoIcons/AmmoIconRifle" );
	m_pAmmoName[1] = new vgui::Label(this, "AmmoNameLabel", "#asw_weapon_autogun");
	//m_pAmmoIcon[1]->SetImage( "swarm/AmmoIcons/AmmoIconShotgun" );
	m_pAmmoName[2] = new vgui::Label(this, "AmmoNameLabel", "#asw_weapon_shotgun");
	//m_pAmmoIcon[2]->SetImage( "swarm/AmmoIcons/AmmoIconAssault" );
	m_pAmmoName[3] = new vgui::Label(this, "AmmoNameLabel", "#asw_weapon_vindicator");
	//m_pAmmoIcon[3]->SetImage( "swarm/AmmoIcons/AmmoIconAutogun" );
	m_pAmmoName[4] = new vgui::Label(this, "AmmoNameLabel", "#asw_weapon_flamer");
	//m_pAmmoIcon[4]->SetImage( "swarm/AmmoIcons/AmmoIconFlamer" );
	//m_pAmmoName[5] = new vgui::Label(this, "AmmoNameLabel", "#asw_weapon_rail");
	//m_pAmmoIcon[5]->SetImage( "swarm/AmmoIcons/AmmoIconPistol" );
	m_pAmmoName[5] = new vgui::Label(this, "AmmoNameLabel", "#asw_weapon_pdw");
	//m_pAmmoIcon[6]->SetImage( "swarm/AmmoIcons/AmmoIconPistol" );
	m_pAmmoName[6] = new vgui::Label(this, "AmmoNameLabel", "#asw_weapon_pistol");
	//m_pAmmoIcon[7]->SetImage( "swarm/AmmoIcons/AmmoIconPistol" );
}

CASW_VGUI_Ammo_List::~CASW_VGUI_Ammo_List()
{

}

#define TEXT_BORDER_X 0.01f
#define TEXT_BORDER_Y 0.012f
#define ASW_AMMO_ICON_SIZE 16

void CASW_VGUI_Ammo_List::PerformLayout()
{
	BaseClass::PerformLayout();

	int width = ScreenWidth() * asw_hud_scale.GetFloat();
	int height = ScreenHeight() * asw_hud_scale.GetFloat();

	m_fScale = (height / 768.0f);

	// position elements
	float y_cursor = TEXT_BORDER_Y * height * 0.5f;
	float left_edge = 98.0f * m_fScale; //14.0f * m_fScale
	int border = m_fScale * 4.0f;
		
	//float name_x = left_edge;	
	int count_x = 0;
	int count_width = m_fScale * 20;
	int icon_x = count_x + count_width;	
	int name_width = width * 0.15f;
	int name_x = icon_x + ASW_AMMO_ICON_SIZE * m_fScale + border;	// ammo icon 32 pixels wide at 1024
	float right_edge = name_x + name_width + border;
	float name_tall = vgui::surface()->GetFontTall(m_pAmmoName[0]->GetFont());	

	// set the widths of the names + counts, and record the widest

	for (int i=0;i<ASW_AMMO_BAG_SLOTS;i++)
	{
		// just do fixed widths for now		
		m_pAmmoIcon[i]->SetSize(name_tall, name_tall);
		m_pAmmoName[i]->SetSize(name_width, name_tall);
		m_pAmmoCount[i]->SetSize(count_width, vgui::surface()->GetFontTall(m_pAmmoCount[i]->GetFont()));
		if (m_pAmmoName[i]->GetWide() + name_x + border > right_edge)		// keep track of the widest ammo name
		{
			right_edge = m_pAmmoName[i]->GetWide() + name_x + border;
		}
	}

	m_pTitle->SetPos(0, border);
	m_pTitleGlow->SetPos(0, border);
	y_cursor += vgui::surface()->GetFontTall(m_pTitle->GetFont()) + TEXT_BORDER_Y * height * 0.5f;
	for (int i=0;i<ASW_AMMO_BAG_SLOTS;i++)
	{
		m_pAmmoIcon[i]->SetPos(icon_x, y_cursor);
		m_pAmmoName[i]->SetPos(name_x, y_cursor);
		m_pAmmoCount[i]->SetPos(count_x, y_cursor);
		y_cursor += vgui::surface()->GetFontTall(m_pAmmoName[i]->GetFont());	// drop down by text height or icon height, whichever takes up more space
	}
	count_x += border;	// gap between columns
	
	// place this window in the lower left above the ammo counter
	SetTall(y_cursor + TEXT_BORDER_Y * height);
	int tx;
	vgui::HFont titleFont = m_pTitle->GetFont();
	tx = UTIL_ComputeStringWidth( titleFont, ASW_AMMO_LIST_TITLE );
	right_edge = MAX(right_edge, tx);
	SetWide(right_edge);
	m_pTitle->SetWide(GetWide() - border * 2);
	m_pTitleGlow->SetWide(GetWide() - border * 2);
	SetPos(left_edge, height - GetTall() - left_edge);
}

void CASW_VGUI_Ammo_List::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetPaintBackgroundType(0);
	SetPaintBackgroundEnabled(true);
	SetBgColor( Color(0,0,0,asw_hud_alpha.GetInt()) );
	SetAlpha(0);
	SetMouseInputEnabled(false);

	m_pTitle->SetFgColor(Color(255,255,255,255));
	m_pTitleGlow->SetFgColor(Color(35,214,250,255));
	m_pTitle->SetFont(m_hFont);
	m_pTitleGlow->SetFont(m_hGlowFont);
	for (int i=0;i<ASW_AMMO_BAG_SLOTS;i++)
	{
		m_pAmmoName[i]->SetFgColor(Color(255,255,255,255));
		m_pAmmoCount[i]->SetFgColor(Color(255,255,255,255));		
		m_pAmmoName[i]->SetFont(m_hFont);
		m_pAmmoCount[i]->SetFont(m_hFont);
	}
}

void CASW_VGUI_Ammo_List::OnThink()
{
	// update ammo counts
	// fade out and close if switch away from ammo bag
	// fade out and fade in somewhere else if mouse overs the list
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (!pPlayer)
		return;

	C_ASW_Marine *pMarine = pPlayer->GetMarine();
	if (!pMarine)
		return;

	C_ASW_Weapon_Ammo_Bag *pBag = dynamic_cast<C_ASW_Weapon_Ammo_Bag*>(pMarine->GetActiveASWWeapon());
	if (!pBag)
		return;

	for (int i=0;i<ASW_AMMO_BAG_SLOTS;i++)
	{
		if (m_iAmmoCount[i] != pBag->AmmoCount(i))
		{
			m_iAmmoCount[i] = pBag->AmmoCount(i);
			char buffer[24];
			if (i == 5)	// PDW hack to show clips doubled
				Q_snprintf(buffer, sizeof(buffer), "%d", m_iAmmoCount[i] * 2);
			else
				Q_snprintf(buffer, sizeof(buffer), "%d", m_iAmmoCount[i]);
			m_pAmmoCount[i]->SetText(buffer);
		}
	}	
}

void CASW_VGUI_Ammo_List::PaintBackground()
{
	//BaseClass::PaintBackground();

	if (m_nBlackBarTexture == -1)
		return;

	int left = 0;
	int top = 0;
	int bottom = GetTall();
	int bottom_right_x = GetWide();
	int label_x = GetWide() * 0.75f;
	int cx, cy;
	m_pTitle->GetContentSize(cx, cy);
	int label_y = cy;
	vgui::surface()->DrawSetColor(Color(255,255,255,asw_hud_alpha.GetInt()));
	vgui::surface()->DrawSetTexture(m_nBlackBarTexture);
	vgui::Vertex_t points[4] = 
	{ 
	vgui::Vertex_t( Vector2D(left,								top),			Vector2D(0,0) ), 
	vgui::Vertex_t( Vector2D(label_x - label_y,					top),			Vector2D(1,0) ), 
	vgui::Vertex_t( Vector2D(label_x,							label_y),		Vector2D(1,1) ),
	vgui::Vertex_t( Vector2D(left,								label_y),		Vector2D(0,1) )
	};
	vgui::surface()->DrawTexturedPolygon( 4, points );	
	vgui::Vertex_t lpoints[4] = 
	{ 
	vgui::Vertex_t( Vector2D(left,									label_y),	Vector2D(0,0) ), 
	vgui::Vertex_t( Vector2D(bottom_right_x,						label_y),	Vector2D(0.6f, 1) ),		//  
	vgui::Vertex_t( Vector2D(bottom_right_x,						bottom),	Vector2D(0.3f,1) ),
	vgui::Vertex_t( Vector2D(left,									bottom),	Vector2D(0,1) )
	}; 
	vgui::surface()->DrawTexturedPolygon( 4, lpoints );
}

bool CASW_VGUI_Ammo_List::MouseClick(int x, int y, bool bRightClick, bool bDown)
{
	return false;	// not interactive
}