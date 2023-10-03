#include "cbase.h"
#include "c_te_effect_dispatch.h"
#include "c_asw_generic_emitter_entity.h"
#include "c_asw_generic_emitter.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( client_asw_emitter, C_ASW_Emitter );

IMPLEMENT_CLIENTCLASS_DT( C_ASW_Emitter, DT_ASW_Emitter, CASW_Emitter )	
	RecvPropInt( RECVINFO( m_bEmit ) ),
	RecvPropString( RECVINFO(m_szTemplateName) ),
	RecvPropFloat( RECVINFO(m_fDesiredScale) ),
	RecvPropFloat( RECVINFO(m_fScaleRate) ),	
END_RECV_TABLE()

C_ASW_Emitter::C_ASW_Emitter()
{
	m_fScale = 1.0f;
	m_fDieTime = false;

	m_hClientAttach = NULL;
	m_szAttach[0] = 0;
}
//-----------------------------------------------------------------------------
// Purpose: Called when data changes on the server
//-----------------------------------------------------------------------------
void C_ASW_Emitter::OnDataChanged( DataUpdateType_t updateType )
{
	// NOTE: We MUST call the base classes' implementation of this function
	BaseClass::OnDataChanged( updateType );	

	// Setup our entity's particle system on creation
	if ( updateType == DATA_UPDATE_CREATED )
	{
		CreateEmitter();
	}
	m_hEmitter->Update();		
}

void C_ASW_Emitter::CreateEmitter()
{
	// Creat the emitter
	m_hEmitter = CASWGenericEmitter::Create( "asw_emitter" );
	m_hEmitter->SetSortOrigin(GetAbsOrigin());		

	// Obtain a reference handle to our particle's desired material
	if ( m_hEmitter.IsValid() )
	{			
		m_hEmitter->UseTemplate(m_szTemplateName);
		m_hEmitter->SetEmitterScale(m_fScale);
		m_hEmitter->SetActive(m_bEmit);
		m_hEmitter->Update();
	}

	// Call our ClientThink() function once every client frame
	SetNextClientThink( CLIENT_THINK_ALWAYS );

	
}

//-----------------------------------------------------------------------------
// Purpose: Client-side think function for the entity
//-----------------------------------------------------------------------------
void C_ASW_Emitter::ClientThink( void )
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
		if (Q_strlen(m_szAttach) <= 0)
		{
			pos = m_hClientAttach->WorldSpaceCenter();
			ang = m_hClientAttach->GetAbsAngles();
		}
		else
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
	m_hEmitter->SetActive(m_bEmit);
	if (gpGlobals->curtime > m_fDieTime && m_fDieTime != 0)
	{
		Die();
	}
}


void C_ASW_Emitter::UseTemplate(const char* pTemplateName, bool bLoadFromCache)
{
	if (m_hEmitter == NULL)
		return;

	strcpy(m_szTemplateName, pTemplateName);
	m_hEmitter->UseTemplate(pTemplateName,true,bLoadFromCache);
}

void C_ASW_Emitter::SaveAsTemplate(const char* pTemplateName)
{
	if (m_hEmitter == NULL)
		return;

	strcpy(m_szTemplateName, pTemplateName);
	m_hEmitter->SaveTemplateAs(pTemplateName);
}

void C_ASW_Emitter::SetDieTime(float fDieTime)
{
	m_fDieTime = fDieTime;
}

void C_ASW_Emitter::Die()
{
	// make sure our emitter knows to die
	if (!(m_hEmitter == NULL))
	{
		m_hEmitter->SetDieTime(m_fDieTime);
	}
	Release();
}

void C_ASW_Emitter::ClientAttach(C_BaseAnimating *pParent, const char *szAttach)
{
	if (!pParent)
		return;
	strcpy(m_szAttach, szAttach);
	m_hClientAttach = pParent;
}