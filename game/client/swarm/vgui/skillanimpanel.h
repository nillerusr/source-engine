#ifndef _INCLUDED_SKILLANIMPANEL_H
#define _INCLUDED_SKILLANIMPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>

// this panel handles animations over the marine's skill buttons when someone upgrades a skill

struct SpendParticle
{
	SpendParticle()
	{
		m_fXPos = 0;
		m_fYPos = 0;
		m_fXSpeed = 0;
		m_fYSpeed = 0;
		m_fLifetime = 1.0f;
	};

	float m_fXPos;
	float m_fYPos;
	float m_fXSpeed;
	float m_fYSpeed;
	float m_fLifetime;
};

class SkillAnimPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( SkillAnimPanel, vgui::Panel );
public:
	SkillAnimPanel(vgui::Panel *parent, const char *panelName);
	virtual ~SkillAnimPanel();
	virtual void Paint();
	virtual void PaintParticles();
	virtual void PaintParticle(SpendParticle* p);
	virtual void PerformLayout();

	virtual void AddParticlesAroundPanel(vgui::Panel* pPanel);
	virtual SpendParticle* AddParticle(int xPos, int yPos, float xSpeed, float ySpeed);
	virtual float GetLifeFraction(SpendParticle* p);
	virtual void OnThink();
	virtual void UpdateParticles();

	CUtlVector<SpendParticle*> m_Particles;
		
	CPanelAnimationVarAliasType( int, m_nParticleTexture, "ParticleTexture", "vgui/swarm/Briefing/SkillParticle", "textureid" );
};


#endif // _INCLUDED_SKILLANIMPANEL_H