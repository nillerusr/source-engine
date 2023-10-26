#include "cbase.h"
#include "MedalCollectionPanel.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/Label.h"
#include <vgui_controls/ImagePanel.h>
#include "asw_medal_store.h"
#include "MedalPanel.h"
#include "BriefingTooltip.h"
#include "c_asw_player.h"
#include <vgui/ISurface.h>
#include <vgui_controls/Frame.h>
#include <vgui/ILocalize.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

ConVar asw_medal_collection_sp("asw_medal_collection_sp", "1", FCVAR_ARCHIVE, "Whether the medal collection shows singleplayer or multiplayer");

MedalCollectionPanel::MedalCollectionPanel(vgui::Panel *parent, const char *name) :
vgui::Panel(parent, name)
{
	m_iCursorOver = -1;	
	m_szNameString[0] = '\0';
	m_szTotalString[0] = '\0';
	SetMouseInputEnabled(true);

	m_pTooltip = new BriefingTooltip(this, "MedalsTooltip");
	m_pTooltip->SetTooltip(this, "INIT", "INIT", 0, 0);	// have to do this, as the tooltip "jumps" for one frame when it's first drawn (this makes the jump happen while the briefing is fading in)

	m_pPlayerArea = new vgui::Panel(this, "PlayerArea");
	m_pNCOArea = new vgui::Panel(this, "NCOArea");
	m_pSpecialWeaponsArea = new vgui::Panel(this, "SWArea");
	m_pMedicArea = new vgui::Panel(this, "MedicArea");
	m_pTechArea = new vgui::Panel(this, "TechArea");
	m_pGenArea = new vgui::Panel(this, "TechArea");

	m_pPlayerArea->SetMouseInputEnabled(false);
	m_pNCOArea->SetMouseInputEnabled(false);
	m_pSpecialWeaponsArea->SetMouseInputEnabled(false);
	m_pMedicArea->SetMouseInputEnabled(false);
	m_pTechArea->SetMouseInputEnabled(false);
	m_pGenArea->SetMouseInputEnabled(false);

	m_pTitle = new vgui::Label(this, "Title", "Medal Collection");
	m_pTitle->SetContentAlignment(vgui::Label::a_north);
	m_pCollectionLabel = new vgui::Label(this, "CollectionLabel", "#asw_commander_medals");
	m_pCollectionLabel->SetContentAlignment(vgui::Label::a_northwest);

	m_pNCOLabel = new vgui::Label(this, "CollectionLabel", "#asw_nco_medals");
	m_pNCOLabel->SetContentAlignment(vgui::Label::a_northwest);
	m_pSWLabel = new vgui::Label(this, "CollectionLabel", "#asw_sw_medals");
	m_pSWLabel->SetContentAlignment(vgui::Label::a_northwest);
	m_pMedicLabel = new vgui::Label(this, "CollectionLabel", "#asw_medic_medals");
	m_pMedicLabel->SetContentAlignment(vgui::Label::a_northwest);
	m_pTechLabel = new vgui::Label(this, "CollectionLabel", "#asw_tech_medals");
	m_pTechLabel->SetContentAlignment(vgui::Label::a_northwest);	

	m_pTotalLabel = new vgui::Label(this, "TotalLabel", "");
	m_pTotalLabel->SetContentAlignment(vgui::Label::a_southeast);

	for (int i=0;i<ASW_MEDAL_COLLECTION_PORTRAITS;i++)
	{
		m_pPortrait[i] = new vgui::ImagePanel(this, "Portrait");
		m_pPortrait[i]->SetShouldScaleImage(true);
		m_pPortrait[i]->SetMouseInputEnabled(false);
	}
	m_pPortrait[0]->SetImage("swarm/PortraitsSmall/Sarge");
	m_pPortrait[1]->SetImage("swarm/PortraitsSmall/Jaeger");
	m_pPortrait[2]->SetImage("swarm/PortraitsSmall/Wildcat");
	m_pPortrait[3]->SetImage("swarm/PortraitsSmall/Wolfe");
	m_pPortrait[4]->SetImage("swarm/PortraitsSmall/Faith");
	m_pPortrait[5]->SetImage("swarm/PortraitsSmall/Bastille");
	m_pPortrait[6]->SetImage("swarm/PortraitsSmall/Crash");
	m_pPortrait[7]->SetImage("swarm/PortraitsSmall/Flynn");
	m_pPortrait[8]->SetImage("swarm/PortraitsSmall/Vegas");

	m_pOnlineLabel = new vgui::Label(this, "onlinelabel", "");
	m_pOnlineLabel->SetMouseInputEnabled(false);
	m_pOnlineLabel->SetContentAlignment(vgui::Label::a_west);
	m_pOnlineImage = new vgui::ImagePanel(this, "onlineimage");
	m_pOnlineImage->SetShouldScaleImage(true);
	m_pOnlineImage->SetMouseInputEnabled(false);

	m_bOffline = (gpGlobals->maxClients <= 1);
	if (m_bOffline)
	{
		m_pOnlineLabel->SetText("#asw_offline_medals");
		m_pOnlineImage->SetImage("swarm/medals/offlinemedals");	
	}
	else
	{
		m_pOnlineLabel->SetText("#asw_online_medals");
		m_pOnlineImage->SetImage("swarm/medals/onlinemedals");	
	}

	for (int i=0;i<ASW_MEDAL_COLLECTION_MEDALS;i++)
	{
		m_pMedals[i] = NULL;
	}

	// create medals
	for (int i=1;i<LAST_MEDAL;i++)
	{
		m_pMedals[i] = new MedalPanel(this, "Medal", i, m_pTooltip);
		m_pMedals[i]->SetMedalIndex(i, m_bOffline);
		m_pMedals[i]->SetVisible(true);
		m_pMedals[i]->SetAlpha(255);
	}
	
	m_pTooltip->MoveToFront();

	m_fScale = 1.0f;
}

void MedalCollectionPanel::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	//m_pTitle->SetFont(pScheme->GetFont("DefaultLarge", true));
	m_pTitle->SetFgColor(Color(255,255,255,255));
	m_pCollectionLabel->SetFgColor(Color(255,255,255,255));
	m_pNCOLabel->SetFgColor(Color(255,255,255,255));
	m_pSWLabel->SetFgColor(Color(255,255,255,255));
	m_pMedicLabel->SetFgColor(Color(255,255,255,255));
	m_pTechLabel->SetFgColor(Color(255,255,255,255));
	m_pTotalLabel->SetFgColor(Color(255,255,255,255));
	//m_pTotalLabel->SetFont(pScheme->GetFont("DefaultLarge", true));
	m_pPlayerArea->SetPaintBackgroundEnabled(true);
	m_pPlayerArea->SetBgColor(Color(33, 38, 49, 153));
	m_pNCOArea->SetPaintBackgroundEnabled(true);
	m_pNCOArea->SetBgColor(Color(33, 38, 49, 153));
	m_pSpecialWeaponsArea->SetPaintBackgroundEnabled(true);
	m_pSpecialWeaponsArea->SetBgColor(Color(33, 38, 49, 153));
	m_pMedicArea->SetPaintBackgroundEnabled(true);
	m_pMedicArea->SetBgColor(Color(33, 38, 49, 153));
	m_pTechArea->SetPaintBackgroundEnabled(true);
	m_pTechArea->SetBgColor(Color(33, 38, 49, 153));
	m_pGenArea->SetPaintBackgroundEnabled(true);
	m_pGenArea->SetBgColor(Color(33, 38, 49, 153));
	//m_pOnlineLabel->SetFont(pScheme->GetFont("Default", true));
	m_pOnlineLabel->SetFgColor(Color(255,255,255,255)); //Color(66, 142, 192, 255));
}

void MedalCollectionPanel::PerformLayout()
{
	int sw, sh;
	vgui::surface()->GetScreenSize( sw, sh );
	int wide = GetWide();
	int tall = GetTall();
	m_fScale = tall / 768.0f;
	//m_fScale *= 0.5f;
	

	int border = 10 * m_fScale;
	int top = 0 * m_fScale;
	int portrait_gap = border; // 40 * m_fScale;

	m_pTitle->SetBounds(border * 2, top - border*3, wide - (border * 4), 96 * m_fScale);
	m_pPlayerArea->SetBounds(border, top, wide - border * 2, 96 * m_fScale);
	m_pCollectionLabel->SetBounds(border * 2, top + border, wide * 0.6f, tall * 0.1f);

	int portrait_width = 64 * m_fScale;
	int portrait_height = portrait_width;
	int medal_size = 56 * m_fScale;
	//int portrait_area_total_wide = border * 11 + portrait_width * 9 + portrait_gap * 5;
	int portrait_area_top = top + 96 * m_fScale + border * 2;
	int label_height = border * 2.3f;
	int portrait_area_h = border + portrait_height + border * 2 + medal_size + border + label_height;
	//int two_portrait_area_width = border * 2 + portrait_gap + portrait_width * 2;	
	int two_portrait_area_width = (wide - border * 5) * 0.25f;	
	int nco_left = border; //(wide * 0.5f) - (portrait_area_total_wide * 0.5f);
	m_pNCOLabel->SetBounds(nco_left + border, portrait_area_top + border, two_portrait_area_width - border * 2, label_height);
	m_pNCOArea->SetBounds(nco_left, portrait_area_top, two_portrait_area_width, portrait_area_h);

	int sw_left = nco_left + border + two_portrait_area_width;
	m_pSWLabel->SetBounds(sw_left + border, portrait_area_top + border, two_portrait_area_width - border * 2, label_height);
	m_pSpecialWeaponsArea->SetBounds(sw_left, portrait_area_top, two_portrait_area_width, portrait_area_h);
	
	int medic_left = nco_left + border * 2 + two_portrait_area_width * 2;
	m_pMedicLabel->SetBounds(medic_left + border, portrait_area_top + border, two_portrait_area_width - border * 2, label_height);
	m_pMedicArea->SetBounds(medic_left, portrait_area_top, two_portrait_area_width, portrait_area_h);

	int tech_left = nco_left + border * 3 + two_portrait_area_width * 3;
	//int three_portrait_area_width = two_portrait_area_width + portrait_gap + portrait_width;
	int three_portrait_area_width = (wide - border * 5) * 0.25f;
	m_pTechLabel->SetBounds(tech_left + border, portrait_area_top + border, two_portrait_area_width - border * 2, label_height);
	m_pTechArea->SetBounds(tech_left, portrait_area_top, three_portrait_area_width, portrait_area_h);

	int bottom = tall * 0.85f;
	int gen_top = (portrait_area_top + portrait_area_h + border * 2);
	m_pGenArea->SetBounds(border, gen_top, wide - border * 2,
					(bottom) - gen_top);

	int total_left = 0.3f * wide;
	int total_h = 64.0f * m_fScale;
	int total_top = bottom - (total_h + border);
	
	m_pTotalLabel->SetBounds(total_left, total_top, (wide - border*2) - total_left, total_h );

	int online_h = 32*m_fScale;
	int online_top = total_top + total_h - online_h;
	m_pOnlineImage->SetBounds(border * 2, online_top, online_h, online_h);
	m_pOnlineLabel->SetBounds(border * 2 + 36 * m_fScale, online_top, 180 * m_fScale, online_h);

	for (int i=0;i<ASW_MEDAL_COLLECTION_PORTRAITS;i++)
	{
		m_pPortrait[i]->SetSize(portrait_width, portrait_height);
	}
	int portrait_y = portrait_area_top + border * 2 + label_height;
	m_pPortrait[0]->SetPos(nco_left + border, portrait_y);
	m_pPortrait[1]->SetPos(nco_left + border + portrait_width + portrait_gap, portrait_y);
	m_pPortrait[2]->SetPos(sw_left + border, portrait_y);
	m_pPortrait[3]->SetPos(sw_left + portrait_width + portrait_gap + border, portrait_y);
	m_pPortrait[4]->SetPos(medic_left + border, portrait_y);
	m_pPortrait[5]->SetPos(medic_left + portrait_width + portrait_gap + border, portrait_y);
	m_pPortrait[6]->SetPos(tech_left + border, portrait_y);
	m_pPortrait[7]->SetPos(tech_left + portrait_width + portrait_gap + border, portrait_y);
	m_pPortrait[8]->SetPos(tech_left + portrait_width*2 + portrait_gap*2 + border, portrait_y);
	
	int iSpacing = 20.0f * m_fScale;
	// player medals
	int mx = border * 2;
	int my = (top + 96 * m_fScale) - (border + medal_size);
	LayoutMedal(m_pMedals[MEDAL_IAF_TRAINING], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_IAF_COMBAT_HONORS], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_IAF_BATTLE_HONORS], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_IAF_CAMPAIGN_HONORS], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_IAF_WARTIME_SERVICE], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_PROFESSIONAL], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_NEMESIS], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_RETRIBUTION], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_IAF_HERO], mx, my, iSpacing);
	
	// nco medals
	mx = nco_left + border;
	my = portrait_y + portrait_height + border;
	LayoutMedal(m_pMedals[MEDAL_IRON_HAMMER], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_INCENDIARY_DEFENCE], mx, my, iSpacing);
	//LayoutMedal(m_pMedals[MEDAL_LAST_STAND], mx, my, iSpacing);
	
	// sw medals
	mx = sw_left + border;
	my = portrait_y + portrait_height + border;	
	LayoutMedal(m_pMedals[MEDAL_IRON_SWORD], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_SWARM_SUPPRESSION], mx, my, iSpacing);

	// medic medals
	mx = medic_left + border;
	my = portrait_y + portrait_height + border;
	LayoutMedal(m_pMedals[MEDAL_BLOOD_HALO], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_SILVER_HALO], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_GOLDEN_HALO], mx, my, iSpacing);

	// tech medals
	mx = tech_left + border;
	my = portrait_y + portrait_height + border;
	LayoutMedal(m_pMedals[MEDAL_ELECTRICAL_SYSTEMS_EXPERT], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_COMPUTER_SYSTEMS_EXPERT], mx, my, iSpacing);

	// shared medals
	mx = border * 2;
	my = gen_top + border;

	LayoutMedal(m_pMedals[MEDAL_BLOOD_HEART], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_FIREFIGHTER], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_LIFESAVER], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_ALL_SURVIVE_A_MISSION], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_RECKLESS_EXPLOSIVES_MERIT], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_PEST_CONTROL], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_BUGSTOMPER], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_SHIELDBUG_ASSASSIN], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_EXTERMINATOR], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_CLEAR_FIRING], mx, my, iSpacing);
	//mx = border * 2; my += medal_size;

	// weapon specific medals
	LayoutMedal(m_pMedals[MEDAL_IRON_FIST], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_IRON_DAGGER], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_PYROMANIAC], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_HUNTER], mx, my, iSpacing);
	//LayoutMedal(m_pMedals[MEDAL_HYBRID_WEAPONS_EXPERT], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_SMALL_ARMS_SPECIALIST], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_GUNFIGHTER], mx, my, iSpacing);
	//mx = border * 2; my += medal_size;

	// high quality shared medals
	LayoutMedal(m_pMedals[MEDAL_PERFECT], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_SHARPSHOOTER], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_COLLATERAL_DAMAGE], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_EXPLOSIVES_MERIT], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_KILLING_SPREE], mx, my, iSpacing);
	//mx = border * 2; my += medal_size;

	// campaign completion medals
	LayoutMedal(m_pMedals[MEDAL_EASY_CAMPAIGN], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_NORMAL_CAMPAIGN], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_HARD_CAMPAIGN], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_INSANE_CAMPAIGN], mx, my, iSpacing);
	//mx = border * 2; my += medal_size;

	// speed runs
	LayoutMedal(m_pMedals[MEDAL_SPEED_RUN_LANDING_BAY], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_SPEED_RUN_OUTSIDE], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_SPEED_RUN_PLANT], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_SPEED_RUN_OFFICE], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_SPEED_RUN_DESCENT], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_SPEED_RUN_SEWERS], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_SPEED_RUN_MINE], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_SPEED_RUN_QUEEN_LAIR], mx, my, iSpacing);
	//mx = border * 2; my += medal_size;

	// outstanding execution
	LayoutMedal(m_pMedals[MEDAL_OUTSTANDING_EXECUTION_LANDING_BAY], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_OUTSTANDING_EXECUTION_OUTSIDE], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_OUTSTANDING_EXECUTION_PLANT], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_OUTSTANDING_EXECUTION_OFFICE], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_OUTSTANDING_EXECUTION_LABS], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_OUTSTANDING_EXECUTION_SEWERS], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_OUTSTANDING_EXECUTION_MINE], mx, my, iSpacing);
	LayoutMedal(m_pMedals[MEDAL_OUTSTANDING_EXECUTION_QUEEN_LAIR], mx, my, iSpacing);
}

void MedalCollectionPanel::OnThink()
{	
	UpdateMedals();
}

// sets the image and color of each medal depending on if we have it or not
void MedalCollectionPanel::UpdateMedals()
{
	if (!GetMedalStore())
	{
		Msg("Error: Couldn't find medal store\n");
		return;
	}
	// update medal colors
	int iCollected = 0;
	int iMarineCollected=0;
	for (int i=1;i<ASW_MEDAL_COLLECTION_MEDALS;i++)
	{
		if (!m_pMedals[i] || !m_pMedals[i]->m_pMedalIcon)
			continue;
		bool bHasMedal = GetMedalStore()->HasMedal(m_pMedals[i]->m_iMedalIndex, m_bOffline, m_iCursorOver);
		if (bHasMedal)
		{
			m_pMedals[i]->m_pMedalIcon->SetDrawColor(Color(255,255,255,255));
			m_pMedals[i]->m_bShowTooltip = true;
			iCollected++;
			if (!GetMedalStore()->IsPlayerMedal(m_pMedals[i]->m_iMedalIndex))
				iMarineCollected++;
		}
		else
		{
			if (m_iCursorOver != -1)	// if we're showing a specific marine, check if another marine has the medal, to just grey it out a bit rather than make it completely black
				bHasMedal = GetMedalStore()->HasMedal(m_pMedals[i]->m_iMedalIndex, m_bOffline, -1);
			if (bHasMedal)
			{
				m_pMedals[i]->m_pMedalIcon->SetDrawColor(Color(64,64,64,255));
				m_pMedals[i]->m_bShowTooltip = true;
			}
			else
			{
				// temp change to debug medals
				m_pMedals[i]->m_pMedalIcon->SetDrawColor(Color(32,32,32,255));
				m_pMedals[i]->m_bShowTooltip = true;

//				m_pMedals[i]->m_pMedalIcon->SetDrawColor(Color(0,0,0,255));
				//m_pMedals[i]->m_bShowTooltip = false;
			}
		}
	}
	// update our corner label
	char buffer[128];
	if (m_iCursorOver == -1)
	{
		Q_snprintf(buffer, sizeof(buffer), "%d / %d", iCollected, ASW_MEDAL_COLLECTION_MEDALS-1);
	}
	else
	{
		wchar_t *pName;
		if (m_iCursorOver == 0) pName = g_pVGuiLocalize->Find("#asw_sarges_total");
		else if (m_iCursorOver == 1) pName = g_pVGuiLocalize->Find("#asw_jaegers_total");
		else if (m_iCursorOver == 2) pName = g_pVGuiLocalize->Find("#asw_wildcats_total");
		else if (m_iCursorOver == 3) pName = g_pVGuiLocalize->Find("#asw_wolfes_total");
		else if (m_iCursorOver == 4) pName = g_pVGuiLocalize->Find("#asw_faiths_total");
		else if (m_iCursorOver == 5) pName = g_pVGuiLocalize->Find("#asw_bastilles_total");
		else if (m_iCursorOver == 6) pName = g_pVGuiLocalize->Find("#asw_crashs_total");
		else if (m_iCursorOver == 7) pName = g_pVGuiLocalize->Find("#asw_flynns_total");
		else pName = g_pVGuiLocalize->Find("#asw_vegas_total");

		// 43 or 44
		int iMarineMax = ASW_MEDAL_COLLECTION_MEDALS - (9 + 8);	// minus the 9 player medals and the other classes medals
		if (m_iCursorOver >= 4 && m_iCursorOver <= 5)
			iMarineMax++;	// +1 for medics, since they get 3 medals

		char number_buffer[12];
		Q_snprintf(number_buffer, sizeof(number_buffer), "%d / %d", iMarineCollected, iMarineMax);
		wchar_t wnumber_buffer[24];
		g_pVGuiLocalize->ConvertANSIToUnicode(number_buffer, wnumber_buffer, sizeof( wnumber_buffer ));

		wchar_t wbuffer[64];				
		g_pVGuiLocalize->ConstructString( wbuffer, sizeof(wbuffer),
			g_pVGuiLocalize->Find("#asw_medal_collect_format"), 2,
			pName, wnumber_buffer);

		g_pVGuiLocalize->ConvertUnicodeToANSI( wbuffer, buffer, sizeof(buffer) );		
	}
	if (Q_strcmp(buffer, m_szTotalString))
	{
		Q_strcpy(m_szTotalString, buffer);
		m_pTotalLabel->SetText(buffer);
	}
	
	HACK_GETLOCALPLAYER_GUARD( "Would need to redo the medal system to associate medals with a particular player's profile, rather than just storing them locally" );
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (pPlayer && Q_strcmp(pPlayer->GetPlayerName(), m_szNameString))
	{
		Q_strcpy(m_szNameString, pPlayer->GetPlayerName());
		wchar_t wname_buffer[32];
		g_pVGuiLocalize->ConvertANSIToUnicode(m_szNameString, wname_buffer, sizeof( wname_buffer ));
		wchar_t wbuffer[64];
		if (m_szNameString[Q_strlen(m_szNameString)-1] == 's' || m_szNameString[Q_strlen(m_szNameString)-1] == 'S')
		{
			g_pVGuiLocalize->ConstructString( wbuffer, sizeof(wbuffer),
				g_pVGuiLocalize->Find("#asw_medal_player_collection_no_s"), 1, wname_buffer);
		}
		else
		{
			g_pVGuiLocalize->ConstructString( wbuffer, sizeof(wbuffer),
				g_pVGuiLocalize->Find("#asw_medal_player_collection"), 1, wname_buffer);
		}
		m_pTitle->SetText(wbuffer);
		vgui::Frame *pFrame = dynamic_cast<vgui::Frame*>(GetParent());
		if (pFrame)
		{
			pFrame->SetTitle(wbuffer, true);
		}
	}
}

// positions a medal and moves the medal cursor along, wrapping it down a line if it goes off the right hand side of the panel
void MedalCollectionPanel::LayoutMedal(MedalPanel* pMedal, int &mx, int &my, int iSpacing)
{
	if (!pMedal)
		return;

	int medal_size = 56 * m_fScale;
	int border = 10 * m_fScale;
	pMedal->SetBounds(mx, my, medal_size, medal_size);
	mx += medal_size + iSpacing;
	if (mx > GetWide() - (border*2 + medal_size + iSpacing))
	{
		mx = border * 2;
		my += medal_size + iSpacing;
	}
}

void MedalCollectionPanel::OnMouseReleased(vgui::MouseCode code)
{
	BaseClass::OnMouseReleased(code);
	if ( code != MOUSE_LEFT )
		return;

	int iCursorOver = -1;
	for (int i=0;i<ASW_MEDAL_COLLECTION_PORTRAITS;i++)
	{
		if (m_pPortrait[i]->IsCursorOver())
		{
			iCursorOver = i;
			m_pPortrait[i]->SetDrawColor(Color(255,255,255,255));
		}
		else
		{
			m_pPortrait[i]->SetDrawColor(Color(64,64,64,255));
		}
	}

	if (iCursorOver != m_iCursorOver)
	{
		m_iCursorOver = iCursorOver;		
	}
	else
	{
		m_iCursorOver = -1;
	}

	if (m_iCursorOver == -1)
	{
		for (int i=0;i<ASW_MEDAL_COLLECTION_PORTRAITS;i++)
		{
			m_pPortrait[i]->SetDrawColor(Color(255,255,255,255));
		}
	}

	if (m_pOnlineLabel->IsCursorOver() || m_pOnlineImage->IsCursorOver())
	{
		m_bOffline = !m_bOffline;
		asw_medal_collection_sp.SetValue(m_bOffline ? 1 : 0);
		if (m_bOffline)
		{
			m_pOnlineLabel->SetText("#asw_offline_medals");
			m_pOnlineImage->SetImage("swarm/medals/offlinemedals");	
		}
		else
		{
			m_pOnlineLabel->SetText("#asw_online_medals");
			m_pOnlineImage->SetImage("swarm/medals/onlinemedals");	
		}

		// update offline status of all our medal panels
		for (int i=1;i<LAST_MEDAL;i++)
		{
			m_pMedals[i]->SetMedalIndex(i, m_bOffline);
		}	
	}
}