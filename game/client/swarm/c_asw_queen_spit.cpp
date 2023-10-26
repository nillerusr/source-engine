#include "cbase.h"
#include "precache_register.h"
#include "particles_simple.h"
#include "iefx.h"
#include "dlight.h"
#include "view.h"
#include "fx.h"
#include "clientsideeffects.h"
#include "c_pixel_visibility.h"
#include "c_asw_queen_spit.h"
#include "c_asw_generic_emitter_entity.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT( C_ASW_Queen_Spit, DT_ASW_Queen_Spit, CASW_Queen_Spit )
	
END_RECV_TABLE()


C_ASW_Queen_Spit::C_ASW_Queen_Spit()
{
	m_pGooEmitter = NULL;
	m_bCreatedEffects = false;
}

C_ASW_Queen_Spit::~C_ASW_Queen_Spit( void )
{
	// kill effects
	if (m_pGooEmitter)
	{
		m_pGooEmitter->SetDieTime(gpGlobals->curtime);
		m_pGooEmitter = NULL;
	}
}

void C_ASW_Queen_Spit::CreateEffects()
{
	if (m_bCreatedEffects)
		return;

	m_bCreatedEffects = true;

	m_pGooEmitter = new C_ASW_Emitter;
	if (m_pGooEmitter)
	{
		if (m_pGooEmitter->InitializeAsClientEntity( NULL, false ))
		{			
			Q_snprintf(m_pGooEmitter->m_szTemplateName, sizeof(m_pGooEmitter->m_szTemplateName), "queenspit");			
			m_pGooEmitter->m_fScale = 2.0f;
			m_pGooEmitter->m_bEmit = true;
			m_pGooEmitter->CreateEmitter();
			m_pGooEmitter->SetAbsOrigin(GetAbsOrigin());			
			m_pGooEmitter->SetAbsAngles(GetAbsAngles());
			m_pGooEmitter->ClientAttach(this, "bleed1");
		}
		else
		{
			UTIL_Remove( m_pGooEmitter );
		}
	}
}

void C_ASW_Queen_Spit::OnDataChanged( DataUpdateType_t updateType )
{
	if ( updateType == DATA_UPDATE_CREATED )
	{
		CreateEffects();
	}

	BaseClass::OnDataChanged( updateType );
}

void C_ASW_Queen_Spit::OnRestore()
{
	CreateEffects();
	BaseClass::OnRestore();
}