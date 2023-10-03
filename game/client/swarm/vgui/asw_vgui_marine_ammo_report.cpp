#include "cbase.h"
#include "asw_vgui_marine_ammo_report.h"
#include "vgui/ISurface.h"
#include "vgui_controls/TextImage.h"
#include "vgui_controls/ImagePanel.h"
#include <vgui/IInput.h>
#include "c_asw_marine.h"
#include "asw_marine_profile.h"
#include "c_asw_player.h"
#include "asw_weapon_ammo_bag_shared.h"
#include "asw_equipment_list.h"
#include "asw_weapon_parse.h"
#include "c_asw_game_resource.h"
#include <vgui_controls/AnimationController.h>
#include "idebugoverlaypanel.h"
#include "engine/IVDebugOverlay.h"
#include "iinput.h"
#include "asw_hud_crosshair.h"
#include "asw_hud_objective.h"
#include "ammodef.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ASW_AMMO_REPORT_FADE_TIME 0.1

CASW_VGUI_Marine_Ammo_Report::CASW_VGUI_Marine_Ammo_Report( vgui::Panel *pParent, const char *pElementName ) 
:	vgui::Panel( pParent, pElementName ),
	CASW_VGUI_Ingame_Panel()
{	
	m_pTitle = new vgui::Label(this, "AmmoBagTitle", "#asw_ammo_title");
	for (int i=0;i<ASW_AMMO_REPORT_SLOTS;i++)
	{	
		m_pAmmoName[i] = new vgui::Label(this, "AmmoNameLabel", "");
		m_pAmmoCount[i] = new vgui::Label(this, "AmmoCountLabel", "999");
		m_pAmmoOverflow[i] = new vgui::Label(this, "AmmoCountLabel", "999");
		m_pAmmoOverflow[i]->SetContentAlignment(vgui::Label::a_northwest);
		m_pAmmoIcon[i] = new vgui::ImagePanel( this, "AmmoIcon" );
		m_pClipsIcon[i] = new vgui::ImagePanel( this, "ClipsIcon" );
		m_pClipsIcon[i]->SetShouldScaleImage(true);
		m_pAmmoIcon[i]->SetShouldScaleImage(true);
		m_pClipsIcon[i]->SetVisible(false);
		m_pAmmoName[i]->SetVisible(false);
		m_pAmmoCount[i]->SetVisible(false);
		m_pAmmoCount[i]->SetContentAlignment(vgui::Label::a_northwest);
		m_pAmmoOverflow[i]->SetVisible(false);
		m_pAmmoIcon[i]->SetVisible(false);
		m_pClipsIcon[i]->SetImage( "swarm/HUD/AmmoClipsIcon" );
		m_pAmmoIcon[i]->SetImage( "swarm/HUD/BulletsIcon" );
	}
	m_pGlowLabel = new vgui::Label(this, "GlowLabel", "");
	m_pGlowLabel->SetAlpha(0);
	// set names and images
	m_pAmmoName[0] = new vgui::Label(this, "AmmoNameLabel", "#asw_weapon_rifle");
	//m_pAmmoIcon[0]->SetImage( "swarm/AmmoIcons/AmmoIconRifle" );
	m_pAmmoName[1] = new vgui::Label(this, "AmmoNameLabel", "#asw_weapon_autogun");
	//m_pAmmoIcon[1]->SetImage( "swarm/AmmoIcons/AmmoIconShotgun" );
	m_pAmmoName[2] = new vgui::Label(this, "AmmoNameLabel", "#asw_weapon_shotgun");
	//m_pAmmoIcon[2]->SetImage( "swarm/AmmoIcons/AmmoIconAssault" );
	m_pAmmoName[3] = new vgui::Label(this, "AmmoNameLabel", "#asw_weapon_vind");
	//m_pAmmoIcon[3]->SetImage( "swarm/AmmoIcons/AmmoIconAutogun" );
	m_pAmmoName[4] = new vgui::Label(this, "AmmoNameLabel", "#asw_weapon_flamer");
	//m_pAmmoIcon[4]->SetImage( "swarm/AmmoIcons/AmmoIconFlamer" );
	//m_pAmmoName[5] = new vgui::Label(this, "AmmoNameLabel", "#asw_weapon_rail");
	//m_pAmmoIcon[5]->SetImage( "swarm/AmmoIcons/AmmoIconPistol" );
	m_pAmmoName[5] = new vgui::Label(this, "AmmoNameLabel", "#asw_weapon_pdw");
	//m_pAmmoIcon[6]->SetImage( "swarm/AmmoIcons/AmmoIconPistol" );
	m_pAmmoName[6] = new vgui::Label(this, "AmmoNameLabel", "#asw_weapon_pistol");
	//m_pAmmoIcon[7]->SetImage( "swarm/AmmoIcons/AmmoIconPistol" );
	m_pAmmoName[7] = new vgui::Label(this, "AmmoNameLabel", "#asw_weapon_mining");
	//m_pAmmoIcon[8]->SetImage( "swarm/AmmoIcons/AmmoIconPistol" );
	m_fNextUpdateTime = 0;
	m_hQueuedMarine = NULL;
	m_hMarine = NULL;
	m_bQueuedMarine = false;
	m_bDoingSlowFade = false;
}

CASW_VGUI_Marine_Ammo_Report::~CASW_VGUI_Marine_Ammo_Report()
{

}

#define TEXT_BORDER_X 0.01f
#define TEXT_BORDER_Y 0.012f
#define ASW_AMMO_ICON_SIZE 16

void CASW_VGUI_Marine_Ammo_Report::PerformLayout()
{
	int width = ScreenWidth();
	int height = ScreenHeight();

	m_fScale = height / 768.0f;

	// position elements	
	float y_cursor = TEXT_BORDER_Y * height * 0.5f;
	int left_edge = TEXT_BORDER_X * width;
	m_pTitle->SetWide(width * 0.5f);
	m_pTitle->InvalidateLayout(true);
	
	int name_x = left_edge;
	int icon_x = name_x;
	int count_x = name_x;
	int overflow_x = name_x;
	int clips_icon_x = name_x;
	int tx, ty;
	m_pTitle->GetTextImage()->GetContentSize(tx, ty);
	int right_edge = MAX(count_x, tx + left_edge * 2);
	vgui::HFont ListFont = m_pTitle->GetFont();
	int name_tall = 0;
	if (ListFont!=0)
		name_tall = vgui::surface()->GetFontTall(ListFont);	
	if (name_tall <= 0)
		name_tall = 5;

	// set the widths of the names + counts, and record the widest
	icon_x = left_edge;
	overflow_x = icon_x + name_tall + 1;
	clips_icon_x = overflow_x + m_fScale * 50.0f;
	count_x = clips_icon_x + name_tall + 1;
	int count_width = m_fScale * 25.0f;
	name_x = count_x + count_width;

	for (int i=0;i<ASW_AMMO_REPORT_SLOTS;i++)
	{
		if (!m_pAmmoName[i]->IsVisible())
			continue;
		// just do fixed widths for now		
		m_pAmmoIcon[i]->SetSize(name_tall, name_tall);
		m_pClipsIcon[i]->SetSize(name_tall, name_tall);
		m_pAmmoName[i]->SetSize(width * 0.11f, name_tall);
		m_pAmmoCount[i]->SetSize(width * 0.035f, vgui::surface()->GetFontTall(m_pAmmoCount[i]->GetFont()));
		m_pAmmoOverflow[i]->SetSize(width * 0.05f, vgui::surface()->GetFontTall(m_pAmmoCount[i]->GetFont()));
		m_pAmmoName[i]->InvalidateLayout(true);
		m_pAmmoName[i]->SizeToContents();
		int ammonamew, ammonamet;
		m_pAmmoName[i]->GetContentSize(ammonamew, ammonamet);
		int ammoname_right_edge = name_x + ammonamew + left_edge;
		if (ammoname_right_edge > right_edge)		// keep track of the widest ammo name
		{			
			right_edge = ammoname_right_edge;
		}
	}	
	m_pTitle->SetPos(left_edge, y_cursor);
	y_cursor += vgui::surface()->GetFontTall(m_pTitle->GetFont()) + TEXT_BORDER_Y * height * 0.5f;
	for (int i=0;i<ASW_AMMO_REPORT_SLOTS;i++)
	{
		if (!m_pAmmoName[i]->IsVisible())
			continue;
		m_pAmmoIcon[i]->SetPos(icon_x, y_cursor);
		m_pAmmoName[i]->SetPos(name_x, y_cursor);
		m_pAmmoCount[i]->SetPos(count_x, y_cursor);
		m_pAmmoOverflow[i]->SetPos(overflow_x, y_cursor);
		m_pClipsIcon[i]->SetPos(clips_icon_x, y_cursor);
		
		y_cursor += vgui::surface()->GetFontTall(m_pAmmoName[i]->GetFont());	// drop down by text height or icon height, whichever takes up more space
	}
	count_x += left_edge;	// gap between columns
	
	// place this window in the lower left above the ammo counter
	SetTall(y_cursor + TEXT_BORDER_Y * height);
	SetWide(right_edge);
}

void CASW_VGUI_Marine_Ammo_Report::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetPaintBackgroundType(0);
	SetPaintBackgroundEnabled(true);
	SetBgColor( Color(16,16,16,192) );
	SetAlpha(200);
	SetMouseInputEnabled(false);

	m_pTitle->SetFgColor(Color(255,255,255,255));
	m_pTitle->SetFont(m_hFont);
	for (int i=0;i<ASW_AMMO_REPORT_SLOTS;i++)
	{
		m_pAmmoName[i]->SetFgColor(Color(255,255,255,255));
		m_pAmmoCount[i]->SetFgColor(Color(255,255,255,255));
		m_pAmmoOverflow[i]->SetFgColor(Color(255,255,255,255));
		m_pAmmoName[i]->SetFont(m_hFont);
		m_pAmmoCount[i]->SetFont(m_hFont);
		m_pAmmoOverflow[i]->SetFont(m_hFont);
	}
	m_pGlowLabel->SetFgColor(Color(35,214,250,255));
	m_pGlowLabel->SetFont(m_hGlowFont);
}

// finds if the marine has a weapon which uses the specified ammo type
bool CASW_VGUI_Marine_Ammo_Report::MarineHasWeapon(C_ASW_Marine *pMarine, int iAmmoType)
{
	if (!pMarine)
		return false;

	C_BaseCombatWeapon *pWeapon = pMarine->GetWeapon(0);
	if (pWeapon && pWeapon->GetPrimaryAmmoType() == iAmmoType)
		return true;
	pWeapon = pMarine->GetWeapon(1);
	if (pWeapon && pWeapon->GetPrimaryAmmoType() == iAmmoType)
		return true;
	pWeapon = pMarine->GetWeapon(2);
	if (pWeapon && pWeapon->GetPrimaryAmmoType() == iAmmoType)
		return true;
	
	return false;
}

void CASW_VGUI_Marine_Ammo_Report::SetAmmoTypesVisible()
{	
	C_ASW_Marine* pMarine = GetMarine();
	// if no ammo to show, hide all labels
	if (!pMarine)
	{
		for (int i=0;i<ASW_AMMO_REPORT_SLOTS;i++)
		{
			m_pAmmoName[i]->SetVisible(false);
			m_pAmmoCount[i]->SetVisible(false);
			m_pAmmoOverflow[i]->SetVisible(false);
			m_pAmmoIcon[i]->SetVisible(false);
			m_pClipsIcon[i]->SetVisible(false);
		}
		m_pGlowLabel->SetAlpha(0);
		return;
	}
	bool bShowingGlowLabel = false;
	for (int i=0;i<ASW_AMMO_REPORT_SLOTS;i++)
	{
		int ammo = 0;
		int iAmmoType = -1;
		switch (i)
		{	
			case 1: ammo = pMarine->GetTotalAmmoCount("ASW_AG"); iAmmoType = GetAmmoDef()->Index("ASW_AG"); break;
			case 2: ammo = pMarine->GetTotalAmmoCount("ASW_SG"); iAmmoType = GetAmmoDef()->Index("ASW_SG"); break;
			case 3: ammo = pMarine->GetTotalAmmoCount("ASW_ASG"); iAmmoType = GetAmmoDef()->Index("ASW_ASG"); break;			
			case 4: ammo = pMarine->GetTotalAmmoCount("ASW_F"); iAmmoType = GetAmmoDef()->Index("ASW_F"); break;
			//case 5: ammo = pMarine->GetTotalAmmoCount("ASW_RG"); iAmmoType = GetAmmoDef()->Index("ASW_RG"); break;
			case 5: ammo = pMarine->GetTotalAmmoCount("ASW_PDW"); iAmmoType = GetAmmoDef()->Index("ASW_PDW"); break;
			case 6: ammo = pMarine->GetTotalAmmoCount("ASW_P"); iAmmoType = GetAmmoDef()->Index("ASW_P"); break;
			case 7: ammo = pMarine->GetTotalAmmoCount("ASW_ML"); iAmmoType = GetAmmoDef()->Index("ASW_ML"); break;
			default: ammo = pMarine->GetTotalAmmoCount("ASW_R"); iAmmoType = GetAmmoDef()->Index("ASW_R"); break;			
		}
		bool bShow = ammo > 0;
		if (ammo <= 0)
		{
			// check if the marine actually has this gun, just with no ammo in
			if (MarineHasWeapon(pMarine, iAmmoType))
				bShow = true;
		}
		if (bShow && ASWEquipmentList())
		{
			// find how many clips that many bullets is
			CASW_WeaponInfo* pWeaponData = NULL;
			switch (i)
			{	
				case 1: pWeaponData = ASWEquipmentList()->GetWeaponDataFor("asw_weapon_autogun"); break;
				case 2: pWeaponData = ASWEquipmentList()->GetWeaponDataFor("asw_weapon_shotgun"); break;
				case 3: pWeaponData = ASWEquipmentList()->GetWeaponDataFor("asw_weapon_vindicator"); break;				
				case 4: pWeaponData = ASWEquipmentList()->GetWeaponDataFor("asw_weapon_flamer"); break;;
				//case 5: pWeaponData = ASWEquipmentList()->GetWeaponDataFor("asw_weapon_railgun"); break;;
				case 5: pWeaponData = ASWEquipmentList()->GetWeaponDataFor("asw_weapon_pdw"); break;
				case 6: pWeaponData = ASWEquipmentList()->GetWeaponDataFor("asw_weapon_pistol"); break;
				case 7: pWeaponData = ASWEquipmentList()->GetWeaponDataFor("asw_weapon_mining_laser"); break;
				default: pWeaponData = ASWEquipmentList()->GetWeaponDataFor("asw_weapon_rifle"); break;		
			}
			int overflow = ammo;
			if (pWeaponData && pWeaponData->iMaxClip1 > 0)
			{
				// overflow should actually be the current count in the guns marine holds using this ammo
				overflow = 0;
				for (int w=0;w<3;w++)
				{
					CASW_Weapon *pWeapon = pMarine->GetASWWeapon(w);
					if (pWeapon && pWeapon->GetPrimaryAmmoType() == iAmmoType)
						overflow += pWeapon->Clip1();
				}

				// subtract what's in the gun and divide count into clips
				ammo -= overflow;
				ammo /= pWeaponData->iMaxClip1;

				//int orig = ammo;
				//
				//overflow = orig - ammo * pWeaponData->iMaxClip1;
				
				// put a fresh clip as overflow if needed
				if (overflow == 0 && ammo > 0)
				{
					ammo--;
					overflow = pWeaponData->iMaxClip1;
				}
			}
			else
			{
				ammo = 0;
			}
			m_pAmmoName[i]->SetVisible(true);
			m_pAmmoCount[i]->SetVisible(true);
			//if (overflow > 0)
				m_pAmmoOverflow[i]->SetVisible(true);
			//else
				//m_pAmmoOverflow[i]->SetVisible(false);
			m_pAmmoIcon[i]->SetVisible(true);
			m_pClipsIcon[i]->SetVisible(true);
			char buffer[20];
			if (i == 5)	// PDW hack to show ammo doubled
				Q_snprintf(buffer, sizeof(buffer), "%d", ammo * 2);
			else
				Q_snprintf(buffer, sizeof(buffer), "%d", ammo);
			m_pAmmoCount[i]->SetText(buffer);
			Q_snprintf(buffer, sizeof(buffer), "%d", overflow);
			m_pAmmoOverflow[i]->SetText(buffer);
			m_pAmmoName[i]->SetFgColor(Color(255,255,255,255));
			m_pAmmoCount[i]->SetFgColor(Color(255,255,255,255));
			m_pAmmoOverflow[i]->SetFgColor(Color(255,255,255,255));

			// if we're ready to give this kind of ammo, position the glowlabel behind us
			CASWHudCrosshair *pCrosshair = GET_HUDELEMENT( CASWHudCrosshair );
			if (pCrosshair && pCrosshair->m_iShowGiveAmmoType != -1
				&& pCrosshair->m_iShowGiveAmmoType == iAmmoType)
			{
				char buffer[32];
				m_pAmmoName[i]->GetText(buffer, sizeof(buffer));
				m_pGlowLabel->SetText(buffer);
				int lx, ly, lw, lt;
				m_pAmmoName[i]->GetBounds(lx, ly, lw, lt);
				m_pGlowLabel->SetBounds(lx, ly, lw, lt);
				//int alpha = m_pGlowLabel->GetAlpha();
				//alpha += gpGlobals->frametime * 5.0f;
				//if (alpha > 255)
					//alpha = 255;
				m_pGlowLabel->SetAlpha(255);
				bShowingGlowLabel = true;
				//Msg("matching ammotype %d buffer is %s\nbound are %d,%d %d,%d\n", iAmmoType, buffer, lx, ly, lw, lt);
			}
		}
		else
		{
			m_pAmmoName[i]->SetVisible(false);
			m_pAmmoCount[i]->SetVisible(false);
			m_pAmmoIcon[i]->SetVisible(false);
			m_pClipsIcon[i]->SetVisible(false);
			m_pAmmoOverflow[i]->SetVisible(false);
		}
	}
	if (!bShowingGlowLabel)
	{
		//int alpha = m_pGlowLabel->GetAlpha();
		//if (alpha > 0)
		//{
			//alpha -= gpGlobals->frametime * 5.0f;
			//if (alpha < 0)
				//alpha = 0;
			m_pGlowLabel->SetAlpha(0);
		//}
	}
}

void CASW_VGUI_Marine_Ammo_Report::OnThink()
{
	// make sure the colours are ok, they go grey sometimes, weirdly
	m_pTitle->SetFgColor(Color(255,255,255,255));
	for (int i=0;i<ASW_AMMO_REPORT_SLOTS;i++)
	{
		m_pAmmoName[i]->SetFgColor(Color(255,255,255,255));
		m_pAmmoCount[i]->SetFgColor(Color(255,255,255,255));
		m_pAmmoOverflow[i]->SetFgColor(Color(255,255,255,255));
	}
	C_ASW_Player* pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (pPlayer)
	{
		if (pPlayer->GetMarine() && dynamic_cast<C_ASW_Weapon_Ammo_Bag*>(pPlayer->GetMarine()->GetActiveASWWeapon()))
		{			
			C_ASW_Marine* pAmmoMarine = dynamic_cast<C_ASW_Marine*>(pPlayer->GetHighlightEntity());
			SetMarine(pAmmoMarine);
		}
		else
		{
			SetMarine(NULL);
		}		
	}
	// fade in/out as the marine changes
	if (m_bQueuedMarine)
	{
		if (GetAlpha() <= 0)
		{
			m_hMarine = m_hQueuedMarine;			
			m_hQueuedMarine = NULL;
			m_bQueuedMarine = false;
			m_bDoingSlowFade = false;
			m_fNextUpdateTime = gpGlobals->curtime;
			if (GetMarine())
				vgui::GetAnimationController()->RunAnimationCommand(this, "Alpha", 200, 0, ASW_AMMO_REPORT_FADE_TIME, vgui::AnimationController::INTERPOLATOR_LINEAR);
		}
		else if (GetAlpha() >= 200)
		{
			if (m_hQueuedMarine.Get() == NULL)
			{
				if (!m_bDoingSlowFade)
				{
					m_bDoingSlowFade = true;
					vgui::GetAnimationController()->RunAnimationCommand(this, "Alpha", 0, 1.5f, ASW_AMMO_REPORT_FADE_TIME * 3, vgui::AnimationController::INTERPOLATOR_LINEAR);
				}
			}
			else
			{
				vgui::GetAnimationController()->RunAnimationCommand(this, "Alpha", 0, 0, ASW_AMMO_REPORT_FADE_TIME, vgui::AnimationController::INTERPOLATOR_LINEAR);
				m_bDoingSlowFade = false;
			}
		}
		// if we're doing a slow fade out but the player has now highlighted another marine, do the fade quickly
		if (m_bDoingSlowFade && m_hQueuedMarine.Get() != NULL)
		{
			vgui::GetAnimationController()->RunAnimationCommand(this, "Alpha", 0, 0, ASW_AMMO_REPORT_FADE_TIME, vgui::AnimationController::INTERPOLATOR_LINEAR);
			m_bDoingSlowFade = false;
		}
	}
	// check for updating ammo list
	C_ASW_Marine* pMarine = GetMarine();
	if (gpGlobals->curtime >= m_fNextUpdateTime && pMarine)
	{
		// go through the marine's ammo and list it
		if (pMarine->GetMarineProfile())
		{
			char buffer[64];
			Q_snprintf(buffer, sizeof(buffer), "%s's ammo:", pMarine->GetMarineProfile()->m_ShortName);
			m_pTitle->SetText(buffer);
		}
		SetAmmoTypesVisible();
		PerformLayout();
		m_fNextUpdateTime = gpGlobals->curtime + 1.0f;
	}
	// set up a queue to empty if we're still being shown and we have no marine
	if (!pMarine && GetAlpha() > 0 && !m_bQueuedMarine)
	{
		m_bQueuedMarine = true;
		m_hQueuedMarine = NULL;		
	}
	// reposition	
	if (pMarine)
	{
		int mx,my;
		Vector vecScreenPos;
		debugoverlay->ScreenPosition( pMarine->WorldSpaceCenter(), vecScreenPos );		
		SetPos(vecScreenPos.x, vecScreenPos.y);
		mx = vecScreenPos.x;
		my = vecScreenPos.y;
		
		// align the tooltip to the mous
		vgui::input()->GetCursorPos(mx, my);
		
		// right and down abit
		//mx += 64;
		//my += 16;

		// below the mouse
		my += 32;
		mx -= GetWide() / 2.0f;


		// align it to his portrait
		mx = ScreenWidth() - 135 * m_fScale - GetWide();
		my = ScreenHeight() * 0.025f;
		/*
		my = 0;
		C_ASW_Game_Resource* pGameResource = ASWGameResource();
		if (pGameResource)
		{
			for (int k=0;k<pGameResource->GetMaxMarineResources();k++)
			{
				if (pGameResource->GetMarineResource(k) == pMarine->GetMarineResource())
				{
					my = ((k * 80) + 10) * m_fScale;
					break;
				}
			}
		}
		*/

		// check it's on the screen
		mx = clamp(mx, 0, ScreenWidth() - GetWide());
		my = clamp(my, 0, ScreenHeight() - GetTall());
		
		SetPos(mx, my);
	}
	
}

void CASW_VGUI_Marine_Ammo_Report::Paint()
{	
	if (GetAlpha() > 0)
		PerformLayout();	// need to redo this as the font height calcs don't work properly outside of paint?
	BaseClass::Paint();	
}

void CASW_VGUI_Marine_Ammo_Report::PaintBackground()
{	
	BaseClass::PaintBackground();
}

bool CASW_VGUI_Marine_Ammo_Report::MouseClick(int x, int y, bool bRightClick, bool bDown)
{
	return false;	// not interactive
}

void CASW_VGUI_Marine_Ammo_Report::SetMarine(C_ASW_Marine *pMarine)
{
	if (m_hMarine.Get() != pMarine && !(m_bQueuedMarine && pMarine == m_hQueuedMarine.Get()))
	{
		m_hQueuedMarine = pMarine;
		m_bQueuedMarine = true;		
	}
}

C_ASW_Marine* CASW_VGUI_Marine_Ammo_Report::GetMarine()
{
	return dynamic_cast<C_ASW_Marine*>(m_hMarine.Get());
}