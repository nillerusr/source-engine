// ai_addon.cpp
#include "cbase.h"
#include "ai_addon.h"
#include "ai_basenpc.h"
#include "ai_behavior_addonhost.h"


//---------------------------------------------------------
// Answers YES if the attachment is on the model and not
// currently plugged by another add-on.
//
// Answers NO if the host's model lacks the attachment or
// the add-on is currently plugged by another add-on.
//
// This is terribly inefficient right now, but it works 
// and lets us move on.
//---------------------------------------------------------
bool IsAddOnAttachmentAvailable( CAI_BaseNPC *pHost, char *pszAttachmentName )
{
	char szOrigialAttachmentName[ 256 ];
	Q_strcpy( szOrigialAttachmentName, pszAttachmentName );

	int iCount = 0;

	// If this loops 20 times, the translate function probably isn't returning the end of list value "" -Jeep
	while ( iCount < 20 )
	{
		// Try another
		Q_strcpy( pszAttachmentName, szOrigialAttachmentName );

		pHost->TranslateAddOnAttachment( pszAttachmentName, iCount );

		if ( pszAttachmentName[ 0 ] == '\0' )
		{
			return false;
		}

		if ( pHost->LookupAttachment(pszAttachmentName) == 0 )
		{
			// Translated to an attachment that doesn't exist
			Msg("***AddOn Error! Host NPC %s does not have attachment %s\n", pHost->GetDebugName(), pszAttachmentName );
			return false;
		}

		int iWishedAttachmentID = pHost->LookupAttachment( pszAttachmentName );

		CAI_BehaviorBase **ppBehaviors = pHost->AccessBehaviors();
		int nBehaviors = pHost->NumBehaviors();

		bool bAttachmentFilled = false;

		for ( int i = 0; i < nBehaviors && !bAttachmentFilled; i++ )
		{
			CAI_AddOnBehaviorBase *pAddOnBehavior = dynamic_cast<CAI_AddOnBehaviorBase *>(ppBehaviors[i]);
			if ( pAddOnBehavior )
			{
				CAI_AddOn **ppAddOns = pAddOnBehavior->GetAddOnsBase();
				int nAddOns = pAddOnBehavior->NumAddOns();
				for ( int j = 0; j < nAddOns && !bAttachmentFilled; j++ )
				{
					bAttachmentFilled = ( ppAddOns[j]->GetAttachmentID() == iWishedAttachmentID );
				}
			}
		}

		if ( !bAttachmentFilled )
		{
			return true;
		}

		++iCount;
	}

	// We should never get here
	DevWarning( "Translating the attachment was tried more than 50 times!\n" );
	return false;
}

//---------------------------------------------------------
//---------------------------------------------------------
int CountAddOns( CAI_BaseNPC *pHost )
{
	int nAddOns = 0;
	CAI_BehaviorBase **ppBehaviors = pHost->AccessBehaviors();
	int nBehaviors = pHost->NumBehaviors();

	for ( int i = 0; i < nBehaviors; i++ )
	{
		CAI_AddOnBehaviorBase *pAddOnBehavior = dynamic_cast<CAI_AddOnBehaviorBase *>(ppBehaviors[i]);
		if ( pAddOnBehavior )
		{
			nAddOns += pAddOnBehavior->NumAddOns();
		}
	}

	return nAddOns;
}

//---------------------------------------------------------
//---------------------------------------------------------
BEGIN_DATADESC( CAI_AddOn )
	DEFINE_FIELD( m_hNPCHost, FIELD_EHANDLE ),
	DEFINE_THINKFUNC( DispatchAddOnThink ),

	DEFINE_FIELD( m_hPhysReplacement, FIELD_EHANDLE ),
	DEFINE_FIELD( m_iPhysReplacementSolidFlags, FIELD_INTEGER ),
	DEFINE_FIELD( m_iPhysReplacementMoveType, FIELD_INTEGER ),
	DEFINE_FIELD( m_angPhysReplacementLocalOrientation, FIELD_VECTOR ),
	DEFINE_FIELD( m_vecPhysReplacementDetatchForce, FIELD_VECTOR ),

	DEFINE_FIELD( m_bWasAttached, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flWaitFinished, FIELD_TIME ),
	DEFINE_FIELD( m_flNextAttachTime, FIELD_FLOAT ),

	DEFINE_INPUTFUNC( FIELD_STRING, "Install", InputInstall ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Remove", InputRemove ),
END_DATADESC()

//---------------------------------------------------------
//---------------------------------------------------------
void CAI_AddOn::Precache()
{
	BaseClass::Precache();
	PrecacheModel( GetAddOnModelName() );
	PrecacheScriptSound( "AddOn.Install" );
}

//---------------------------------------------------------
//---------------------------------------------------------
void CAI_AddOn::Spawn()
{	
	BaseClass::Spawn();
	Precache();

	CBaseEntity *pPhysReplacement = m_hPhysReplacement.Get();

	if ( pPhysReplacement )
	{
		// Use the same model as the replacement
		SetModel( pPhysReplacement->GetModelName().ToCStr() );
	}
	else
	{
		SetModel( GetAddOnModelName() );
	}

	SetCollisionGroup( COLLISION_GROUP_WEAPON );

	VPhysicsInitNormal( SOLID_VPHYSICS, GetSolidFlags() | FSOLID_TRIGGER, false );
	SetMoveType( MOVETYPE_VPHYSICS );

	SetThink( &CAI_AddOn::DispatchAddOnThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
}


//---------------------------------------------------------
//---------------------------------------------------------
void CAI_AddOn::UpdateOnRemove()
{
	Remove();
	BaseClass::UpdateOnRemove();
}

int CAI_AddOn::DrawDebugTextOverlays( void )
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	// Draw debug text for agent
	m_AgentDebugOverlays = m_debugOverlays;

	Vector vecLocalCenter;

	ICollideable *pCollidable = GetCollideable();

	if ( pCollidable )
	{
		VectorAdd( pCollidable->OBBMins(), pCollidable->OBBMaxs(), vecLocalCenter );
		vecLocalCenter *= 0.5f;

		if ( ( pCollidable->GetCollisionAngles() == vec3_angle ) || ( vecLocalCenter == vec3_origin ) )
		{
			VectorAdd( vecLocalCenter, pCollidable->GetCollisionOrigin(), m_vecAgentDebugOverlaysPos );
		}
		else
		{
			VectorTransform( vecLocalCenter, pCollidable->CollisionToWorldTransform(), m_vecAgentDebugOverlaysPos );
		}
	}
	else
	{
		m_vecAgentDebugOverlaysPos = GetAbsOrigin();
	}

	text_offset = static_cast<CAI_Agent*>( this )->DrawDebugTextOverlays( text_offset );

	return text_offset;
}

//---------------------------------------------------------
//---------------------------------------------------------
void CAI_AddOn::GatherConditions()
{
	CAI_Agent::GatherConditions();

	ClearCondition( COND_ADDON_LOST_HOST );

	if( GetNPCHost() )
	{
		m_bWasAttached = true;
	}
	else 
	{
		if( m_bWasAttached == true )
		{
			SetCondition( COND_ADDON_LOST_HOST );

			if ( m_flNextAttachTime != 0.0f && m_flNextAttachTime < gpGlobals->curtime )
			{
				m_flNextAttachTime = 0.0f;
				m_bWasAttached = false;
			}
		}
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
int CAI_AddOn::SelectSchedule( void )
{
	return SCHED_ADDON_NO_OWNER;
}

//---------------------------------------------------------
//---------------------------------------------------------
void CAI_AddOn::StartTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_ADDON_WAIT:
		m_flWaitFinished = gpGlobals->curtime + pTask->flTaskData;
		break;

	case TASK_ADDON_WAIT_RANDOM:
		m_flWaitFinished = gpGlobals->curtime + random->RandomFloat( 0.1f, pTask->flTaskData );
		break;

	default:
		CAI_Agent::StartTask( pTask );
		break;
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
void CAI_AddOn::RunTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_ADDON_WAIT:
	case TASK_ADDON_WAIT_RANDOM:
		if( gpGlobals->curtime > m_flWaitFinished )
			TaskComplete();
		break;

	default:
		CAI_Agent::RunTask( pTask );
		break;
	}
}

void CAI_AddOn::SetPhysReplacement( CBaseEntity *pEntity )
{
	m_hPhysReplacement = pEntity;
}

//---------------------------------------------------------
//---------------------------------------------------------
bool CAI_AddOn::Attach( CAI_BaseNPC *pHost )
{
	// Make sure we're not already attached to someone else!
	Assert( GetAttachmentID() == INVALID_ADDON_ATTACHMENT_ID );

	char szAttachmentName[ 256 ];
	szAttachmentName[ 0 ] = '\0';

	PickAttachment( pHost, szAttachmentName );

	if ( szAttachmentName[ 0 ] == '\0' )
	{
		return false;
	}

	int iAttachmentIndex = pHost->LookupAttachment( szAttachmentName );
	if ( !iAttachmentIndex )
	{
		return false;
	}

	Vector vecOrigin;
	Vector vecForward, vecRight, vecUp;
	QAngle angles;

	pHost->GetAttachment( iAttachmentIndex, vecOrigin, angles );

	AngleVectors( angles, &vecForward, &vecRight, &vecUp );

	SetAbsOrigin( vecOrigin + GetAttachOffset( angles ) );
	SetAbsAngles( GetAttachOrientation( angles ) );
	m_iAttachmentID = iAttachmentIndex;
	SetParent( pHost, iAttachmentIndex );

	QAngle angLocalAngles = GetLocalOrientation();

	SetLocalAngles( angLocalAngles );

	// Stop acting physical
	IPhysicsObject *pPhysicsObject = VPhysicsGetObject();
	if ( pPhysicsObject )
	{
		pPhysicsObject->EnableMotion( false );
		pPhysicsObject->EnableCollisions( false );
	}

	SetMoveType( MOVETYPE_NONE );

	// Handle the phys replacement
	CBaseEntity *pPhysReplacement = m_hPhysReplacement.Get();
	if ( pPhysReplacement )
	{
		CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
		pPlayer->ForceDropOfCarriedPhysObjects( pPhysReplacement );

		pPhysReplacement->AddEffects( EF_NODRAW );

		m_iPhysReplacementSolidFlags = pPhysReplacement->GetSolidFlags();
		pPhysReplacement->SetSolidFlags( FSOLID_NOT_SOLID );

		m_iPhysReplacementMoveType = pPhysReplacement->GetMoveType();
		pPhysReplacement->SetMoveType( MOVETYPE_NONE );

		pPhysReplacement->SetAbsOrigin( vecOrigin + GetAttachOffset( angles ) );
		pPhysReplacement->SetAbsAngles( GetAttachOrientation( angles ) );
		pPhysReplacement->SetParent( pHost, iAttachmentIndex );
		pPhysReplacement->SetOwnerEntity( pHost );

		m_angPhysReplacementLocalOrientation = pPhysReplacement->GetLocalAngles();
		pPhysReplacement->SetLocalAngles( angLocalAngles );

		IPhysicsObject *pReplacementPhysObject = pPhysReplacement->VPhysicsGetObject();
		if ( pReplacementPhysObject )
		{
			pReplacementPhysObject->EnableMotion( false );
			pReplacementPhysObject->EnableCollisions( false );
			SetMoveType( MOVETYPE_NONE );
		}
	}

	return true;
}

void CAI_AddOn::Dettach( void )
{
	if ( !m_bWasAttached )
		return;

	m_flNextAttachTime = gpGlobals->curtime + 2.0f;

	m_hNPCHost.Set( NULL );
	SetParent( NULL );

	IPhysicsObject *pPhysObject = NULL;

	CBaseEntity *pPhysReplacement = m_hPhysReplacement.Get();
	if ( pPhysReplacement )
	{
		// Make the replacement visible
		pPhysReplacement->RemoveEffects( EF_NODRAW );

		pPhysReplacement->SetSolidFlags( m_iPhysReplacementSolidFlags );
		pPhysReplacement->SetMoveType( MoveType_t( m_iPhysReplacementMoveType ) );

		pPhysObject = pPhysReplacement->VPhysicsGetObject();
		if ( pPhysObject )
		{
			pPhysReplacement->SetMoveType( MOVETYPE_VPHYSICS );	
		}

		pPhysReplacement->SetParent( NULL );
		pPhysReplacement->SetOwnerEntity( NULL );

		pPhysReplacement->SetLocalAngles( m_angPhysReplacementLocalOrientation );

		// Kill ourselves off because we're being replaced
		UTIL_Remove( this );
	}
	else
	{
		pPhysObject = VPhysicsGetObject();
		if ( pPhysObject )
		{
			SetMoveType( MOVETYPE_VPHYSICS );			
		}
	}

	if ( pPhysObject )
	{
		// Start acting physical
		pPhysObject->EnableCollisions( true );
		pPhysObject->EnableMotion( true );
		pPhysObject->EnableGravity( true );
		pPhysObject->SetPosition( GetAbsOrigin(), GetAbsAngles(), true );
		pPhysObject->Wake();

		pPhysObject->AddVelocity( &m_vecPhysReplacementDetatchForce, NULL );
	}
}

//---------------------------------------------------------
// Return true if I successfully attach to the NPC host.
//
// Return false if I am already attached to an NPC, or
// could not be attached to this host.
//---------------------------------------------------------
bool CAI_AddOn::Install( CAI_BaseNPC *pHost, bool bRemoveOnFail )
{
	if( m_bWasAttached )
		return false;

	// Associate the addon with this host
	Assert( m_hNPCHost == NULL ); // For now, prevent slamming from one host to the next.
	m_hNPCHost.Set( pHost );

	// Parent and 
	if ( Attach( pHost ) )
	{
		Bind();
		return true;
	}

	// Failed to attach
	m_hNPCHost = NULL;

	if ( bRemoveOnFail || m_hPhysReplacement.Get() )
	{
		UTIL_Remove( this );
	}

	return false;
}

//---------------------------------------------------------
//---------------------------------------------------------
CAI_BaseNPC *CAI_AddOn::GetNPCHost()
{
	return m_hNPCHost.Get();
}

//---------------------------------------------------------
//---------------------------------------------------------
CBaseEntity *CAI_AddOn::GetHostEnemy()
{
	if( !GetNPCHost() )
		return NULL;

	return GetNPCHost()->GetEnemy();
}

//---------------------------------------------------------
//---------------------------------------------------------
void CAI_AddOn::DispatchAddOnThink()
{
	if( GetNPCHost() != NULL && !GetNPCHost()->IsAlive() )
	{
		EjectFromHost();
		return;
	}

	CAI_Agent::Think(); SetNextThink( gpGlobals->curtime + GetThinkInterval() );
}

QAngle CAI_AddOn::GetLocalOrientation( void )
{
	CBaseEntity *pPhysReplacement = m_hPhysReplacement.Get();

	if ( pPhysReplacement )
	{
		CBaseAnimating *pBaseAnimatingReplacement = dynamic_cast<CBaseAnimating *>( pPhysReplacement );
		if ( pBaseAnimatingReplacement )
		{
			int iMuzzle = pBaseAnimatingReplacement->LookupAttachment( "muzzle" );
			if ( iMuzzle )
			{
				// Use the muzzle angles!
				Vector vecMuzzleOrigin;
				QAngle angMuzzleAngles;
				pBaseAnimatingReplacement->GetAttachmentLocal( iMuzzle, vecMuzzleOrigin, angMuzzleAngles );
				return angMuzzleAngles;
			}
		}

		// Use the local angles
		return pPhysReplacement->GetLocalAngles();
	}

	// No special angles to use
	return QAngle( 0.0f, 0.0f, 0.0f );
}

//---------------------------------------------------------
//---------------------------------------------------------
void CAI_AddOn::EjectFromHost()
{
	Unbind();
	m_hNPCHost.Set( NULL );

	SetThink( NULL );
	SetParent( NULL );

	SetSize( Vector( 0,0,0), Vector(0,0,0) );
	SetMoveType( MOVETYPE_FLYGRAVITY );
	SetMoveCollide( MOVECOLLIDE_FLY_BOUNCE );
	SetSolid( SOLID_BBOX );

	Vector vecDir;
	GetVectors( NULL, NULL, &vecDir );

	SetAbsVelocity( GetAbsVelocity() + vecDir * RandomFloat(50, 200) );
	QAngle avelocity( RandomFloat( 10, 60), RandomFloat( 10, 60), 0 );
	SetLocalAngularVelocity( avelocity );

	SetThink( &CBaseEntity::SUB_FadeOut );
	SetNextThink( gpGlobals->curtime + 1.0f );
}

//---------------------------------------------------------
//---------------------------------------------------------
void CAI_AddOn::InputInstall( inputdata_t &data )
{
	CAI_BaseNPC *pHost = dynamic_cast<CAI_BaseNPC *>( gEntList.FindEntityByName( NULL, data.value.String() ) );

	if( !pHost )
	{
		DevMsg(" AddOn: %s couldn't find Host %s\n", GetDebugName(), data.value.String() );
	}
	else
	{
		Install( pHost );
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
void CAI_AddOn::InputRemove( inputdata_t &data )
{
	Remove();
	m_hNPCHost.Set( NULL );
	SetThink( NULL );
	SetParent( NULL );
	UTIL_Remove( this );
}


AI_BEGIN_AGENT_(CAI_AddOn,CAI_Agent)
	DECLARE_TASK( TASK_ADDON_WAIT )
	DECLARE_TASK( TASK_ADDON_WAIT_RANDOM )

	DECLARE_CONDITION( COND_ADDON_LOST_HOST )

	DEFINE_SCHEDULE
	(
		SCHED_ADDON_NO_OWNER,
		"	Tasks"
		"	TASK_ADDON_WAIT		1"
		"	"
		"	Interrupts"
		"	"
	)
AI_END_AGENT()
