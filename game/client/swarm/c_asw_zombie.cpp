#include "cbase.h"
#include "c_asw_alien.h"
#include "c_asw_zombie.h"
#include "eventlist.h"
#include "decals.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "c_asw_generic_emitter_entity.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_ASW_Zombie, DT_ASW_Zombie, CASW_Zombie)
	RecvPropBool(RECVINFO(m_bOnFire)),
END_RECV_TABLE()

// make sure the clientside aiming knows about the zombie

C_ASW_Zombie::C_ASW_Zombie()
{
#ifdef INFESTED_FIRE
	m_bClientOnFire = false;
#endif
}


C_ASW_Zombie::~C_ASW_Zombie()
{
	m_bOnFire = false;
	UpdateFireEmitters();
}

// todo: need to call this regualarly somewhere
void C_ASW_Zombie::UpdateFireEmitters()
{
#ifdef INFESTED_FIRE
	bool bOnFire = (m_bOnFire && !IsEffectActive(EF_NODRAW));
	if (bOnFire != m_bClientOnFire)
	{
		m_bClientOnFire = bOnFire;
		if (m_bClientOnFire)
		{
			// spawn some emitter(s) burning us
			for (int i=0;i<ASW_NUM_FIRE_EMITTERS;i++)
			{
				C_ASW_Emitter *pEmitter = new C_ASW_Emitter;
				if (pEmitter)
				{
					if (pEmitter->InitializeAsClientEntity( NULL, false ))
					{
						Q_snprintf(pEmitter->m_szTemplateName, sizeof(pEmitter->m_szTemplateName), "envfireslow");
						pEmitter->m_fScale = 1.0f;
						pEmitter->m_bEmit = true;
						pEmitter->CreateEmitter();
						pEmitter->SetAbsOrigin(WorldSpaceCenter());			
						pEmitter->SetAbsAngles(QAngle(0,0,0));
						pEmitter->SetParent(this);
					}
					m_hFireEmitters[i] = pEmitter;
				}
			}
			EmitSound( "ASWFire.BurningFlesh" );
		}
		else
		{
			// get rid of our emitters
			for (int i=0;i<ASW_NUM_FIRE_EMITTERS;i++)
			{
				if (m_hFireEmitters[i].Get())
				{
					m_hFireEmitters[i]->m_bEmit = false;
					m_hFireEmitters[i]->SetDieTime(gpGlobals->curtime);
					m_hFireEmitters[i] = NULL;
				}
			}	
			StopSound("ASWFire.BurningFlesh");
			if ( C_BaseEntity::IsAbsQueriesValid() )
				EmitSound("ASWFire.StopBurning");
		}
	}
#endif
}

void C_ASW_Zombie::UpdateOnRemove()
{
	BaseClass::UpdateOnRemove();
	m_bOnFire = false;
	UpdateFireEmitters();
}