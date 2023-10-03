#include "cbase.h"
#include "dlight.h"
#include "iefx.h"
#include "IViewRender.h"
#include "c_asw_dynamic_light.h"
#include "asw_shareddefs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_ASW_DynamicLight, DT_ASW_Dynamic_Light, CASW_Dynamic_Light)
RecvPropInt		(RECVINFO(m_Flags)),
RecvPropInt		(RECVINFO(m_LightStyle)),
RecvPropFloat	(RECVINFO(m_Radius)),
RecvPropInt		(RECVINFO(m_Exponent)),
RecvPropFloat	(RECVINFO(m_InnerAngle)),
RecvPropFloat	(RECVINFO(m_OuterAngle)),
RecvPropFloat	(RECVINFO(m_SpotRadius)),
END_RECV_TABLE()

C_ASW_DynamicLight::C_ASW_DynamicLight(void) : m_pSpotlightEnd(0)
{
	m_pDynamicLight = NULL;
}

C_ASW_DynamicLight::~C_ASW_DynamicLight()
{
	if (m_pDynamicLight)
	{
		m_pDynamicLight->die = gpGlobals->curtime;
		m_pDynamicLight = NULL;
	}
}

void C_ASW_DynamicLight::OnDataChanged(DataUpdateType_t updateType)
{
	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink(gpGlobals->curtime + 0.05);
	}
}

bool C_ASW_DynamicLight::ShouldDraw()
{
	//return false;
	return true;
}

void C_ASW_DynamicLight::ClientThink(void)
{
	Vector forward;
	AngleVectors( GetAbsAngles(), &forward );

	if ( (m_Flags & DLIGHT_NO_MODEL_ILLUMINATION) == 0 )
	{
		if (!m_pDynamicLight || m_pDynamicLight->key != ASW_LIGHT_INDEX_FIRES + index)
		{
			m_pDynamicLight = effects->CL_AllocDlight( ASW_LIGHT_INDEX_FIRES + index );	
		}
		m_pDynamicLight->color.b = GetRenderColorB();
		m_pDynamicLight->color.g = GetRenderColorG();
		m_pDynamicLight->color.r = GetRenderColorR();

		m_pDynamicLight->origin	= GetAbsOrigin();
		m_pDynamicLight->radius = m_Radius;
		m_pDynamicLight->color.exponent = m_Exponent;

		m_pDynamicLight->die		= gpGlobals->curtime + 30.0f;
	}
	else
	{
		// In this case, the m_Flags could have changed; which is how we turn the light off
		if (m_pDynamicLight)
		{
			m_pDynamicLight->die = gpGlobals->curtime;
			m_pDynamicLight = 0;
		}
	}

	SetNextClientThink(gpGlobals->curtime + 0.001);
}

