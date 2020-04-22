//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CL_DEMOACTION_H
#define CL_DEMOACTION_H
#ifdef _WIN32
#pragma once
#endif

// Forward declarations
#include <KeyValues.h>

class CUtlBuffer;
class CBaseDemoAction;
class CBaseActionEditDialog;
class CDemoEditorPanel;
/*
namespace vgui
{
	class Panel;
}
*/

//-----------------------------------------------------------------------------
// Purpose: Types of demo actions we can take

// Actions all have a name and can have a start time/frame to auto start or can
//  be fired in response to other events
//-----------------------------------------------------------------------------
typedef enum
{
	DEMO_ACTION_UNKNOWN = 0,			// error
	DEMO_ACTION_SKIPAHEAD,				// SKip ahead in demo to time/frame
	DEMO_ACTION_STOPPLAYBACK,			// Terminate playback
	DEMO_ACTION_PLAYCOMMANDS,			// Type commands into console
	DEMO_ACTION_SCREENFADE_START,		// Start fade w/ name
	DEMO_ACTION_SCREENFADE_STOP,		// Cancel fade w/ name
	DEMO_ACTION_TEXTMESSAGE_START,		// Start text message w/ name
	DEMO_ACTION_TEXTMESSAGE_STOP,		// Stop text message by name
	DEMO_ACTION_PLAYCDTRACK_START,		// Start playing cd track
	DEMO_ACTION_PLAYCDTRACK_STOP,		// Cancel cd track
	DEMO_ACTION_PLAYSOUND_START,		// Start playing sound
	DEMO_ACTION_PLAYSOUND_END,			// Cancel sound

	DEMO_ACTION_ONSKIPPEDAHEAD,			// Listener for named skip ahead succeeding
	DEMO_ACTION_ONSTOPPEDPLAYBACK,		// Listener for stop event
	DEMO_ACTION_ONSCREENFADE_FINISHED,	// Fire when screen fade of specified name finishes
	DEMO_ACTION_ONTEXTMESSAGE_FINISHED,	// Fire when specified text message finishes
	DEMO_ACTION_ONPLAYCDTRACK_FINISHED,	// Fire when played cd track finishes
	DEMO_ACTION_ONPLAYSOUND_FINISHED,	// Fire when played sound finishes

	DEMO_ACTION_PAUSE,					// Pause playback for N seconds w/auto resume
	DEMO_ACTION_CHANGEPLAYBACKRATE,		// Slo-mo, etc

	DEMO_ACTION_ZOOM,					// Zoom in/out with hold

	// Must be last
	NUM_DEMO_ACTIONS,
} DEMOACTION;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
typedef enum
{
	ACTION_USES_NEITHER = 0,
	ACTION_USES_TICK,
	ACTION_USES_TIME,

	NUM_TIMING_TYPES,
} DEMOACTIONTIMINGTYPE;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
struct DemoActionTimingContext
{
	int		prevtick;
	int		curtick;
	float	prevtime;
	float	curtime;
};

typedef CBaseDemoAction * (*DEMOACTIONFACTORY_FUNC)( void );
typedef CBaseActionEditDialog *(*DEMOACTIONEDIT_FUNC)( CDemoEditorPanel *parent, CBaseDemoAction *action, bool newaction );

abstract_class CBaseDemoAction
{
public:

							CBaseDemoAction();
	virtual 				~CBaseDemoAction();

	virtual bool			Init( KeyValues *pInitData );
	virtual bool			Update( const DemoActionTimingContext& tc );

	// Do whatever the even is supposed to do
	virtual void			FireAction( void ) = 0;

	virtual void			Reset( void );

	virtual void			SaveKeysToBuffer( int depth, CUtlBuffer& buf );

	virtual void			OnActionFinished( void );

// Public methods
public:

	DEMOACTION				GetType( void ) const;
	void					SetType( DEMOACTION actionType );

	DEMOACTIONTIMINGTYPE	GetTimingType( void ) const;
	void					SetTimingType( DEMOACTIONTIMINGTYPE timingtype );
	
	void					SetActionFired( bool fired );
	bool					GetActionFired( void ) const;

	int						GetStartTick( void ) const;
	void					SetStartTick( int tick );
	float					GetStartTime( void ) const;
	void					SetStartTime( float time );

	void					SetFinishedAction( bool finished );
	bool					HasActionFinished( void ) const;

	char const				*GetActionName( void ) const;
	void					SetActionName( char const *name );

	bool					ActionHasTarget( void ) const;
	char const				*GetActionTarget( void ) const;
	void					SetActionTarget( char const *name );

	void					SaveToBuffer( int depth, int index, CUtlBuffer& buf );

public:

	static void				*operator new( size_t sz );
	static void				operator delete( void *pMem );

	static char const		*NameForType( DEMOACTION actionType );
	static DEMOACTION		TypeForName( char const *name );

	static char const		*NameForTimingType( DEMOACTIONTIMINGTYPE timingType );
	static DEMOACTIONTIMINGTYPE		TimingTypeForName( char const *name );

	static void				AddFactory( DEMOACTION actionType, DEMOACTIONFACTORY_FUNC func );
	static CBaseDemoAction	*CreateDemoAction( DEMOACTION actionType );

	static void				AddEditorFactory( DEMOACTION actionType, DEMOACTIONEDIT_FUNC func );
	static CBaseActionEditDialog *CreateActionEditor( DEMOACTION actionType, CDemoEditorPanel *parent, CBaseDemoAction *action, bool newaction );
	static bool				HasEditorFactory( DEMOACTION actionType );

	// Serialization helper ( handles indenting )
	static void				BufPrintf( int depth, CUtlBuffer& buf, PRINTF_FORMAT_STRING char const *fmt, ... );

private:

	enum
	{
		MAX_ACTION_NAME = 64,
	};

	DEMOACTION				m_Type;

	bool					m_bActionFired;
	bool					m_bActionFinished;
	
	char					m_szActionName[ MAX_ACTION_NAME ];

	char					m_szActionTarget[ MAX_ACTION_NAME ];

	DEMOACTIONTIMINGTYPE	m_Timing;
	int						m_nStartTick;
	float					m_flStartTime;
};

#define DECLARE_DEMOACTION( type, classname )			\
static CBaseDemoAction *FnCreate##classname( void )		\
{														\
	CBaseDemoAction *item = new classname();			\
	if ( item ) item->SetType( type );					\
	return item;										\
}														\
class CFactory##classname								\
{														\
public:													\
	CFactory##classname()								\
	{													\
		CBaseDemoAction::AddFactory(					\
			type,										\
			FnCreate##classname );						\
	}													\
};														\
static CFactory##classname g_Factory##classname;

#define DECLARE_DEMOACTIONEDIT( type, classname )		\
static CBaseActionEditDialog *FnCreateEditor##classname	\
	( CDemoEditorPanel *parent,							\
	  CBaseDemoAction *action,							\
	  bool newaction )									\
{														\
	CBaseActionEditDialog *editor = new classname(		\
		parent, action, newaction );					\
	if ( editor ) editor->Init();						\
	return editor;										\
}														\
class CFactoryEditor##classname							\
{														\
public:													\
	CFactoryEditor##classname()							\
	{													\
		CBaseDemoAction::AddEditorFactory(				\
			type,										\
			FnCreateEditor##classname );				\
	}													\
};														\
static CFactoryEditor##classname g_FactoryEditor##classname;

#endif // CL_DEMOACTION_H
