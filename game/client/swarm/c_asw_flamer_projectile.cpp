#include "cbase.h"
#include "c_asw_flamer_projectile.h"
#include "dlight.h"
#include "iefx.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_ASW_Flamer_Projectile, DT_ASW_Flamer_Projectile, CASW_Flamer_Projectile)
	
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_ASW_Flamer_Projectile )

END_PREDICTION_DATA()

ConVar asw_flamer_light_scale("asw_flamer_light_scale", "0.7f", FCVAR_CHEAT, "Alters the size of the flamer dynamic light");
ConVar asw_flamer_light_r("asw_flamer_light_r", "255", FCVAR_CHEAT, "Alters the colour of the flamer dynamic light");
ConVar asw_flamer_light_g("asw_flamer_light_g", "192", FCVAR_CHEAT, "Alters the colour of the flamer dynamic light");
ConVar asw_flamer_light_b("asw_flamer_light_b", "160", FCVAR_CHEAT, "Alters the colour of the flamer dynamic light");
ConVar asw_flamer_light_exponent("asw_flamer_light_exponent", "5", FCVAR_CHEAT, "Alters the flamer dynamic light");

C_ASW_Flamer_Projectile::C_ASW_Flamer_Projectile()
{
	m_pDynamicLight = 0;
}


C_ASW_Flamer_Projectile::~C_ASW_Flamer_Projectile()
{
	if (m_pDynamicLight)
	{
		m_pDynamicLight->die = gpGlobals->curtime;
	}
}

void C_ASW_Flamer_Projectile::CreateLight()
{
	m_pDynamicLight = effects->CL_AllocDlight( index );
	m_pDynamicLight->origin = GetAbsOrigin();
	m_pDynamicLight->radius = 61.6 * asw_flamer_light_scale.GetFloat(); 
	m_pDynamicLight->decay = 0 / 0.05f;
	m_pDynamicLight->die = gpGlobals->curtime + 1.0f;
	m_pDynamicLight->color.r = asw_flamer_light_r.GetFloat();
	m_pDynamicLight->color.g = asw_flamer_light_g.GetFloat();	
	m_pDynamicLight->color.b = asw_flamer_light_b.GetFloat();	
	m_pDynamicLight->color.exponent = asw_flamer_light_exponent.GetInt();
}

void C_ASW_Flamer_Projectile::ClientThink(void)
{
	if (m_pDynamicLight)
	{
		m_pDynamicLight->radius += 78.4f * gpGlobals->frametime * asw_flamer_light_scale.GetFloat(); // was 140 from radius 0
		m_pDynamicLight->origin = GetAbsOrigin();
		float f = m_pDynamicLight->die - gpGlobals->curtime;
		if (f < 0.0f)
			f = 0.0f;
		if (f > 1.0f)
			f = 1.0f;
		m_pDynamicLight->color.r = asw_flamer_light_r.GetFloat() * f;
		m_pDynamicLight->color.g = asw_flamer_light_g.GetFloat() * f;	
		m_pDynamicLight->color.b = asw_flamer_light_b.GetFloat() * f;
	}

	SetNextClientThink(CLIENT_THINK_ALWAYS);//gpGlobals->curtime + 0.001
}

void C_ASW_Flamer_Projectile::OnDataChanged(DataUpdateType_t updateType)
{
	if ( updateType == DATA_UPDATE_CREATED )
	{
		CreateLight();
		SetNextClientThink(CLIENT_THINK_ALWAYS);
	}
	BaseClass::OnDataChanged(updateType);
}