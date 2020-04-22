//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "entity_burn_effect.h"



IMPLEMENT_SERVERCLASS_ST_NOBASE( CEntityBurnEffect, DT_EntityBurnEffect )
	SendPropEHandle( SENDINFO( m_hBurningEntity ) )
END_SEND_TABLE()


LINK_ENTITY_TO_CLASS( entity_burn_effect, CEntityBurnEffect );


CEntityBurnEffect* CEntityBurnEffect::Create( CBaseEntity *pBurningEntity )
{
	CEntityBurnEffect *pEffect = static_cast<CEntityBurnEffect*>(CreateEntityByName( "entity_burn_effect" ));
	if ( pEffect )
	{
		pEffect->m_hBurningEntity = pBurningEntity;
		return pEffect;
	}
	else
	{
		return NULL;
	}
}

int CEntityBurnEffect::UpdateTransmitState()
{
	return SetTransmitState( FL_EDICT_FULLCHECK );
}


int CEntityBurnEffect::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	CBaseEntity *pEnt = m_hBurningEntity;
	if ( pEnt )
		return pEnt->ShouldTransmit( pInfo );
	else
		return FL_EDICT_DONTSEND;
}



