//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client side C_CHostage class
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_cs_hostage.h"
#include <bitbuf.h>
#include "ragdoll_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#undef CHostage

//-----------------------------------------------------------------------------

static float HOSTAGE_HEAD_TURN_RATE = 130;


CUtlVector< C_CHostage* > g_Hostages;
CUtlVector< EHANDLE > g_HostageRagdolls;

extern ConVar g_ragdoll_fadespeed;

//-----------------------------------------------------------------------------
const int NumInterestingPoseParameters = 6;
static const char* InterestingPoseParameters[NumInterestingPoseParameters] =
{
	"body_yaw",
	"spine_yaw",
	"neck_trans",
	"head_pitch",
	"head_yaw",
	"head_roll"
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
class C_LowViolenceHostageDeathModel : public C_BaseAnimating
{
public:
	DECLARE_CLASS( C_LowViolenceHostageDeathModel, C_BaseAnimating );
	
	C_LowViolenceHostageDeathModel();
	~C_LowViolenceHostageDeathModel();

	bool SetupLowViolenceModel( C_CHostage *pHostage );

	// fading out
	void ClientThink( void );

private:

	void Interp_Copy( VarMapping_t *pDest, CBaseEntity *pSourceEntity, VarMapping_t *pSrc );

	float m_flFadeOutStart;
};

//-----------------------------------------------------------------------------
C_LowViolenceHostageDeathModel::C_LowViolenceHostageDeathModel()
{
}

//-----------------------------------------------------------------------------
C_LowViolenceHostageDeathModel::~C_LowViolenceHostageDeathModel()
{
}

//-----------------------------------------------------------------------------
void C_LowViolenceHostageDeathModel::Interp_Copy( VarMapping_t *pDest, CBaseEntity *pSourceEntity, VarMapping_t *pSrc )
{
	if ( !pDest || !pSrc )
		return;

	if ( pDest->m_Entries.Count() != pSrc->m_Entries.Count() )
	{
		Assert( false );
		return;
	}

	int c = pDest->m_Entries.Count();
	for ( int i = 0; i < c; i++ )
	{
		pDest->m_Entries[ i ].watcher->Copy( pSrc->m_Entries[i].watcher );
	}
}

//-----------------------------------------------------------------------------
bool C_LowViolenceHostageDeathModel::SetupLowViolenceModel( C_CHostage *pHostage )
{
	const model_t *model = pHostage->GetModel();
	const char *pModelName = modelinfo->GetModelName( model );
	if ( InitializeAsClientEntity( pModelName, RENDER_GROUP_OPAQUE_ENTITY ) == false )
	{
		Release();
		return false;
	}

	// Play the low-violence death anim
	if ( LookupSequence( "death1" ) == -1 )
	{
		Release();
		return false;
	}

	m_flFadeOutStart = gpGlobals->curtime + 5.0f;
	SetNextClientThink( CLIENT_THINK_ALWAYS );

	SetSequence( LookupSequence( "death1" ) );
	ForceClientSideAnimationOn();

	if ( pHostage && !pHostage->IsDormant() )
	{
		SetNetworkOrigin( pHostage->GetAbsOrigin() );
		SetAbsOrigin( pHostage->GetAbsOrigin() );
		SetAbsVelocity( pHostage->GetAbsVelocity() );

		// move my current model instance to the ragdoll's so decals are preserved.
		pHostage->SnatchModelInstance( this );

		SetAbsAngles( pHostage->GetRenderAngles() );
		SetNetworkAngles( pHostage->GetRenderAngles() );

		CStudioHdr *pStudioHdr = GetModelPtr();

		// update pose parameters
		float poseParameter[MAXSTUDIOPOSEPARAM];
		GetPoseParameters( pStudioHdr, poseParameter );
		for ( int i=0; i<NumInterestingPoseParameters; ++i )
		{
			int poseParameterIndex = LookupPoseParameter( pStudioHdr, InterestingPoseParameters[i] );
			SetPoseParameter( pStudioHdr, poseParameterIndex, poseParameter[poseParameterIndex] );
		}
	}

	Interp_Reset( GetVarMapping() );
	return true;
}

//-----------------------------------------------------------------------------
void C_LowViolenceHostageDeathModel::ClientThink( void )
{
	if ( m_flFadeOutStart > gpGlobals->curtime )
	{
		 return;
	}

	int iAlpha = GetRenderColor().a;

	iAlpha = MAX( iAlpha - ( g_ragdoll_fadespeed.GetInt() * gpGlobals->frametime ), 0 );

	SetRenderMode( kRenderTransAlpha );
	SetRenderColorA( iAlpha );

	if ( iAlpha == 0 )
	{
		Release();
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void C_CHostage::RecvProxy_Rescued( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_CHostage *pHostage= (C_CHostage *) pStruct;
	
	bool isRescued = pData->m_Value.m_Int != 0;

	if ( isRescued && !pHostage->m_isRescued )
	{
		// hostage was rescued
		pHostage->m_flDeadOrRescuedTime = gpGlobals->curtime + 2;
		pHostage->SetRenderMode( kRenderGlow );
		pHostage->SetNextClientThink( gpGlobals->curtime );
	}

	pHostage->m_isRescued = isRescued;
}

//-----------------------------------------------------------------------------
IMPLEMENT_CLIENTCLASS_DT(C_CHostage, DT_CHostage, CHostage)
	
	RecvPropInt( RECVINFO( m_isRescued ), 0, C_CHostage::RecvProxy_Rescued ),
	RecvPropInt( RECVINFO( m_iHealth ) ),
	RecvPropInt( RECVINFO( m_iMaxHealth ) ),
	RecvPropInt( RECVINFO( m_lifeState ) ),
	
	RecvPropEHandle( RECVINFO( m_leader ) ),

END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_CHostage::C_CHostage()
{
	g_Hostages.AddToTail( this );

	m_flDeadOrRescuedTime = 0.0;
	m_flLastBodyYaw = 0;
	m_createdLowViolenceRagdoll = false;
	
	// TODO: Get IK working on the steep slopes CS has, then enable it on characters.
	m_EntClientFlags |= ENTCLIENTFLAG_DONTUSEIK;

	// set the model so the PlayerAnimState uses the Hostage activities/sequences
	SetModelName( "models/Characters/Hostage_01.mdl" );

	m_PlayerAnimState = CreateHostageAnimState( this, this, LEGANIM_8WAY, false );
	
	m_leader = NULL;
	m_blinkTimer.Invalidate();
	m_seq = -1;

	m_flCurrentHeadPitch = 0;
	m_flCurrentHeadYaw = 0;

	m_eyeAttachment = -1;
	m_chestAttachment = -1;
	m_headYawPoseParam = -1;
	m_headPitchPoseParam = -1;
	m_lookAt = Vector( 0, 0, 0 );
	m_isInit = false;
	m_lookAroundTimer.Invalidate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_CHostage::~C_CHostage()
{
	g_Hostages.FindAndRemove( this );
	m_PlayerAnimState->Release();
}

//-----------------------------------------------------------------------------
void C_CHostage::Spawn( void )
{
	m_leader = NULL;
	m_blinkTimer.Invalidate();
}

//-----------------------------------------------------------------------------
bool C_CHostage::ShouldDraw( void )
{
	if ( m_createdLowViolenceRagdoll )
		return false;

	return BaseClass::ShouldDraw();
}

//-----------------------------------------------------------------------------
C_BaseAnimating * C_CHostage::BecomeRagdollOnClient()
{
	if ( g_RagdollLVManager.IsLowViolence() )
	{
		// We can't just play the low-violence anim ourselves, since we're about to be deleted by the server.
		// So, let's create another entity that can play the anim and stick around.
		C_LowViolenceHostageDeathModel *pLowViolenceModel = new C_LowViolenceHostageDeathModel();
		m_createdLowViolenceRagdoll = pLowViolenceModel->SetupLowViolenceModel( this );
		if ( m_createdLowViolenceRagdoll )
		{
			UpdateVisibility();
			g_HostageRagdolls.AddToTail( pLowViolenceModel );
			return pLowViolenceModel;
		}
		else
		{
			// if we don't have a low-violence death anim, don't create a ragdoll.
			return NULL;
		}
	}

	C_BaseAnimating *pRagdoll = BaseClass::BecomeRagdollOnClient();
	if ( pRagdoll && pRagdoll != this )
	{
		g_HostageRagdolls.AddToTail( pRagdoll );
	}
	return pRagdoll;
}

//-----------------------------------------------------------------------------
/** 
 * Set up attachment and pose param indices.
 * We can't do this in the constructor or Spawn() because the data isn't 
 * there yet.
 */
void C_CHostage::Initialize( )
{
	m_eyeAttachment = LookupAttachment( "eyes" );
	m_chestAttachment = LookupAttachment( "chest" );

	m_headYawPoseParam = LookupPoseParameter( "head_yaw" );
	GetPoseParameterRange( m_headYawPoseParam, m_headYawMin, m_headYawMax );

	m_headPitchPoseParam = LookupPoseParameter( "head_pitch" );
	GetPoseParameterRange( m_headPitchPoseParam, m_headPitchMin, m_headPitchMax );

	m_bodyYawPoseParam = LookupPoseParameter( "body_yaw" );
	GetPoseParameterRange( m_bodyYawPoseParam, m_bodyYawMin, m_bodyYawMax );

	Vector pos;
	QAngle angles;

	if (!GetAttachment( m_eyeAttachment, pos, angles ))
	{
		m_vecViewOffset = Vector( 0, 0, 50.0f );
	}
	else
	{
		m_vecViewOffset = pos - GetAbsOrigin();
	}


	if (!GetAttachment( m_chestAttachment, pos, angles ))
	{
		m_lookAt = Vector( 0, 0, 0 );
	}
	else
	{
		Vector forward;
		AngleVectors( angles, &forward );
		m_lookAt = EyePosition() + 100.0f * forward;
	}
}

//-----------------------------------------------------------------------------
CWeaponCSBase* C_CHostage::CSAnim_GetActiveWeapon()
{
	return NULL;
}

//-----------------------------------------------------------------------------
bool C_CHostage::CSAnim_CanMove()
{
	return true;
}


//-----------------------------------------------------------------------------
/**
 * Orient head and eyes towards m_lookAt.
 */
void C_CHostage::UpdateLookAt( CStudioHdr *pStudioHdr )
{
	if (!m_isInit)
	{
		m_isInit = true;
		Initialize( );
	}

	// head yaw
	if (m_headYawPoseParam < 0 || m_bodyYawPoseParam < 0 || m_headPitchPoseParam < 0)
		return;

	if (GetLeader())
	{
		m_lookAt = GetLeader()->EyePosition();
	}
	
	// orient eyes
	m_viewtarget = m_lookAt;

	// blinking
	if (m_blinkTimer.IsElapsed())
	{
		m_blinktoggle = !m_blinktoggle;
		m_blinkTimer.Start( RandomFloat( 1.5f, 4.0f ) );
	}

	// Figure out where we want to look in world space.
	QAngle desiredAngles;
	Vector to = m_lookAt - EyePosition();
	VectorAngles( to, desiredAngles );

	// Figure out where our body is facing in world space.
	float poseParams[MAXSTUDIOPOSEPARAM];
	GetPoseParameters( pStudioHdr, poseParams );
	QAngle bodyAngles( 0, 0, 0 );
	bodyAngles[YAW] = GetRenderAngles()[YAW] + RemapVal( poseParams[m_bodyYawPoseParam], 0, 1, m_bodyYawMin, m_bodyYawMax );


	float flBodyYawDiff = bodyAngles[YAW] - m_flLastBodyYaw;
	m_flLastBodyYaw = bodyAngles[YAW];
	

	// Set the head's yaw.
	float desired = AngleNormalize( desiredAngles[YAW] - bodyAngles[YAW] );
	desired = clamp( desired, m_headYawMin, m_headYawMax );
	m_flCurrentHeadYaw = ApproachAngle( desired, m_flCurrentHeadYaw, HOSTAGE_HEAD_TURN_RATE * gpGlobals->frametime );

	// Counterrotate the head from the body rotation so it doesn't rotate past its target.
	m_flCurrentHeadYaw = AngleNormalize( m_flCurrentHeadYaw - flBodyYawDiff );
	desired = clamp( desired, m_headYawMin, m_headYawMax );
	
	SetPoseParameter( pStudioHdr, m_headYawPoseParam, m_flCurrentHeadYaw );

	
	// Set the head's yaw.
	desired = AngleNormalize( desiredAngles[PITCH] );
	desired = clamp( desired, m_headPitchMin, m_headPitchMax );
	
	m_flCurrentHeadPitch = ApproachAngle( desired, m_flCurrentHeadPitch, HOSTAGE_HEAD_TURN_RATE * gpGlobals->frametime );
	m_flCurrentHeadPitch = AngleNormalize( m_flCurrentHeadPitch );
	SetPoseParameter( pStudioHdr, m_headPitchPoseParam, m_flCurrentHeadPitch );

	SetPoseParameter( pStudioHdr, "head_roll", 0.0f );
}


//-----------------------------------------------------------------------------
/**
 * Look around at various interesting things
 */
void C_CHostage::LookAround( void )
{
	if (GetLeader() == NULL && m_lookAroundTimer.IsElapsed())
	{
		m_lookAroundTimer.Start( RandomFloat( 3.0f, 15.0f ) );

		Vector forward;
		QAngle angles = GetAbsAngles();
		angles[ YAW ] += RandomFloat( m_headYawMin, m_headYawMax );
		angles[ PITCH ] += RandomFloat( m_headPitchMin, m_headPitchMax );
		AngleVectors( angles, &forward );
		m_lookAt = EyePosition() + 100.0f * forward;
	}
}

//-----------------------------------------------------------------------------
void C_CHostage::UpdateClientSideAnimation()
{
	if (IsDormant())
	{
		return;
	}

	m_PlayerAnimState->Update( GetAbsAngles()[YAW], GetAbsAngles()[PITCH] );

	// initialize pose parameters
	char *setToZero[] =
	{
		"spine_yaw",
		"head_roll"
	};
	CStudioHdr *pStudioHdr = GetModelPtr();
	for ( int i=0; i < ARRAYSIZE( setToZero ); i++ )
	{
		int index = LookupPoseParameter( pStudioHdr, setToZero[i] );
		if ( index >= 0 )
			SetPoseParameter( pStudioHdr, index, 0 );
	}

	// orient head and eyes
	LookAround();
	UpdateLookAt( pStudioHdr );


	BaseClass::UpdateClientSideAnimation();
}

//-----------------------------------------------------------------------------
void C_CHostage::ClientThink()
{
	C_BaseCombatCharacter::ClientThink();

	int speed = 2;
	int a = m_clrRender->a;

	a = MAX( 0, a - speed );

	SetRenderColorA( a );

	if ( m_clrRender->a > 0 )
	{
		SetNextClientThink( gpGlobals->curtime + 0.001 );
	}
}

//-----------------------------------------------------------------------------
bool C_CHostage::WasRecentlyKilledOrRescued( void )
{
	return ( gpGlobals->curtime < m_flDeadOrRescuedTime );
}

//-----------------------------------------------------------------------------
void C_CHostage::OnPreDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnPreDataChanged( updateType );

	m_OldLifestate = m_lifeState;
}

//-----------------------------------------------------------------------------
void C_CHostage::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( m_OldLifestate != m_lifeState )
	{
		if( m_lifeState == LIFE_DEAD || m_lifeState == LIFE_DYING )
			m_flDeadOrRescuedTime = gpGlobals->curtime + 2;
	}
}

//-----------------------------------------------------------------------------
void C_CHostage::ImpactTrace( trace_t *pTrace, int iDamageType, const char *pCustomImpactName )
{
	static ConVar *violence_hblood = cvar->FindVar( "violence_hblood" );
	if ( violence_hblood && !violence_hblood->GetBool() )
		return;

	BaseClass::ImpactTrace( pTrace, iDamageType, pCustomImpactName );
}

