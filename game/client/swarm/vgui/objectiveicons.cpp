#include "cbase.h"

#include "c_asw_game_resource.h"
#include "ObjectiveIcons.h"
#include "c_asw_player.h"
#include "c_asw_objective.h"
#include "vgui_controls/AnimationController.h"
#include "ObjectiveTitlePanel.h"
#include "ObjectiveDetailsPanel.h"
#include <vgui/ISurface.h>
#include <vgui_controls/ImagePanel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

ObjectiveIcons::ObjectiveIcons(Panel *parent, const char *name) : Panel(parent, name)
{	
	for (int i=0;i<5;i++)
	{
		m_pIcon[i] = new vgui::ImagePanel(this, "ObjectiveIconImage");
		if (m_pIcon[i])
		{
			m_pIcon[i]->SetShouldScaleImage(true);
			m_pIcon[i]->SetMouseInputEnabled(false);
		}
	}
	m_iNumFading = 1;
	m_pObjective = NULL;
	m_pQueuedObjective = NULL;
	m_bHaveQueued = false;
}
	
ObjectiveIcons::~ObjectiveIcons()
{

}

void ObjectiveIcons::PerformLayout()
{
	int width = ScreenWidth();
	int height = ScreenHeight();

	int map_edge = (width * 0.3f) + (height * 0.66666f);

	float fIconSize = height * 0.1f;
	int padding = (width/1024.0f) * 28.0f;
	SetSize(fIconSize * 1.01 + padding, fIconSize * 1.1f * m_iNumFading + padding * 0.5f);
	SetPos(map_edge + padding, height * 0.03f);
	for (int i=0;i<5;i++)
	{
		m_pIcon[i]->SetPos(padding * 0.5f, fIconSize * 1.1f * i + padding * 0.5f);
		m_pIcon[i]->SetSize(fIconSize, fIconSize);
	}
}

void ObjectiveIcons::OnThink()
{
	BaseClass::OnThink();

	// this is no longer being used
	/*
	if (m_bHaveQueued)
	{
		if (m_pIcon[0] && m_pIcon[0]->GetAlpha() <= 0)
		{
			m_bHaveQueued = false;
			m_pObjective = m_pQueuedObjective;
			m_pQueuedObjective = NULL;
			if (m_pObjective)
			{
				char buffer[255];
				m_iNumFading = 0;				
				for (int i=0;i<5;i++)
				{
					if (m_pObjective->GetInfoIcon(i) && m_pObjective->GetInfoIcon(i)[0] != 0)
					{
						Q_snprintf(buffer, sizeof(buffer), "swarm/ObjectiveIcons/%s", m_pObjective->GetInfoIcon(i));	
						m_pIcon[i]->SetImage(buffer);			
						// fade it in, in sequence
						int xpos, ypos;
						m_pIcon[i]->GetPos(xpos, ypos);
						//Msg("fading in %d which has ypos %d\n", i, ypos);
						vgui::GetAnimationController()->RunAnimationCommand(m_pIcon[i], "alpha", 255, 0.2f * i, 0.3f, vgui::AnimationController::INTERPOLATOR_LINEAR);
						m_pIcon[i]->SetDrawColor(Color(66,142,192,255));
						m_iNumFading++;
					}					
				}
				if (m_iNumFading > 0)
				{
					int icon_size = ScreenHeight() * 0.1f;					
					int padding = ScreenHeight() * 0.0125f;
					SetTall(icon_size * 1.1f * m_iNumFading + padding * 0.5f);
					vgui::GetAnimationController()->RunAnimationCommand(this, "alpha", 255, 0, 0.3f, vgui::AnimationController::INTERPOLATOR_LINEAR);

					CLocalPlayerFilter filter;
					C_BaseEntity::EmitSound( filter, -1, "ASWInterface.MissionBoxes" );
				}
			}
		}
	}
	*/
}

void ObjectiveIcons::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetBgColor(Color(30,66,89,164));
	//SetBorder(pScheme->GetBorder("ASWBriefingButtonBorder"));
	//SetPaintBorderEnabled(true);
	//SetPaintBackgroundEnabled(false);
	for (int i=0;i<5;i++)
	{
		m_pIcon[i]->SetAlpha(0);
		m_pIcon[i]->SetPaintBackgroundEnabled(true);
		//m_pIcon[i]->SetFillColor(Color(30,66,89,164));
		//m_pIcon[i]->SetFillColor(Color(0,0,0,64));
		m_pIcon[i]->SetDrawColor(pScheme->GetColor("LightBlue", Color(255,255,255,255)));
	}
	SetAlpha(0);
}

void ObjectiveIcons::FadeOut()
{
	for (int i=0;i<5;i++)
	{
		if (m_pIcon[i]->GetAlpha() > 0)
			vgui::GetAnimationController()->RunAnimationCommand(m_pIcon[i], "alpha", 0, 0, 0.3f, vgui::AnimationController::INTERPOLATOR_LINEAR);
	}
	vgui::GetAnimationController()->RunAnimationCommand(this, "alpha", 0, 0, 0.3f, vgui::AnimationController::INTERPOLATOR_LINEAR);
}

void ObjectiveIcons::SetObjective(C_ASW_Objective* pObjective)
{
	if (pObjective != m_pObjective 
		&& pObjective != m_pQueuedObjective)
	{
		m_pQueuedObjective = pObjective;
		m_bHaveQueued = true;
		FadeOut();
	}	
}