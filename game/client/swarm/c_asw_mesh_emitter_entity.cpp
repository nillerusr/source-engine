#include "cbase.h"
#include "c_te_effect_dispatch.h"
#include "c_asw_mesh_emitter_entity.h"
#include "c_asw_generic_emitter.h"
#include "model_types.h"
#include "engine/IVDebugOverlay.h"
#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// this is a particle emitter where each particle is a mesh instead of a sprite

LINK_ENTITY_TO_CLASS( client_asw_mesh_emitter, C_ASW_Mesh_Emitter );

C_ASW_Mesh_Emitter::C_ASW_Mesh_Emitter()
{
	DisableCachedRenderBounds( true );

	m_fScale = 1.0f;
	m_fDieTime = false;

	m_hClientAttach = NULL;
	m_szAttach[0] = 0;

	m_bFrozen = false;
}

//-----------------------------------------------------------------------------
// Purpose: Called when data changes on the server
//-----------------------------------------------------------------------------
void C_ASW_Mesh_Emitter::OnDataChanged( DataUpdateType_t updateType )
{
	// NOTE: We MUST call the base classes' implementation of this function
	BaseClass::OnDataChanged( updateType );	

	// Setup our entity's particle system on creation
	if ( updateType == DATA_UPDATE_CREATED )
	{
		CreateEmitter(vec3_origin);
	}
	//if (m_hEmitter.IsValid() && !strcmp(m_szTemplateName, m_hEmitter->GetTemplateName()))
	//m_hEmitter->UseTemplate(m_szTemplateName);

	m_hEmitter->SetActive(m_bEmit);
	m_hEmitter->Update();	
}

void C_ASW_Mesh_Emitter::CreateEmitter(const Vector &force)
{
	// Creat the emitter
	m_hEmitter = CASWGenericEmitter::Create( "asw_emitter" );
	m_hEmitter->SetSortOrigin(GetAbsOrigin());		

	// Obtain a reference handle to our particle's desired material
	if ( m_hEmitter.IsValid() )
	{			
		m_hEmitter->UseTemplate(m_szTemplateName);
		m_hEmitter->velocityMin += force;
		m_hEmitter->velocityMax += force;		
		m_hEmitter->SetMeshEmitter(this);
		m_hEmitter->SetEmitterScale(m_fScale);
		//m_hMaterial = m_hEmitter->GetPMaterial( "effects/yellowflare" );
		//sprintf(m_szMaterialName, "effects/yellowflare");
		m_hEmitter->m_bHullTraces = true;
		m_hEmitter->m_fReduceRollRateOnCollision = 0.7f;		
		m_hEmitter->SetActive(m_bEmit);
		m_hEmitter->Update();
	}

	// Call our ClientThink() function once every client frame
	SetNextClientThink( CLIENT_THINK_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose: Client-side think function for the entity
//-----------------------------------------------------------------------------
void C_ASW_Mesh_Emitter::ClientThink( void )
{
	// We must have a valid emitter
	if ( m_hEmitter == NULL )
	{
		return;
	}

	// if we're clientside attached to something, update our position/angle
	if (m_hClientAttach.Get())
	{
		Vector pos;
		QAngle ang;
		m_hClientAttach->GetAttachment( m_hClientAttach->LookupAttachment( m_szAttach ), pos, ang );
		SetAbsOrigin(pos);
		SetAbsAngles(ang);
	}

	m_hEmitter->Think(gpGlobals->frametime, GetAbsOrigin(), GetAbsAngles());
	if (m_fScale != m_fDesiredScale)
	{
		if (m_fScale < m_fDesiredScale)
		{
			m_fScale = MIN(m_fScale + m_fScaleRate * gpGlobals->frametime, m_fDesiredScale);			
		}
		else
		{
			m_fScale = MAX(m_fScale - m_fScaleRate * gpGlobals->frametime, m_fDesiredScale);			
		}
	}
	m_hEmitter->SetEmitterScale(m_fScale);
	if (gpGlobals->curtime > m_fDieTime && m_fDieTime != 0)
	{
		Die();
	}
}


void C_ASW_Mesh_Emitter::UseTemplate(const char* pTemplateName, bool bLoadFromCache)
{
	if (m_hEmitter == NULL)
		return;

	strcpy(m_szTemplateName, pTemplateName);
	m_hEmitter->UseTemplate(pTemplateName,true,bLoadFromCache);
	m_hEmitter->SetMeshEmitter(this);
}

void C_ASW_Mesh_Emitter::SaveAsTemplate(const char* pTemplateName)
{
	if (m_hEmitter == NULL)
		return;

	strcpy(m_szTemplateName, pTemplateName);
	m_hEmitter->SaveTemplateAs(pTemplateName);
}

void C_ASW_Mesh_Emitter::SetDieTime(float fDieTime)
{
	m_fDieTime = fDieTime;
}

void C_ASW_Mesh_Emitter::Die()
{
	// make sure our emitter knows to die
	if (!(m_hEmitter == NULL))
	{
		m_hEmitter->SetDieTime(m_fDieTime);
	}
	Release();
}

void C_ASW_Mesh_Emitter::ClientAttach(C_BaseAnimating *pParent, const char *szAttach)
{
	strcpy(m_szAttach, szAttach);
	m_hClientAttach = pParent;
}

bool C_ASW_Mesh_Emitter::PrepareToDraw()
{
	if ( !GetModel() )
		return false;

	if (!m_hEmitter)
		return false;

	// This should never happen, but if the server class hierarchy has bmodel entities derived from CBaseAnimating or does a
	//  SetModel with the wrong type of model, this could occur.
	if ( modelinfo->GetModelType( GetModel() ) != mod_studio )
	{
		return false;
	}

	// Make sure hdr is valid for drawing
	if ( !GetModelPtr() )
		return false;

	return true;
}

// don't use fast path
IClientModelRenderable*	C_ASW_Mesh_Emitter::GetClientModelRenderable()
{ 
	return NULL;
}

int C_ASW_Mesh_Emitter::InternalDrawModel( int flags, const RenderableInstance_t &instance )
{
	VPROF( "C_ASW_Mesh_Emitter::InternalDrawModel" );

	if ( !GetModel() )
		return 0;

	if (!m_hEmitter)
		return 0;

	// This should never happen, but if the server class hierarchy has bmodel entities derived from CBaseAnimating or does a
	//  SetModel with the wrong type of model, this could occur.
	if ( modelinfo->GetModelType( GetModel() ) != mod_studio )
	{
		return BaseClass::DrawModel( flags, instance );
	}

	// Make sure hdr is valid for drawing
	if ( !GetModelPtr() )	
		return 0;	

	if ( IsEffectActive( EF_ITEM_BLINK ) )
	{
		flags |= STUDIO_ITEM_BLINK;
	}

	ModelRenderInfo_t sInfo;
	sInfo.flags = flags;
	sInfo.pRenderable = this;
	sInfo.instance = GetModelInstance();
	sInfo.entity_index = index;
	sInfo.pModel = GetModel();
	sInfo.origin = GetRenderOrigin();
	sInfo.angles = GetRenderAngles();
	sInfo.skin = m_nSkin;
	sInfo.body = m_nBody;
	sInfo.hitboxset = m_nHitboxSet;

	Vector vecPreDrawPos = GetAbsOrigin();
	QAngle angPreDrawAngles = GetAbsAngles();

	int drawn = 0;
	for (int i=0; i<m_hEmitter->GetBinding().m_Materials.Count(); i++)
	{
		CEffectMaterial *pMaterial = m_hEmitter->GetBinding().m_Materials[i];
		if (pMaterial)
		{
			Particle *pCur = pMaterial->m_Particles.m_pNext;
			while (pCur != &pMaterial->m_Particles && pCur != NULL)
			{
				InvalidateBoneCaches();
				SetAbsOrigin( pCur->m_Pos );
				ASWParticle *pParticle = static_cast<ASWParticle*>(pCur);
				if (pParticle)
				{
					float roll = RAD2DEG(pParticle->m_flRoll);
					//Msg("Setting angle to roll %f\n", pParticle->m_flRoll);
					SetAbsAngles( QAngle(roll, roll, roll) );
				}
				else
				{
					Msg("Couldn't get aswparticle\n");
				}
				sInfo.origin = pCur->m_Pos;
				sInfo.angles = GetAbsAngles();
				drawn += modelrender->DrawModelEx( sInfo );
				pCur = pCur->m_pNext;
			}
		}
	}

	SetAbsOrigin(vecPreDrawPos);
	SetAbsAngles(angPreDrawAngles);


	if (drawn > 0)
	{
		drawn = 1;
	}

	return drawn;
}

int C_ASW_Mesh_Emitter::DrawParticle(Vector &vecPos)
{
	VPROF( "C_ASW_Mesh_Emitter::DrawParticle" );

	ModelRenderInfo_t sInfo;
	sInfo.flags = STUDIO_RENDER;
	sInfo.pRenderable = this;
	sInfo.instance = GetModelInstance();
	sInfo.entity_index = index;
	sInfo.pModel = GetModel();
	sInfo.origin = vecPos;
	sInfo.angles = QAngle(0,0,0);
	sInfo.skin = m_nSkin;
	sInfo.body = m_nBody;
	sInfo.hitboxset = m_nHitboxSet;

	int drawn = modelrender->DrawModelEx( sInfo );	// asw	 1.0f, 0.2f

	return drawn;
}

void C_ASW_Mesh_Emitter::GetRenderBounds( Vector& theMins, Vector& theMaxs )
{
	if (m_hEmitter.IsValid())
	{
		m_hEmitter->GetBinding().GetRenderBounds(theMins, theMaxs);
		return;
	}
	BaseClass::GetRenderBounds(theMins, theMaxs);
}