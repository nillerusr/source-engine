#include "cbase.h"
#include "SkillAnimPanel.h"
#include "vgui/isurface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

ConVar asw_skill_particle_size("asw_skill_particle_size", "40.0", 0, "Size of particles for the skill spending effect");
ConVar asw_skill_particle_gravity("asw_skill_particle_gravity", "0.0", 0, "Gravity for particles for the skill spending effect");
ConVar asw_skill_particle_count("asw_skill_particle_count", "20", 0, "How many particles are spawned for each edge of the skill button");
ConVar asw_skill_particle_deviation("asw_skill_particle_deviation", "0.0", 0, "Random deviation applied to initial velocity of skill spending particles");
ConVar asw_skill_particle_speed("asw_skill_particle_speed", "50.0f", 0, "Initial speed of skill spend particles");
ConVar asw_skill_particle_speed_up("asw_skill_particle_speed_up", "0.0", 0, "Upward boost given to skill spend particles");
ConVar asw_skill_particle_inset("asw_skill_particle_inset", "10.0", 0, "How many pixels inset inside the skill button to spawn particles");

SkillAnimPanel::SkillAnimPanel(vgui::Panel* parent, const char *panelName) :
	vgui::Panel(parent, panelName)
{
	SetMouseInputEnabled(false);
}

SkillAnimPanel::~SkillAnimPanel()
{
	m_Particles.PurgeAndDeleteElements();
}

void SkillAnimPanel::Paint()
{
	PaintParticles();
}

void SkillAnimPanel::PaintParticles()
{
	vgui::surface()->DrawSetTexture(m_nParticleTexture);

	int c = m_Particles.Count();
	for (int i=01;i<c;i++)
	{
		SpendParticle* p = m_Particles[i];
		if (!p)
			continue;

		PaintParticle(p);
	}
}

void SkillAnimPanel::PaintParticle(SpendParticle* p)
{
	if (!p)
		return;

	Color col(255, 255, 255, GetLifeFraction(p) * 255.0f);
	vgui::surface()->DrawSetColor(col);
	
	float fScale = ScreenHeight() / 768.0f;
	float size = fScale * asw_skill_particle_size.GetFloat() * (1.0f - GetLifeFraction(p));	// particles get bigger as life runs out
	float x = p->m_fXPos;
	float y = p->m_fYPos;
	vgui::Vertex_t points[4] = 
	{ 
	vgui::Vertex_t( Vector2D(x-size, y-size), Vector2D(0,0) ), 
	vgui::Vertex_t( Vector2D(x+size, y-size), Vector2D(1,0) ), 
	vgui::Vertex_t( Vector2D(x+size, y+size), Vector2D(1,1) ), 
	vgui::Vertex_t( Vector2D(x-size, y+size), Vector2D(0,1) ) 
	}; 
	vgui::surface()->DrawTexturedPolygon( 4, points );
}

void SkillAnimPanel::PerformLayout()
{
	BaseClass::PerformLayout();
	if (!GetParent())
		return;

	SetBounds(0, 0, GetParent()->GetWide(), GetParent()->GetTall());
}

void SkillAnimPanel::AddParticlesAroundPanel(vgui::Panel* pPanel)
{
	if (!pPanel)
		return;

	// find the target panel's location relative to ours
	int mx, my;
	mx = my = 0;
	LocalToScreen(mx, my);

	int px, py;
	px = py = 0;
	pPanel->LocalToScreen(px, py);

	int x = px - mx;
	int y = py - my;

	x += asw_skill_particle_inset.GetInt();
	y += asw_skill_particle_inset.GetInt();
	int width = pPanel->GetWide() - (asw_skill_particle_inset.GetInt() * 2);
	int height = pPanel->GetTall() - (asw_skill_particle_inset.GetInt() * 2);

	const int particles_per_edge = asw_skill_particle_count.GetInt();	// future todo? could make this scale with wider panels
	float x_interval = float(width) / float(particles_per_edge);
	float y_interval = float(height) / float(particles_per_edge);

	float speed = asw_skill_particle_speed.GetFloat();
		
	for (int i=0;i<particles_per_edge;i++)
	{
		float sidespeed = ((particles_per_edge * 0.5f) - i) / (particles_per_edge * 0.5) * speed;		

		// top edge	
		AddParticle(x + i * x_interval, y, -sidespeed, -speed);

		// bottom edge	
		AddParticle(x + i * x_interval, y + height, -sidespeed, speed);

		// left edge	
		AddParticle(x, y + i * y_interval, -speed, -sidespeed);

		// right edge		
		AddParticle(x + width, y + i * y_interval, speed, -sidespeed);
	}
}

#define ASW_SKILL_PARTICLE_LIFETIME 0.4f

float SkillAnimPanel::GetLifeFraction(SpendParticle* p)
{
	if (!p)
		return 0;

	return p->m_fLifetime / ASW_SKILL_PARTICLE_LIFETIME;
}

SpendParticle* SkillAnimPanel::AddParticle(int xPos, int yPos, float xSpeed, float ySpeed)
{
	float fRandomX = random->RandomFloat(1.0f - asw_skill_particle_deviation.GetFloat(), 
										 1.0f + asw_skill_particle_deviation.GetFloat());
	float fRandomY = random->RandomFloat(1.0f - asw_skill_particle_deviation.GetFloat(), 
										 1.0f + asw_skill_particle_deviation.GetFloat());

	SpendParticle* p = new SpendParticle();
	p->m_fXPos = xPos;
	p->m_fYPos = yPos;
	p->m_fXSpeed = xSpeed * fRandomX;
	p->m_fYSpeed = (ySpeed - asw_skill_particle_speed_up.GetFloat()) * fRandomY;
	p->m_fLifetime = ASW_SKILL_PARTICLE_LIFETIME;	

	m_Particles.AddToTail(p);

	return p;
}

void SkillAnimPanel::OnThink()
{
	UpdateParticles();
}

void SkillAnimPanel::UpdateParticles()
{
	float fDelta = gpGlobals->frametime;
	int c = m_Particles.Count();
	for (int i=c-1;i>=0;i--)
	{
		SpendParticle* p = m_Particles[i];
		if (!p)
			continue;
		
		p->m_fLifetime -= fDelta;
		if (p->m_fLifetime <= 0)
		{
			m_Particles.Remove(i);
			delete p;
		}
		else
		{
			// apply a bit of gravity
			p->m_fYSpeed += asw_skill_particle_gravity.GetFloat() * fDelta;

			p->m_fXPos += p->m_fXSpeed * fDelta;
			p->m_fYPos += p->m_fYSpeed * fDelta;			
		}
	}
}