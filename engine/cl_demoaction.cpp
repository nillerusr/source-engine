//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "client_pch.h"
#include "cl_demoaction.h"
#include "cl_demoactionmanager.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseDemoAction::CBaseDemoAction()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseDemoAction::~CBaseDemoAction()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : DEMOACTION
//-----------------------------------------------------------------------------
DEMOACTION CBaseDemoAction::GetType( void ) const
{
	return m_Type;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : actionType - 
//-----------------------------------------------------------------------------
void CBaseDemoAction::SetType( DEMOACTION actionType )
{
	m_Type = actionType;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : DEMOACTIONTIMINGTYPE
//-----------------------------------------------------------------------------
DEMOACTIONTIMINGTYPE CBaseDemoAction::GetTimingType( void ) const
{
	return m_Timing;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : timingtype - 
//-----------------------------------------------------------------------------
void CBaseDemoAction::SetTimingType( DEMOACTIONTIMINGTYPE timingtype )
{
	m_Timing = timingtype;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : fired - 
//-----------------------------------------------------------------------------
void CBaseDemoAction::SetActionFired( bool fired )
{
	m_bActionFired = fired;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseDemoAction::GetActionFired( void ) const
{
	return m_bActionFired;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseDemoAction::SetFinishedAction( bool finished )
{
	m_bActionFinished = finished;
	if ( finished )
	{
		OnActionFinished();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseDemoAction::HasActionFinished( void ) const
{
	return m_bActionFinished;
}

#include "tier0/memdbgoff.h"

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : sz - 
// Output : void *CBaseDemoAction::operator
//-----------------------------------------------------------------------------
void *CBaseDemoAction::operator new( size_t sz )
{
	Assert( sz != 0 );
	return calloc( 1, sz );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pMem - 
// Output : void CBaseDemoAction::operator
//-----------------------------------------------------------------------------
void CBaseDemoAction::operator delete( void *pMem )
{
#if defined( WIN32 ) && defined( _DEBUG )
	// set the memory to a known value
	int size = _msize( pMem );
	Q_memset( pMem, 0xcd, size );
#endif

	// get the engine to free the memory
	free( pMem );
}

#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
struct DemoActionDictionary
{
	DEMOACTION				actiontype;;
	char const				*name;
	DEMOACTIONFACTORY_FUNC	func;
	DEMOACTIONEDIT_FUNC		editfunc;
};

static DemoActionDictionary g_rgDemoTypeNames[ NUM_DEMO_ACTIONS ] =
{
	{ DEMO_ACTION_UNKNOWN					, "Unknown" },
	{ DEMO_ACTION_SKIPAHEAD					, "SkipAhead" },
	{ DEMO_ACTION_STOPPLAYBACK				, "StopPlayback" },
	{ DEMO_ACTION_PLAYCOMMANDS				, "PlayCommands" },
	{ DEMO_ACTION_SCREENFADE_START			, "ScreenFadeStart" },
	{ DEMO_ACTION_SCREENFADE_STOP			, "ScreenFadeStop" },
	{ DEMO_ACTION_TEXTMESSAGE_START			, "TextMessageStart" },
	{ DEMO_ACTION_TEXTMESSAGE_STOP			, "TextMessageStop" },
	{ DEMO_ACTION_PLAYCDTRACK_START			, "PlayCDTrackStart" },
	{ DEMO_ACTION_PLAYCDTRACK_STOP			, "PlayCDTrackStop" },
	{ DEMO_ACTION_PLAYSOUND_START			, "PlaySoundStart" },
	{ DEMO_ACTION_PLAYSOUND_END				, "PlaySoundStop" },

	{ DEMO_ACTION_ONSKIPPEDAHEAD			, "OnSkippedAhead" },
	{ DEMO_ACTION_ONSTOPPEDPLAYBACK			, "OnStoppedPlayback" },
	{ DEMO_ACTION_ONSCREENFADE_FINISHED		, "OnScreenFadeFinished" },
	{ DEMO_ACTION_ONTEXTMESSAGE_FINISHED	, "OnTextMessageFinished" },
	{ DEMO_ACTION_ONPLAYCDTRACK_FINISHED	, "OnPlayCDTrackFinished" },
	{ DEMO_ACTION_ONPLAYSOUND_FINISHED		, "OnPlaySoundFinished" },

	{ DEMO_ACTION_PAUSE						, "Pause" },
	{ DEMO_ACTION_CHANGEPLAYBACKRATE		, "ChangePlaybackRate" },

	{ DEMO_ACTION_ZOOM						, "Zoom FOV" },
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : actionType - 
//			func - 
//-----------------------------------------------------------------------------
void CBaseDemoAction::AddFactory( DEMOACTION actionType, DEMOACTIONFACTORY_FUNC func )
{
	int idx = (int)actionType;
	if ( idx < 0 || idx >= NUM_DEMO_ACTIONS )
	{
		Sys_Error( "CBaseDemoAction::AddFactory: Bogus factory type %i\n", idx );
		return;
	}

	g_rgDemoTypeNames[ idx ].func = func;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : actionType - 
//-----------------------------------------------------------------------------
CBaseDemoAction	*CBaseDemoAction::CreateDemoAction( DEMOACTION actionType )
{
	int idx = (int)actionType;
	if ( idx < 0 || idx >= NUM_DEMO_ACTIONS )
	{
		Sys_Error( "CBaseDemoAction::AddFactory: Bogus factory type %i\n", idx );
		return NULL;
	}

	DEMOACTIONFACTORY_FUNC pfn = g_rgDemoTypeNames[ idx ].func;
	if ( !pfn )
	{
		ConMsg( "CBaseDemoAction::CreateDemoAction:  Missing factory for %s\n",
			NameForType( actionType ) );
		return NULL;
	}

	return (*pfn)();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : actionType - 
//			func - 
//-----------------------------------------------------------------------------
void CBaseDemoAction::AddEditorFactory( DEMOACTION actionType, DEMOACTIONEDIT_FUNC func )
{
	int idx = (int)actionType;
	if ( idx < 0 || idx >= NUM_DEMO_ACTIONS )
	{
		Sys_Error( "CBaseDemoAction::AddEditorFactory: Bogus factory type %i\n", idx );
		return;
	}

	g_rgDemoTypeNames[ idx ].editfunc = func;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : actionType - 
//			*parent - 
//			*action - 
//			newaction - 
// Output : CBaseActionEditDialog
//-----------------------------------------------------------------------------
CBaseActionEditDialog *CBaseDemoAction::CreateActionEditor( DEMOACTION actionType, CDemoEditorPanel *parent, CBaseDemoAction *action, bool newaction )
{
	int idx = (int)actionType;
	if ( idx < 0 || idx >= NUM_DEMO_ACTIONS )
	{
		Sys_Error( "CBaseDemoAction::AddFactory: Bogus factory type %i\n", idx );
		return NULL;
	}

	DEMOACTIONEDIT_FUNC pfn = g_rgDemoTypeNames[ idx ].editfunc;
	if ( !pfn )
	{
		ConMsg( "CBaseDemoAction::CreateActionEditor:  Missing edit factory for %s\n",
			NameForType( actionType ) );
		return NULL;
	}

	return (*pfn)( parent, action, newaction );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : actionType - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseDemoAction::HasEditorFactory( DEMOACTION actionType )
{
	int idx = (int)actionType;
	if ( idx < 0 || idx >= NUM_DEMO_ACTIONS )
	{
		return false;
	}

	DEMOACTIONEDIT_FUNC pfn = g_rgDemoTypeNames[ idx ].editfunc;
	if ( !pfn )
	{
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
struct DemoTimingTagDictionary
{
	DEMOACTIONTIMINGTYPE timingtype;;
	char const			*name;
};

static DemoTimingTagDictionary g_rgDemoTimingTypeNames[ NUM_TIMING_TYPES ] =
{
	{ ACTION_USES_NEITHER			, "TimeDontCare" },
	{ ACTION_USES_TICK				, "TimeUseTick" },
	{ ACTION_USES_TIME				, "TimeUseClock" },
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : DEMOACTION - 
// Output : char const
//-----------------------------------------------------------------------------
char const *CBaseDemoAction::NameForType( DEMOACTION actionType )
{
	int idx = (int)actionType;
	if ( idx < 0 || idx >= NUM_DEMO_ACTIONS )
	{
		ConMsg( "ERROR: CBaseDemoAction::NameForType type %i out of range\n", idx );
		return g_rgDemoTypeNames[ DEMO_ACTION_UNKNOWN ].name;
	}
	
	DemoActionDictionary *entry = &g_rgDemoTypeNames[ idx ];
	Assert( entry->actiontype == actionType );

	return entry->name;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
// Output : DEMOACTION
//-----------------------------------------------------------------------------
DEMOACTION CBaseDemoAction::TypeForName( char const *name )
{
	int c = NUM_DEMO_ACTIONS;
	int i;
	for ( i= 0; i < c; i++ )
	{
		DemoActionDictionary *entry = &g_rgDemoTypeNames[ i ];
		if ( !Q_strcasecmp( entry->name, name ) )
		{
			return entry->actiontype;
		}
	}

	return DEMO_ACTION_UNKNOWN;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : DEMOACTION - 
// Output : char const
//-----------------------------------------------------------------------------
char const *CBaseDemoAction::NameForTimingType( DEMOACTIONTIMINGTYPE timingType )
{
	int idx = (int)timingType;
	if ( idx < 0 || idx >= NUM_TIMING_TYPES )
	{
		ConMsg( "ERROR: CBaseDemoAction::NameForTimingType type %i out of range\n", idx );
		return g_rgDemoTimingTypeNames[ ACTION_USES_NEITHER ].name;
	}
	
	DemoTimingTagDictionary *entry = &g_rgDemoTimingTypeNames[ idx ];
	Assert( entry->timingtype == timingType );

	return entry->name;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
// Output : DEMOACTION
//-----------------------------------------------------------------------------
DEMOACTIONTIMINGTYPE CBaseDemoAction::TimingTypeForName( char const *name )
{
	int c = NUM_TIMING_TYPES;
	int i;
	for ( i= 0; i < c; i++ )
	{
		DemoTimingTagDictionary *entry = &g_rgDemoTimingTypeNames[ i ];
		if ( !Q_strcasecmp( entry->name, name ) )
		{
			return entry->timingtype;
		}
	}

	return ACTION_USES_NEITHER;
}

static bool g_bSaveChained = false;

//-----------------------------------------------------------------------------
// Purpose: Simple printf wrapper which handles tab characters
// Input  : buf - 
//			*fmt - 
//			... - 
//-----------------------------------------------------------------------------
void CBaseDemoAction::BufPrintf( int depth, CUtlBuffer& buf, char const *fmt, ... )
{
	va_list		argptr;
	char		string[1024];
	va_start (argptr,fmt);
	Q_vsnprintf(string, sizeof( string ), fmt,argptr);
	va_end (argptr);

	while ( depth-- > 0 )
	{
		buf.Printf( "\t" );
	}

	buf.Printf( "%s", string );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : buf - 
//-----------------------------------------------------------------------------
void CBaseDemoAction::SaveKeysToBuffer( int depth, CUtlBuffer& buf )
{
	// All derived actions will need to do a BaseClass::SaveKeysToBuffer call
	g_bSaveChained = true;

	BufPrintf( depth, buf, "name \"%s\"\n", GetActionName() );
	if ( ActionHasTarget() )
	{
		BufPrintf( depth, buf, "target \"%s\"\n", GetActionTarget() );
	}
	switch ( GetTimingType() )
	{
	default:
	case ACTION_USES_NEITHER:
		break;
	case ACTION_USES_TICK:
		{
			BufPrintf( depth, buf, "starttick \"%i\"\n", GetStartTick() );
		}
		break;
	case ACTION_USES_TIME:
		{
			BufPrintf( depth, buf, "starttime \"%.3f\"\n", GetStartTime() );
		}
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : buf - 
//-----------------------------------------------------------------------------
void CBaseDemoAction::SaveToBuffer( int depth, int index, CUtlBuffer& buf )
{
	// Store index
	BufPrintf( depth, buf, "\"%i\"\n", index );
	BufPrintf( depth, buf, "%{\n" );

	g_bSaveChained = false;

	// First key is factory name
	BufPrintf( depth + 1, buf, "factory \"%s\"\n", NameForType( GetType() ) );
	SaveKeysToBuffer( depth + 1, buf );
	Assert( g_bSaveChained );

	BufPrintf( depth, buf, "}\n" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
void CBaseDemoAction::SetActionName( char const *name )
{
	Q_strncpy( m_szActionName, name, sizeof( m_szActionName ) );
}

//-----------------------------------------------------------------------------
// Purpose: Parse root data
// Input  : *pInitData - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseDemoAction::Init( KeyValues *pInitData )
{
	char const *actionname = pInitData->GetString( "name", "" );
	if ( !actionname || !actionname[ 0 ] )
	{
		Msg( "CBaseDemoAction::Init:  must specify a name for action!\n" );
		return false;
	}

	SetActionName( actionname );

	m_nStartTick	= pInitData->GetInt( "starttick", -1 );
	m_flStartTime	= pInitData->GetFloat( "starttime", -1.0f );

	if ( m_nStartTick == -1 && m_flStartTime == -1.0f )
	{
		m_Timing = ACTION_USES_NEITHER;
	}
	else if ( m_nStartTick != -1 )
	{
		m_Timing = ACTION_USES_TICK;
	}
	else
	{
		Assert( m_flStartTime != -1.0f );
		m_Timing = ACTION_USES_TIME;
	}

	// See if there's a target name
	char const *target = pInitData->GetString( "target", "" );
	if ( target && target[ 0 ] )
	{
		Q_strncpy( m_szActionTarget, target, sizeof( m_szActionTarget ) );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CBaseDemoAction::GetStartTick( void ) const
{
	Assert( m_Timing == ACTION_USES_TICK );
	return m_nStartTick;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CBaseDemoAction::GetStartTime( void ) const
{
	Assert( m_Timing == ACTION_USES_TIME );
	return m_flStartTime;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : frame - 
//-----------------------------------------------------------------------------
void CBaseDemoAction::SetStartTick( int tick )
{
	Assert( m_Timing == ACTION_USES_TICK );
	m_nStartTick = tick;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : t - 
//-----------------------------------------------------------------------------
void CBaseDemoAction::SetStartTime( float t )
{
	Assert( m_Timing == ACTION_USES_TIME );
	m_flStartTime = t;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : demoframe - 
//			demotime - 
//-----------------------------------------------------------------------------
bool CBaseDemoAction::Update( const DemoActionTimingContext& tc )
{
	// Already fired and done?
	if ( HasActionFinished() )
	{
		Assert( GetActionFired() );
		return false;
	}

	// Already fired, just waiting for finished tag
	if ( GetActionFired() )
	{
		return true;
	}
	
	// See if it's time to fire
	switch ( GetTimingType() )
	{
	default:
	case ACTION_USES_NEITHER:
		return false;
	case ACTION_USES_TICK:
		{
			if ( GetStartTick() >= tc.prevtick && GetStartTick() <= tc.curtick )
			{
				demoaction->InsertFireEvent( this );
			}
		}
		break;
	case ACTION_USES_TIME:
		{
			if ( GetStartTime() >= tc.prevtime && GetStartTime() <= tc.curtime )
			{
				demoaction->InsertFireEvent( this );
			}
		}
		break;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
char const *CBaseDemoAction::GetActionName( void ) const
{
	Assert( m_szActionName[ 0 ] );
	return m_szActionName;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseDemoAction::ActionHasTarget( void ) const
{
	return m_szActionTarget[ 0 ] ? true : false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
char const *CBaseDemoAction::GetActionTarget( void ) const
{
	Assert( ActionHasTarget() );
	return m_szActionTarget;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
void CBaseDemoAction::SetActionTarget( char const *name )
{
	Q_strncpy( m_szActionTarget, name, sizeof( m_szActionTarget ) );
}

//-----------------------------------------------------------------------------
// Purpose: Restart timing info
//-----------------------------------------------------------------------------
void CBaseDemoAction::Reset( void )
{
	SetActionFired( false );
	SetFinishedAction( false );
}

void CBaseDemoAction::OnActionFinished( void )
{
}

