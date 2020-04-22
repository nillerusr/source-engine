//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "grenadetrail.h"
#include "dt_send.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define GRENADETRAIL_ENTITYNAME		"env_grenadetrail"

//-----------------------------------------------------------------------------
//Data table
//-----------------------------------------------------------------------------
IMPLEMENT_SERVERCLASS_ST(CGrenadeTrail, DT_GrenadeTrail)
	SendPropFloat(SENDINFO(m_SpawnRate), 8, 0, 1, 1024),
	SendPropFloat(SENDINFO(m_ParticleLifetime), 16, SPROP_ROUNDUP, 0.1, 100),
	SendPropFloat(SENDINFO(m_StopEmitTime), 0, SPROP_NOSCALE),
	SendPropBool(SENDINFO(m_bEmit) ),
	SendPropInt(SENDINFO(m_nAttachment), 32 ),	
END_SEND_TABLE()


BEGIN_DATADESC( CGrenadeTrail )

	DEFINE_KEYFIELD( m_SpawnRate, FIELD_FLOAT, "spawnrate" ),
	DEFINE_KEYFIELD( m_ParticleLifetime, FIELD_FLOAT, "lifetime" ),
	DEFINE_FIELD( m_StopEmitTime, FIELD_TIME ),
	DEFINE_FIELD( m_bEmit, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_nAttachment, FIELD_INTEGER ),

END_DATADESC()


LINK_ENTITY_TO_CLASS(env_grenadetrail, CGrenadeTrail);


//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
CGrenadeTrail::CGrenadeTrail()
{
	m_SpawnRate = 10;
	m_ParticleLifetime = 5;
	m_StopEmitTime = 0; // Don't stop emitting particles
	m_bEmit = true;
	m_nAttachment	= 0;
}

//-----------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//-----------------------------------------------------------------------------
void CGrenadeTrail::SetEmit(bool bVal)
{
	m_bEmit = bVal;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : CGrenadeTrail*
//-----------------------------------------------------------------------------
CGrenadeTrail* CGrenadeTrail::CreateGrenadeTrail()
{
	CBaseEntity *pEnt = CreateEntityByName(GRENADETRAIL_ENTITYNAME);
	if(pEnt)
	{
		CGrenadeTrail *pTrail = dynamic_cast<CGrenadeTrail*>(pEnt);
		if(pTrail)
		{
			pTrail->Activate();
			return pTrail;
		}
		else
		{
			UTIL_Remove(pEnt);
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Attach the smoke trail to an entity or point 
// Input  : index - entity that has the attachment
//			attachment - point to attach to
//-----------------------------------------------------------------------------
void CGrenadeTrail::FollowEntity( CBaseEntity *pEntity, const char *pAttachmentName )
{
	// For attachments
	if ( pAttachmentName && pEntity && pEntity->GetBaseAnimating() )
	{
		m_nAttachment = pEntity->GetBaseAnimating()->LookupAttachment( pAttachmentName );
	}
	else
	{
		m_nAttachment = 0;
	}

	BaseClass::FollowEntity( pEntity );
}
