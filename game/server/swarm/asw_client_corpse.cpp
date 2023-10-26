#include "cbase.h"
#include "asw_client_corpse.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_SERVERCLASS_ST(CASW_Client_Corpse, DT_ASW_Client_Corpse)
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Client_Corpse )

DEFINE_KEYFIELD( m_szRagdollSequence, FIELD_STRING, "ragdollsequence" ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( asw_client_corpse, CASW_Client_Corpse );

void CASW_Client_Corpse::Spawn( void )
{
	Precache();
	SetModel(STRING(GetModelName()));
	int iSeq = LookupSequence(STRING(m_szRagdollSequence));
	if (iSeq != -1)
	{
		ResetSequence(iSeq);
		StudioFrameAdvance();
	}
	BaseClass::Spawn(STRING(GetModelName()), GetAbsOrigin(), Vector(0, 0, 0), 0.0f);
}

void CASW_Client_Corpse::Precache()
{
	PrecacheModel( STRING( GetModelName() ) );
	BaseClass::Precache();
}

// always send, so all clients receive the ragdoll on map load
// argh - players who join after map load aren't going to get these?
int CASW_Client_Corpse::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	return FL_EDICT_ALWAYS;
}

bool CASW_Client_Corpse::BecomeRagdollOnClient( const Vector &force )
{
	// If this character has a ragdoll animation, turn it over to the physics system
	if ( CanBecomeRagdoll() ) 
	{
		VPhysicsDestroyObject();
		AddSolidFlags( FSOLID_NOT_SOLID );
		m_bClientSideRagdoll = true;

		// Have to do this dance because m_vecForce is a network vector
		// and can't be sent to ClampRagdollForce as a Vector *
		Vector vecClampedForce;
		ClampRagdollForce( force, &vecClampedForce );
		m_vecForce = vecClampedForce;

		SetParent( NULL );

		AddFlag( FL_TRANSRAGDOLL );

		SetMoveType( MOVETYPE_NONE );
		//UTIL_SetSize( this, vec3_origin, vec3_origin );
		SetThink( NULL );

		// asw - don't remove, so new clients connecting get the ragdoll too
		//SetNextThink( gpGlobals->curtime + 2.0f );
		//If we're here, then we can vanish safely
		//SetThink( &CBaseEntity::SUB_Remove );

		return true;
	}
	return false;
}