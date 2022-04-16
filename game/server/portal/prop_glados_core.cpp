//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Core of the GlaDOS computer.
//
//=====================================================================================//

#include "cbase.h"
#include "baseentity.h"
#include "te_effect_dispatch.h"	// Sprite effect
#include "props.h"				// CPhysicsProp base class
#include "saverestore_utlvector.h"

#define GLADOS_CORE_MODEL_NAME "models/props_bts/glados_ball_reference.mdl" 

static const char *s_pAnimateThinkContext = "Animate";

#define DEFAULT_LOOK_ANINAME			"look_01"
#define CURIOUS_LOOK_ANINAME			"look_02"
#define AGGRESSIVE_LOOK_ANINAME			"look_03"
#define CRAZY_LOOK_ANINAME				"look_04"

#define DEFAULT_SKIN					0
#define CURIOUS_SKIN					1
#define AGGRESSIVE_SKIN					2
#define	CRAZY_SKIN						3

class CPropGladosCore : public CPhysicsProp
{
public:
	DECLARE_CLASS( CPropGladosCore, CPhysicsProp );
	DECLARE_DATADESC();

	CPropGladosCore();
	~CPropGladosCore();

	typedef enum 
	{
		CORETYPE_CURIOUS,
		CORETYPE_AGGRESSIVE,
		CORETYPE_CRAZY,
		CORETYPE_NONE,
		CORETYPE_TOTAL,

	} CORETYPE;

	virtual void Spawn( void );
	virtual void Precache( void );

	virtual QAngle	PreferredCarryAngles( void ) { return QAngle( 180, -90, 180 ); }
	virtual bool	HasPreferredCarryAnglesForPlayer( CBasePlayer *pPlayer ) { return true; }

	void	InputPanic( inputdata_t &inputdata );
	void	InputStartTalking( inputdata_t &inputdata );

	void	StartPanic ( void );
	void	StartTalking ( float flDelay );

	void	TalkingThink ( void );
	void	PanicThink ( void );
	void	AnimateThink ( void );

	void	SetupVOList ( void );
	
	void	OnPhysGunPickup( CBasePlayer* pPhysGunUser, PhysGunPickup_t reason );

private:
	int m_iTotalLines;
	int m_iEyeballAttachment;
	float m_flBetweenVOPadding;		// Spacing (in seconds) between VOs
	bool m_bFirstPickup;

	// Names of sound scripts for this core's personality
	CUtlVector<string_t> m_speechEvents;
	int m_iSpeechIter;

	string_t	m_iszPanicSoundScriptName;
	string_t	m_iszDeathSoundScriptName;
	string_t	m_iszLookAnimationName;		// Different animations for each personality

	CORETYPE m_iCoreType;
};

LINK_ENTITY_TO_CLASS( prop_glados_core, CPropGladosCore );

//-----------------------------------------------------------------------------
// Save/load 
//-----------------------------------------------------------------------------
BEGIN_DATADESC( CPropGladosCore )

	DEFINE_FIELD( m_iEyeballAttachment,						FIELD_INTEGER ),
	DEFINE_FIELD( m_iTotalLines,							FIELD_INTEGER ),
	DEFINE_FIELD( m_iSpeechIter,							FIELD_INTEGER ),
	DEFINE_FIELD( m_iszDeathSoundScriptName,				FIELD_STRING ),
	DEFINE_FIELD( m_iszPanicSoundScriptName,				FIELD_STRING ),
	DEFINE_FIELD( m_iszLookAnimationName,					FIELD_STRING ),
	DEFINE_UTLVECTOR( m_speechEvents,						FIELD_STRING ),
	DEFINE_FIELD( m_bFirstPickup,							FIELD_BOOLEAN ),

	DEFINE_KEYFIELD( m_iCoreType,			FIELD_INTEGER, "CoreType" ),
	DEFINE_KEYFIELD( m_flBetweenVOPadding,  FIELD_FLOAT, "DelayBetweenLines" ),

	DEFINE_INPUTFUNC( FIELD_VOID,			"Panic", InputPanic ),
	DEFINE_INPUTFUNC( FIELD_VOID,			"StartTalking", InputStartTalking ),

	DEFINE_THINKFUNC( TalkingThink ),
	DEFINE_THINKFUNC( PanicThink ),
	DEFINE_THINKFUNC( AnimateThink ),
	
END_DATADESC()

CPropGladosCore::CPropGladosCore()
{
	m_iTotalLines = m_iSpeechIter = 0;
	m_iszLookAnimationName = m_iszPanicSoundScriptName = m_iszDeathSoundScriptName = NULL_STRING;
	m_flBetweenVOPadding = 2.5f;
	m_bFirstPickup = true;
}

CPropGladosCore::~CPropGladosCore()
{
	m_speechEvents.Purge();
}

void CPropGladosCore::Spawn( void )
{
	SetupVOList();

	Precache();
	KeyValue( "model", GLADOS_CORE_MODEL_NAME );
	BaseClass::Spawn();

	//Default to 'dropped' animation
	ResetSequence(LookupSequence("drop"));
	SetCycle( 1.0f );

	DisableAutoFade();
	m_iEyeballAttachment = LookupAttachment( "eyeball" );

	SetContextThink( &CPropGladosCore::AnimateThink, gpGlobals->curtime + 0.1f, s_pAnimateThinkContext );
}

void CPropGladosCore::Precache( void )
{
	BaseClass::Precache();

	// Personality VOs -- Curiosity
	PrecacheScriptSound ( "Portal.Glados_core.Curiosity_1" );
	PrecacheScriptSound ( "Portal.Glados_core.Curiosity_2" );
	PrecacheScriptSound ( "Portal.Glados_core.Curiosity_3" );
	PrecacheScriptSound ( "Portal.Glados_core.Curiosity_4" );
	PrecacheScriptSound ( "Portal.Glados_core.Curiosity_5" );
	PrecacheScriptSound ( "Portal.Glados_core.Curiosity_6" );
	PrecacheScriptSound ( "Portal.Glados_core.Curiosity_7" );
	PrecacheScriptSound ( "Portal.Glados_core.Curiosity_8" );
	PrecacheScriptSound ( "Portal.Glados_core.Curiosity_9" );
	PrecacheScriptSound ( "Portal.Glados_core.Curiosity_10" );
	PrecacheScriptSound ( "Portal.Glados_core.Curiosity_11" );
	PrecacheScriptSound ( "Portal.Glados_core.Curiosity_12" );
	PrecacheScriptSound ( "Portal.Glados_core.Curiosity_13" );
	PrecacheScriptSound ( "Portal.Glados_core.Curiosity_15" );
	PrecacheScriptSound ( "Portal.Glados_core.Curiosity_16" );
	PrecacheScriptSound ( "Portal.Glados_core.Curiosity_17" );
	PrecacheScriptSound ( "Portal.Glados_core.Curiosity_18" );

	// Aggressive
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_00" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_01" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_02" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_03" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_04" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_05" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_06" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_07" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_08" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_09" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_10" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_11" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_12" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_13" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_14" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_15" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_16" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_17" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_18" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_19" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_20" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_21" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_panic_01" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_panic_02" );

	// Crazy
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_01" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_02" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_03" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_04" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_05" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_06" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_07" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_08" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_09" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_10" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_11" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_12" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_13" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_14" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_15" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_16" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_17" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_18" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_19" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_20" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_21" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_22" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_23" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_24" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_25" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_26" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_27" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_28" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_29" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_30" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_31" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_32" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_33" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_34" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_35" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_36" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_37" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_38" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_39" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_40" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_41" );

	PrecacheModel( GLADOS_CORE_MODEL_NAME );
}

//-----------------------------------------------------------------------------
// Purpose: Switch to panic think, play panic vo and animations
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CPropGladosCore::InputPanic( inputdata_t &inputdata )
{
	StartPanic();
}

void CPropGladosCore::StartPanic( void )
{
	ResetSequence( LookupSequence( STRING(m_iszLookAnimationName) ) );
	SetThink( &CPropGladosCore::PanicThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

//-----------------------------------------------------------------------------
// Purpose: Play panic vo and animations, then return to talking
// Output :
//-----------------------------------------------------------------------------
void CPropGladosCore::PanicThink ( void )
{
	if ( m_speechEvents.Count() <= 0 || !m_speechEvents.IsValidIndex( m_iSpeechIter ) || m_iszPanicSoundScriptName == NULL_STRING )
	{
		SetThink ( NULL );
		SetNextThink( gpGlobals->curtime );
		return;
	}

	StopSound( m_speechEvents[m_iSpeechIter].ToCStr() );
	EmitSound( m_iszPanicSoundScriptName.ToCStr() );
	float flCurDuration = GetSoundDuration(  m_iszPanicSoundScriptName.ToCStr(), GLADOS_CORE_MODEL_NAME );

	SetThink( &CPropGladosCore::TalkingThink );
	SetNextThink( gpGlobals->curtime + m_flBetweenVOPadding + flCurDuration );
}

//-----------------------------------------------------------------------------
// Purpose: Start playing personality VO list
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CPropGladosCore::InputStartTalking ( inputdata_t &inputdata )
{
	StartTalking( 0.0f );
}

void CPropGladosCore::StartTalking( float flDelay )
{
	if ( m_speechEvents.IsValidIndex( m_iSpeechIter ) &&  m_speechEvents.Count() > 0 )
	{
		StopSound( m_speechEvents[m_iSpeechIter].ToCStr() );
	}

	m_iSpeechIter = 0;
	SetThink( &CPropGladosCore::TalkingThink );
	SetNextThink( gpGlobals->curtime + m_flBetweenVOPadding + flDelay );
}

//-----------------------------------------------------------------------------
// Purpose: Start playing personality VO list
//-----------------------------------------------------------------------------
void CPropGladosCore::TalkingThink( void )
{
	if ( m_speechEvents.Count() <= 0 || !m_speechEvents.IsValidIndex( m_iSpeechIter ) )
	{
		SetThink ( NULL );
		SetNextThink( gpGlobals->curtime );
		return;
	}

	// Loop the 'look around' animation after the first line.
	int iCurSequence = GetSequence();
	int iLookSequence = LookupSequence( STRING(m_iszLookAnimationName) );
	if ( iCurSequence != iLookSequence && m_iSpeechIter > 0 )
	{
		ResetSequence( iLookSequence );
	}

	int iPrevIter = m_iSpeechIter-1;
	if ( iPrevIter < 0 )
		iPrevIter = 0;

	StopSound( m_speechEvents[iPrevIter].ToCStr() );

	float flCurDuration = GetSoundDuration( m_speechEvents[m_iSpeechIter].ToCStr(), GLADOS_CORE_MODEL_NAME );

	EmitSound( m_speechEvents[m_iSpeechIter].ToCStr() );
	SetNextThink( gpGlobals->curtime + m_flBetweenVOPadding + flCurDuration );

	// wrap if we hit the end of the list
	m_iSpeechIter = (m_iSpeechIter+1)%m_speechEvents.Count();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CPropGladosCore::AnimateThink()
{
	StudioFrameAdvance();
	SetContextThink( &CPropGladosCore::AnimateThink, gpGlobals->curtime + 0.1f, s_pAnimateThinkContext );
}

//-----------------------------------------------------------------------------
// Purpose: Setup list of lines based on core personality
//-----------------------------------------------------------------------------
void CPropGladosCore::SetupVOList( void )
{
	m_speechEvents.RemoveAll();

	switch ( m_iCoreType )
	{
	case CORETYPE_CURIOUS:
		{
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Curiosity_1" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Curiosity_2" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Curiosity_3" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Curiosity_4" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Curiosity_5" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Curiosity_6" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Curiosity_7" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Curiosity_8" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Curiosity_9" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Curiosity_10" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Curiosity_11" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Curiosity_12" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Curiosity_13" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Curiosity_16" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Curiosity_17" ) );
			m_iszPanicSoundScriptName =  AllocPooledString( "Portal.Glados_core.Curiosity_15" );
			m_iszLookAnimationName = AllocPooledString( CURIOUS_LOOK_ANINAME );
			m_nSkin = CURIOUS_SKIN;
			
		}
		break;
	case CORETYPE_AGGRESSIVE:
		{
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_01" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_02" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_03" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_04" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_05" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_06" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_07" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_08" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_09" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_10" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_11" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_12" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_13" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_14" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_15" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_16" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_17" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_18" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_19" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_20" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_21" ) );
			m_iszPanicSoundScriptName = AllocPooledString( "Portal.Glados_core.Aggressive_panic_01" );
			m_iszLookAnimationName = AllocPooledString( AGGRESSIVE_LOOK_ANINAME );
			m_nSkin = AGGRESSIVE_SKIN;
		}
		break;
	case CORETYPE_CRAZY:
		{
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_01" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_02" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_03" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_04" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_05" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_06" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_07" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_08" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_09" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_10" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_11" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_12" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_13" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_14" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_15" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_16" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_17" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_18" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_19" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_20" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_21" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_22" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_23" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_24" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_25" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_26" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_27" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_28" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_29" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_30" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_31" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_32" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_33" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_34" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_35" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_36" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_37" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_38" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_39" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_40" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_41" ) );
			m_iszLookAnimationName = AllocPooledString( CRAZY_LOOK_ANINAME );
			m_nSkin = CRAZY_SKIN;
		}
		break;
	default:
		{
			m_iszLookAnimationName = AllocPooledString( DEFAULT_LOOK_ANINAME );
			m_nSkin = DEFAULT_SKIN;
		}
		break;
	};

	m_iszDeathSoundScriptName =  AllocPooledString( "Portal.Glados_core.Death" );
	m_iTotalLines = m_speechEvents.Count();
	m_iSpeechIter = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Cores play a special animation when picked up and dropped
// Input  : pPhysGunUser - player picking up object
//			reason - type of pickup
//-----------------------------------------------------------------------------
void CPropGladosCore::OnPhysGunPickup( CBasePlayer* pPhysGunUser, PhysGunPickup_t reason )
{
	if ( m_bFirstPickup )
	{
		float flTalkingDelay = (CORETYPE_CURIOUS == m_iCoreType) ? (2.0f) : (0.0f);
		StartTalking ( flTalkingDelay );
	}

	m_bFirstPickup = false;
	ResetSequence(LookupSequence("turn"));

	// +use always enables motion on these props
	EnableMotion();

	BaseClass::OnPhysGunPickup ( pPhysGunUser, reason );
}