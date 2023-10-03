#include "cbase.h"
#include "c_asw_sentry_top.h"
#include "c_asw_sentry_base.h"
#include "c_asw_sentry_top_icer.h"
#include <vgui/ISurface.h>
#include <vgui_controls/Panel.h>
#include "c_te_legacytempents.h"
#include "c_asw_fx.h"
#include "c_asw_generic_emitter.h"
#include "c_user_message_register.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "soundenvelope.h"
#include "ai_debug_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT( C_ASW_Sentry_Top_Icer, DT_ASW_Sentry_Top_Icer, CASW_Sentry_Top_Icer)	
END_RECV_TABLE()

/*
BEGIN_PREDICTION_DATA( C_ASW_Sentry_Top_Icer )
END_PREDICTION_DATA()
*/

extern ConVar asw_sentry_emitter_test;

C_ASW_Sentry_Top_Icer::C_ASW_Sentry_Top_Icer()  
{
	m_szParticleEffectFireName = "asw_freezer_spray";

	m_szDuringFireSoundScriptName = "ASW_Sentry.IceLoop" ;
	m_szEndFireSoundScriptName = "ASW_Sentry.IceStop" ;
}

/*
void C_ASW_Sentry_Top_Icer::CreateObsoleteEmitters( void )
{
	AssertMsg( m_OldStyleEmitters.Count() == 0, "Flame turret tried to create emitters when it already had some!" );

	// create two emitter handles (use default constructor to pull them to NULL to begin with)
	m_OldStyleEmitters.EnsureCount(1);

	CSmartPtr<CASWGenericEmitter> &m_hFireExtinguisherEmitter = m_OldStyleEmitters[0];

	m_hFireExtinguisherEmitter = CASWGenericEmitter::Create( "asw_emitter" );
	if ( m_hFireExtinguisherEmitter.IsValid() )
	{			
		m_hFireExtinguisherEmitter->UseTemplate("fireextinguisher2");
		m_hFireExtinguisherEmitter->SetActive(false);
		m_hFireExtinguisherEmitter->m_hCollisionIgnoreEntity = this;
	}
	else
	{
		Warning("Failed to create a turret's fire extinguisher emitter\n");
	}
}
*/
