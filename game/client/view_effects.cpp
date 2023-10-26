//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "ivieweffects.h"
#include "shake.h"
#include "hud_macros.h"
#include "isaverestore.h"
#include "view_shared.h"
#include "iviewrender.h"
#include "viewrender.h"
#include "con_nprint.h"
#include "saverestoretypes.h"
#include "c_rumble.h"
#include "prediction.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IntroData_t *g_pIntroData;


// Arbitrary limit so that bad entity logic on the server can't consume tons of memory on the client.
#define MAX_SHAKES		32


//-----------------------------------------------------------------------------
// Purpose: Screen fade variables
//-----------------------------------------------------------------------------
struct screenfade_t
{
	float		Speed;		// How fast to fade (tics / second) (+ fade in, - fade out)
	float		End;		// When the fading hits maximum
	float		Reset;		// When to reset to not fading (for fadeout and hold)
	byte		r, g, b, alpha;	// Fade color
	int			Flags;		// Fading flags

	DECLARE_SIMPLE_DATADESC();
};

BEGIN_SIMPLE_DATADESC( screenfade_t )
	DEFINE_FIELD( Speed,	FIELD_FLOAT ),
	DEFINE_FIELD( End,		FIELD_TIME ),
	DEFINE_FIELD( Reset,	FIELD_TIME ),
	DEFINE_FIELD( r,		FIELD_CHARACTER ),
	DEFINE_FIELD( g,		FIELD_CHARACTER ),
	DEFINE_FIELD( b,		FIELD_CHARACTER ),
	DEFINE_FIELD( alpha,	FIELD_CHARACTER ),
	DEFINE_FIELD( Flags,	FIELD_INTEGER ),
END_DATADESC()


//-----------------------------------------------------------------------------
// Purpose: Screen shake variables
//-----------------------------------------------------------------------------
struct screenshake_t 
{
	float	endtime;
	float	duration;
	float	amplitude;
	float	frequency;
	float	nextShake;
	Vector	offset;
	float	angle;
	int		command;
	Vector  direction; // used only by kSHAKE_DIRECTIONAL

	// there are different types of screenshake -- 
	// eventually these different types could become
	// proper classes, but given the existing infrastructure
	// for transmitting screenshakes, right now it's just 
	// easier to use an enum and switch.
	enum ShakeType_t 
	{
		kSHAKE_BASIC,  ///< the original screenshake mechanism, a random offset selected every few frames.
		kSHAKE_DIRECTIONAL,  ///< a pseudo-damped-spring-ish punch to a specific screen space direction.
	};
	uint8 nShakeType; // actually a ShakeType_t, packed into eight bits (the datadesc system doesn't like bitfields)

	screenshake_t() : nShakeType(kSHAKE_BASIC) {};  // nothing else is explicitly initialized

	DECLARE_SIMPLE_DATADESC();
};

BEGIN_SIMPLE_DATADESC( screenshake_t )
	DEFINE_FIELD( endtime,		FIELD_TIME ),
	DEFINE_FIELD( duration,		FIELD_FLOAT ),
	DEFINE_FIELD( amplitude,	FIELD_FLOAT ),
	DEFINE_FIELD( frequency,	FIELD_FLOAT ),
	DEFINE_FIELD( nextShake,	FIELD_TIME ),
	DEFINE_FIELD( offset,		FIELD_VECTOR ),
	DEFINE_FIELD( angle,		FIELD_FLOAT ),
	DEFINE_FIELD( nShakeType,	FIELD_CHARACTER ),
	DEFINE_FIELD( direction,	FIELD_VECTOR ),
END_DATADESC()


void CC_Shake_Stop();
//-----------------------------------------------------------------------------
// Purpose: Screen tilt variables
//-----------------------------------------------------------------------------
struct screentilt_t 
{
	bool	easeInOut;
	QAngle	angle;
	float	starttime;
	float	endtime;
	float	duration;
	float	tiltTime;
	Vector	offset;
	int		command;

	DECLARE_SIMPLE_DATADESC();
};

BEGIN_SIMPLE_DATADESC( screentilt_t )
	DEFINE_FIELD( angle,		FIELD_VECTOR ),
	DEFINE_FIELD( starttime,	FIELD_TIME ),
	DEFINE_FIELD( endtime,		FIELD_TIME ),
	DEFINE_FIELD( duration,		FIELD_FLOAT ),
	DEFINE_FIELD( tiltTime,	FIELD_TIME ),
	DEFINE_FIELD( offset,		FIELD_VECTOR ),
END_DATADESC()


//-----------------------------------------------------------------------------
// Purpose: Implements the view effects interface for the client .dll
//-----------------------------------------------------------------------------
class CViewEffects : public IViewEffects
{
public:

	~CViewEffects()
	{
		ClearAllFades();
	}

	virtual void	Init( void );
	virtual void	LevelInit( void );
	virtual void	GetFadeParams( byte *r, byte *g, byte *b, byte *a, bool *blend );
	virtual void	CalcShake( void );
	virtual void	ApplyShake( Vector& origin, QAngle& angles, float factor );
	virtual void	CalcTilt( void );
	virtual void	ApplyTilt( QAngle& angles, float factor );

	virtual void	Shake( const ScreenShake_t &data );
	virtual void	Tilt( ScreenTilt_t &data );
	virtual void	Fade( ScreenFade_t &data );
	virtual void	ClearPermanentFades( void );
	virtual void	FadeCalculate( void );
	virtual void	ClearAllFades( void );

	// Save / Restore
	virtual void	Save( ISave *pSave );
	virtual void	Restore( IRestore *pRestore, bool fCreatePlayers );

private:

	void ClearAllShakes();
	screenshake_t *FindLongestShake();

	void ClearAllTilts();
	screentilt_t *FindLongestTilt();

	// helper subfunctions used inside CalcShake
	void CalcShake_Basic( screenshake_t * pShake, float * RESTRICT pflRumbleAngle );
	void CalcShake_Directional( screenshake_t * pShake, float * RESTRICT pflRumbleAngle );

	CUtlVector<screenfade_t *>	m_FadeList;

	CUtlVector<screenshake_t *>	m_ShakeList;
	Vector m_vecShakeAppliedOffset;
	float m_flShakeAppliedAngle;

	CUtlVector<screentilt_t *>	m_TiltList;
	QAngle m_vecTiltAppliedAngle;

	int							m_FadeColorRGBA[4];
	bool						m_bModulate;

	friend void CC_Shake_Stop();
};

static CViewEffects g_ViewEffects[ MAX_SPLITSCREEN_PLAYERS ];

IViewEffects *GetViewEffects()
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	return &g_ViewEffects[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
}

static CViewEffects &GetCViewEffects()
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	return g_ViewEffects[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
}

// Callback function to call at end of screen m_Fade.
static int s_nCallbackParameter;
static void ( *s_pfnFadeDoneCallback )( int parm1 );


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pszName - 
//			iSize - 
//			*pbuf - 
// Output : static int
//-----------------------------------------------------------------------------
void __MsgFunc_Shake( bf_read &msg )
{
	ScreenShake_t shake;

	shake.command	= (ShakeCommand_t)(msg.ReadByte());
	shake.amplitude = msg.ReadFloat();
	shake.frequency = msg.ReadFloat();
	shake.duration	= msg.ReadFloat();

	GetCViewEffects().Shake( shake );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pszName - 
//			iSize - 
//			*pbuf - 
// Output : static int
//-----------------------------------------------------------------------------
void __MsgFunc_ShakeDir( bf_read &msg )
{
	if ( prediction && prediction->InPrediction() && !prediction->IsFirstTimePredicted() )
		return;

	ScreenShake_t shake;

	shake.command	= (ShakeCommand_t)msg.ReadByte();
	shake.amplitude = msg.ReadFloat();
	shake.frequency = msg.ReadFloat();
	shake.duration	= msg.ReadFloat();
	msg.ReadBitVec3Normal( shake.direction );

	GetCViewEffects().Shake( shake );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pszName - 
//			iSize - 
//			*pbuf - 
// Output : static int
//-----------------------------------------------------------------------------
void __MsgFunc_Tilt( bf_read &msg )
{
	ScreenTilt_t tilt;

	Vector vecAngle;

	tilt.command = msg.ReadByte();
	tilt.easeInOut = msg.ReadByte() ? true : false;
	tilt.angle.x = msg.ReadFloat();
	tilt.angle.y = msg.ReadFloat();
	tilt.angle.z = msg.ReadFloat();
	tilt.duration = msg.ReadFloat();
	tilt.time = msg.ReadFloat();

	GetCViewEffects().Tilt( tilt );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pszName - 
//			iSize - 
//			*pbuf - 
// Output : static int
//-----------------------------------------------------------------------------
void __MsgFunc_Fade( bf_read &msg )
{
	ScreenFade_t fade;

	fade.duration = msg.ReadShort(); // fade lasts this long
	fade.holdTime = msg.ReadShort(); // fade lasts this long
	fade.fadeFlags = msg.ReadShort(); // fade type (in / out)
	fade.r = msg.ReadByte(); // fade red
	fade.g = msg.ReadByte(); // fade green
	fade.b = msg.ReadByte(); // fade blue
	fade.a = msg.ReadByte(); // fade blue

	GetCViewEffects().Fade( fade );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CViewEffects::Init( void )
{
	HOOK_MESSAGE( Shake );
#ifdef INFESTED_DLL // the user message ShakeDir isn't registered for other games, but if you add it to your RegisterUserMessages, then you can un-#ifdef this
	HOOK_MESSAGE( ShakeDir ); // directional screen shake
#endif
#ifdef HL2_CLIENT
	// @TODO: Jeep, this causes assert in other games w/o this guard ifdef [6/3/2008 tom]
	HOOK_MESSAGE( Tilt );
#endif
	HOOK_MESSAGE( Fade );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CViewEffects::LevelInit( void )
{
	ClearAllShakes();
	ClearAllTilts();
	ClearAllFades();
}


static ConVar shake_show( "shake_show", "0", 0, "Displays a list of the active screen shakes." );
static ConCommand shake_stop("shake_stop", CC_Shake_Stop, "Stops all active screen shakes.\n", FCVAR_CHEAT );

//-----------------------------------------------------------------------------
// Purpose: Stops all active screen shakes.
//-----------------------------------------------------------------------------
void CC_Shake_Stop()
{
	GetCViewEffects().ClearAllShakes();
}


//-----------------------------------------------------------------------------
// Purpose: Test a punch-type screen shake
//-----------------------------------------------------------------------------
void CC_Shake_TestPunch( const CCommand &args )
{
	if ( args.ArgC() < 7 )
	{
		Msg("Usage: %s x y z f a d\n"
			"where x,y,z are direction of screen punch\n"
			"      f     is  frequency (1 means three bounces before settling)\n"
			"      a     is  amplitude\n"
			"      d     is  duration\n", 
			args[0] );
	}

	const float x = atof( args[1] );
	const float y = atof( args[2] );
	const float z = atof( args[3] );
	const float f = atof( args[4] );
	const float a = atof( args[5] );
	const float d = atof( args[6] );

	ScreenShake_t shake;
	shake.command = SHAKE_START;
	shake.amplitude = a;
	shake.frequency = f;
	shake.duration  = d;
	shake.direction = Vector(x,y,z);


	GetCViewEffects().Shake(shake);
}


static ConCommand shake_testpunch("shake_testpunch", CC_Shake_TestPunch, "Test a punch-style screen shake.\n", FCVAR_CHEAT );

//-----------------------------------------------------------------------------
// Purpose: Apply noise to the eye position.
// UNDONE: Feedback a bit of this into the view model position.  It shakes too much
//-----------------------------------------------------------------------------
void CViewEffects::CalcShake( void )
{

	// We'll accumulate the aggregate shake for this frame into these data members.
	m_vecShakeAppliedOffset.Init(0, 0, 0);
	m_flShakeAppliedAngle = 0;
	float flRumbleAngle = 0;

	bool bShow = shake_show.GetBool();

	int nShakeCount = m_ShakeList.Count();

	for ( int nShake = nShakeCount - 1; nShake >= 0; nShake-- )
	{
		screenshake_t * RESTRICT pShake = m_ShakeList.Element( nShake );

		if ( pShake->endtime == 0 )
		{
			// Shouldn't be any such shakes in the list.
			AssertMsg( false, "A screenshake has null endtime in CViewEffects::CalcShake\n" );
			continue;
		}

		if ( ( gpGlobals->curtime > pShake->endtime ) || 
			pShake->duration <= 0 || 
			pShake->amplitude <= 0 || 
			pShake->frequency <= 0 )
		{
			// Retire this shake.
			delete m_ShakeList.Element( nShake );
			m_ShakeList.FastRemove( nShake );
			continue;
		}

		if ( bShow )
		{
			con_nprint_t np;
			np.time_to_live = 2.0f;
			np.fixed_width_font = true;
			np.color[0] = 1.0;
			np.color[1] = 0.8;
			np.color[2] = 0.1;
			np.index = nShake + 2;

			engine->Con_NXPrintf( &np, "%02d: dur(%8.2f) amp(%8.2f) freq(%8.2f)", nShake + 1, (double)pShake->duration, (double)pShake->amplitude, (double)pShake->frequency );
		}

		// select the appropriate behavior based on screenshake type
		switch ( pShake->nShakeType )
		{
		case screenshake_t::kSHAKE_BASIC:
			CalcShake_Basic( pShake, &flRumbleAngle );
			break;
		case screenshake_t::kSHAKE_DIRECTIONAL:
			CalcShake_Directional( pShake, &flRumbleAngle );
			break;
		default:
			AssertMsg1( false, "Unknown shake type %d\n", pShake->nShakeType );
		}

	}

	// Feed this to the rumble system!
	UpdateScreenShakeRumble( XBX_GetActiveUserId(), flRumbleAngle );
}



void CViewEffects::CalcShake_Basic( screenshake_t * RESTRICT pShake, float * RESTRICT pflRumbleAngle )
{
	float	fraction, freq;

	if ( gpGlobals->curtime > pShake->nextShake )
	{
		// Higher frequency means we recalc the extents more often and perturb the display again
		pShake->nextShake = gpGlobals->curtime + (1.0f / pShake->frequency);

		// Compute random shake extents (the shake will settle down from this)
		for (int i = 0; i < 3; i++ )
		{
			pShake->offset[i] = random->RandomFloat( -pShake->amplitude, pShake->amplitude );
		}

		pShake->angle = random->RandomFloat( -pShake->amplitude*0.25, pShake->amplitude*0.25 );
	}

	// Ramp down amplitude over duration (fraction goes from 1 to 0 linearly with slope 1/duration)
	fraction = ( pShake->endtime - gpGlobals->curtime ) / pShake->duration;

	// Ramp up frequency over duration
	if ( fraction )
	{
		freq = (pShake->frequency / fraction);
	}
	else
	{
		freq = 0;
	}

	// square fraction to approach zero more quickly
	fraction *= fraction;

	// Sine wave that slowly settles to zero
	float angle = gpGlobals->curtime * freq;
	if ( angle > 1e8 )
	{
		angle = 1e8;
	}
	fraction = fraction * sin( angle );

	if( pShake->command != SHAKE_START_NORUMBLE )
	{
		// As long as this isn't a NO RUMBLE effect, then accumulate rumble
		*pflRumbleAngle += pShake->angle * fraction;
	}

	if( pShake->command != SHAKE_START_RUMBLEONLY )
	{
		// As long as this isn't a RUMBLE ONLY effect, then accumulate screen shake

		// Add to view origin
		m_vecShakeAppliedOffset += pShake->offset * fraction;

		// Add to roll
		m_flShakeAppliedAngle += pShake->angle * fraction;
	}

	// Drop amplitude a bit, less for higher frequency shakes
	pShake->amplitude -= pShake->amplitude * ( gpGlobals->frametime / (pShake->duration * pShake->frequency) );

}


void CViewEffects::CalcShake_Directional( screenshake_t * RESTRICT pShake, float * RESTRICT pflRumbleAngle )
{
	// a screen punch follows an equation of the form 
	// y = sin(fx) * ( 1 - x / (3pi) ) for x=0..3pi
	// where the duration is transformed to occupy 
	// the region 0..3pi
	// and the frequency can be any number (which controls the number of oscillations 
	// before lapsing out)

	// because of the resolution of this shake, it is performed every frame
	// (ignores nextShake)
	pShake->nextShake = gpGlobals->curtime + 0.001;
	float t = 1 - ( pShake->endtime - gpGlobals->curtime ) / pShake->duration; // t varies 0 .. 1 over life of shake
	float fraction = ( 1 - t ); // compiler will hopefully elide the double subtraction

	// transform the duration and so that x varies 0 .. 3PI over lifespan
	t *= ( 3 * M_PI ); // t varies 0 .. 3PI 

	const float x = t * pShake->frequency;
	const float y = sin(x) * fraction;

	// transform this -1..1 sinusoid by amplitude and direction
	pShake->offset = pShake->direction * ( pShake->amplitude * y );


	if( pShake->command != SHAKE_START_NORUMBLE )
	{
		// As long as this isn't a NO RUMBLE effect, then accumulate rumble
		*pflRumbleAngle += y;
	}

	if( pShake->command != SHAKE_START_RUMBLEONLY )
	{
		// As long as this isn't a RUMBLE ONLY effect, then accumulate screen shake

		// Add to view origin
		m_vecShakeAppliedOffset += pShake->offset ;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Apply the current screen shake to this origin/angles.  Factor is the amount to apply
//  This is so you can blend in part of the shake
// Input  : origin - 
//			angles - 
//			factor - 
//-----------------------------------------------------------------------------
void CViewEffects::ApplyShake( Vector& origin, QAngle& angles, float factor )
{
	VectorMA( origin, factor, m_vecShakeAppliedOffset, origin );
	angles.z += m_flShakeAppliedAngle * factor;
}

//-----------------------------------------------------------------------------
// Purpose: Apply noise to the eye position.
// UNDONE: Feedback a bit of this into the view model position.  It shakes too much
//-----------------------------------------------------------------------------
void CViewEffects::CalcTilt( void )
{
	m_vecTiltAppliedAngle.Init();

	int nTiltCount = m_TiltList.Count();

	for ( int nTilt = nTiltCount - 1; nTilt >= 0; nTilt-- )
	{
		screentilt_t *pTilt = m_TiltList.Element( nTilt );

		if ( pTilt->endtime == 0 )
		{
			// Shouldn't be any such tilts in the list.
			Assert( false );
			continue;
		}

		if ( ( gpGlobals->curtime > pTilt->endtime ) || 
			pTilt->duration <= 0 || pTilt->angle == QAngle( 0.0f, 0.0f, 0.0f ) )
		{
			// Retire this tilt.
			delete m_TiltList.Element( nTilt );
			m_TiltList.FastRemove( nTilt );
			continue;
		}

		float flInterp = ( gpGlobals->curtime - pTilt->starttime ) / pTilt->tiltTime;

		float flReturnInterp = ( pTilt->endtime - gpGlobals->curtime ) / pTilt->tiltTime;

		if ( flReturnInterp < flInterp )
		{
			flInterp = flReturnInterp;
		}

		float flSmoothInterp = clamp( flInterp, 0.0f, 1.0f );

		if ( pTilt->easeInOut )
		{
			// Do a smooth ease in and out
			flSmoothInterp = 1.0f - 0.5f * ( cosf( flSmoothInterp * M_PI ) + 1.0f );
		}

		// Accumulate world tilt
		m_vecTiltAppliedAngle += pTilt->angle * flSmoothInterp;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Apply the current screen shake to this origin/angles.  Factor is the amount to apply
//  This is so you can blend in part of the shake
// Input  : origin - 
//			angles - 
//			factor - 
//-----------------------------------------------------------------------------
void CViewEffects::ApplyTilt( QAngle& angles, float factor )
{
	if ( m_vecTiltAppliedAngle == QAngle( 0.0f, 0.0f, 0.0f ) )
	{
		// Fast out, no tilt to apply
		return;
	}

	matrix3x4_t matTilt;
	AngleIMatrix( m_vecTiltAppliedAngle, matTilt );

	matrix3x4_t matToWorld;
	AngleMatrix( angles, matToWorld );

	matrix3x4_t matTiltToWorld;
	ConcatTransforms( matTilt, matToWorld, matTiltToWorld);

	Vector vecForwardTilted, vecUpTilted;
	VectorTransform( Vector( 1.0f, 0.0, 0.0f ), matTiltToWorld, vecForwardTilted );
	VectorTransform( Vector( 0.0f, 0.0, 1.0f ), matTiltToWorld, vecUpTilted );

	QAngle anglesTilted;
	VectorAngles( vecForwardTilted, vecUpTilted, anglesTilted );

	angles = anglesTilted;
}

//-----------------------------------------------------------------------------
// Purpose: Zeros out all active screen shakes.
//-----------------------------------------------------------------------------
void CViewEffects::ClearAllShakes()
{
	int nShakeCount = m_ShakeList.Count();
	for ( int i = 0; i < nShakeCount; i++ )
	{
		delete m_ShakeList.Element( i );
	}

	m_ShakeList.Purge();
}


//-----------------------------------------------------------------------------
// Purpose: Returns the shake with the longest duration. This is the shake we
//			use anytime we get an amplitude or frequency command, because the
//			most likely case is that we're modifying a shake with a long
//			duration rather than a brief shake caused by an explosion, etc.
//-----------------------------------------------------------------------------
screenshake_t *CViewEffects::FindLongestShake()
{
	screenshake_t *pLongestShake = NULL;

	int nShakeCount = m_ShakeList.Count();
	for ( int i = 0; i < nShakeCount; i++ )
	{
		screenshake_t *pShake = m_ShakeList.Element( i );
		if ( pShake && ( !pLongestShake || ( pShake->duration > pLongestShake->duration ) ) )
		{
			pLongestShake = pShake;
		}
	}

	return pLongestShake;
}


//-----------------------------------------------------------------------------
// Purpose: Message hook to parse ScreenShake messages
// Input  : pszName - 
//			iSize - 
//			pbuf - 
// Output : 
//-----------------------------------------------------------------------------
void CViewEffects::Shake( const ScreenShake_t &data )
{
	if ( prediction && prediction->InPrediction() && !prediction->IsFirstTimePredicted() )
		return;

	if ( ( data.command == SHAKE_START || data.command == SHAKE_START_RUMBLEONLY ) && ( m_ShakeList.Count() < MAX_SHAKES ) )
	{
		screenshake_t * RESTRICT pNewShake = new screenshake_t; // ugh, should just make these a static array
			
		pNewShake->amplitude = data.amplitude;
		pNewShake->frequency = data.frequency;
		pNewShake->duration = data.duration;
		pNewShake->nextShake = 0;
		pNewShake->endtime = gpGlobals->curtime + data.duration;

		if ( prediction && prediction->InPrediction() )
		{
			pNewShake->endtime = prediction->GetSavedTime() + data.duration;
		}

		pNewShake->command = data.command;
		pNewShake->direction = data.direction;
		pNewShake->nShakeType = data.direction.IsZeroFast() ? screenshake_t::kSHAKE_BASIC : screenshake_t::kSHAKE_DIRECTIONAL;

		m_ShakeList.AddToTail( pNewShake );
	}
	else if ( data.command == SHAKE_STOP)
	{
		ClearAllShakes();
	}
	else if ( data.command == SHAKE_AMPLITUDE )
	{
		// Look for the most likely shake to modify.
		screenshake_t * RESTRICT pShake = FindLongestShake();
		if ( pShake )
		{
			pShake->amplitude = data.amplitude;
		}
	}
	else if ( data.command == SHAKE_FREQUENCY )
	{
		// Look for the most likely shake to modify.
		screenshake_t * RESTRICT pShake = FindLongestShake();
		if ( pShake )
		{
			pShake->frequency = data.frequency;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Zeros out all active screen tilts.
//-----------------------------------------------------------------------------
void CViewEffects::ClearAllTilts()
{
	int nTiltCount = m_TiltList.Count();
	for ( int i = 0; i < nTiltCount; i++ )
	{
		delete m_TiltList.Element( i );
	}

	m_TiltList.Purge();
}


//-----------------------------------------------------------------------------
// Purpose: Returns the shake with the longest duration. This is the shake we
//			use anytime we get an amplitude or frequency command, because the
//			most likely case is that we're modifying a shake with a long
//			duration rather than a brief shake caused by an explosion, etc.
//-----------------------------------------------------------------------------
screentilt_t *CViewEffects::FindLongestTilt()
{
	screentilt_t *pLongestTilt = NULL;

	int nTiltCount = m_TiltList.Count();
	for ( int i = 0; i < nTiltCount; i++ )
	{
		screentilt_t *pTilt = m_TiltList.Element( i );
		if ( pTilt && ( !pLongestTilt || ( pTilt->duration > pLongestTilt->duration ) ) )
		{
			pLongestTilt = pTilt;
		}
	}

	return pLongestTilt;
}

//-----------------------------------------------------------------------------
// Purpose: Message hook to parse ScreenTilt messages
// Input  : pszName - 
//			iSize - 
//			pbuf - 
// Output : 
//-----------------------------------------------------------------------------
void CViewEffects::Tilt( ScreenTilt_t &data )
{
	if ( ( data.command == SHAKE_START || data.command == SHAKE_START_RUMBLEONLY ) && ( m_ShakeList.Count() < MAX_SHAKES ) )
	{
		screentilt_t *pNewTilt = new screentilt_t;

		pNewTilt->easeInOut = data.easeInOut;
		pNewTilt->angle = data.angle;
		pNewTilt->duration = data.duration;
		pNewTilt->tiltTime = data.time;
		pNewTilt->starttime = gpGlobals->curtime;
		pNewTilt->endtime = pNewTilt->starttime + data.duration;
		pNewTilt->command = data.command;

		m_TiltList.AddToTail( pNewTilt );
	}
	else if ( data.command == SHAKE_STOP)
	{
		ClearAllTilts();
	}
}



//-----------------------------------------------------------------------------
// Purpose: Message hook to parse ScreenFade messages
// Input  : *pszName - 
//			iSize - 
//			*pbuf - 
// Output : int
//-----------------------------------------------------------------------------
void CViewEffects::Fade( ScreenFade_t &data )
{
	// Create a new fade and append it to the list
	screenfade_t *pNewFade = new screenfade_t;
	pNewFade->End	= data.duration * (1.0f/(float)(1<<SCREENFADE_FRACBITS));
	pNewFade->Reset	= data.holdTime * (1.0f/(float)(1<<SCREENFADE_FRACBITS));
	pNewFade->r		= data.r;
	pNewFade->g		= data.g;
	pNewFade->b		= data.b;
	pNewFade->alpha	= data.a;
	pNewFade->Flags	= data.fadeFlags;
	pNewFade->Speed	= 0;

	// Calc fade speed
	if ( data.duration > 0 )
	{
		if ( data.fadeFlags & FFADE_OUT )
		{
			if ( pNewFade->End )
			{
				pNewFade->Speed = -(float)pNewFade->alpha / pNewFade->End;
			}

			pNewFade->End	+= gpGlobals->curtime;
			pNewFade->Reset	+= pNewFade->End;
		}
		else
		{
			if ( pNewFade->End )
			{
				pNewFade->Speed = (float)pNewFade->alpha / pNewFade->End;
			}

			pNewFade->Reset	+= gpGlobals->curtime;
			pNewFade->End	+= pNewFade->Reset;
		}
	}

	if ( data.fadeFlags & FFADE_PURGE )
	{
		ClearAllFades();
	}

	m_FadeList.AddToTail( pNewFade );
}

//-----------------------------------------------------------------------------
// Purpose: Compute the overall color & alpha of the fades
//-----------------------------------------------------------------------------
void CViewEffects::FadeCalculate( void )
{
	// Cycle through all fades and remove any that have finished (work backwards)
	int i;
	int iSize = m_FadeList.Count();
	for (i = iSize-1; i >= 0; i-- )
	{
		screenfade_t *pFade = m_FadeList[i];

		// Keep pushing reset time out indefinitely
		if ( pFade->Flags & FFADE_STAYOUT )
		{
			pFade->Reset = gpGlobals->curtime + 0.1;
		}

		// All done?
		if ( ( gpGlobals->curtime > pFade->Reset ) && ( gpGlobals->curtime > pFade->End ) )
		{
			// User passed in a callback function, call it now
			if ( s_pfnFadeDoneCallback )
			{
				s_pfnFadeDoneCallback( s_nCallbackParameter );
				s_pfnFadeDoneCallback = NULL;
				s_nCallbackParameter = 0;
			}

			// Remove this Fade from the list
			m_FadeList.FindAndRemove( pFade );
			delete pFade;
		}
	}

	m_bModulate = false;
	m_FadeColorRGBA[0] = m_FadeColorRGBA[1] = m_FadeColorRGBA[2] = m_FadeColorRGBA[3] = 0;

	// Cycle through all fades in the list and calculate the overall color/alpha
	for ( i = 0; i < m_FadeList.Count(); i++ )
	{
		screenfade_t *pFade = m_FadeList[i];

		// Color
		m_FadeColorRGBA[0] += pFade->r;
		m_FadeColorRGBA[1] += pFade->g;
		m_FadeColorRGBA[2] += pFade->b;

		// Fading...
		int iFadeAlpha;
		if ( pFade->Flags & (FFADE_OUT|FFADE_IN) )
		{
			iFadeAlpha = pFade->Speed * ( pFade->End - gpGlobals->curtime );
			if ( pFade->Flags & FFADE_OUT )
			{
				iFadeAlpha += pFade->alpha;
			}
			iFadeAlpha = MIN( iFadeAlpha, pFade->alpha );
			iFadeAlpha = MAX( 0, iFadeAlpha );
		}
		else
		{
			iFadeAlpha = pFade->alpha;
		}

		// Use highest alpha
		if ( iFadeAlpha > m_FadeColorRGBA[3] )
		{
			m_FadeColorRGBA[3] = iFadeAlpha;
		}

		// Modulate?
		if ( pFade->Flags & FFADE_MODULATE )
		{
			m_bModulate = true;
		}
	}

	// Divide colors
	if ( m_FadeList.Count() )
	{
		m_FadeColorRGBA[0] /= m_FadeList.Count();
		m_FadeColorRGBA[1] /= m_FadeList.Count();
		m_FadeColorRGBA[2] /= m_FadeList.Count();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Clear only the permanent fades in our fade list
//-----------------------------------------------------------------------------
void CViewEffects::ClearPermanentFades( void )
{
	int iSize = m_FadeList.Count();
	for (int i =  iSize-1; i >= 0; i-- )
	{
		screenfade_t *pFade = m_FadeList[i];

		if ( pFade->Flags & FFADE_STAYOUT )
		{
			// Destroy this fade
			m_FadeList.FindAndRemove( pFade );
			delete pFade;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Purge & delete all fades in the queue
//-----------------------------------------------------------------------------
void CViewEffects::ClearAllFades( void )
{
	int iSize = m_FadeList.Count();
	for (int i =  iSize-1; i >= 0; i-- )
	{
		delete m_FadeList[i];
	}
	m_FadeList.Purge();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : context - Which call to Render is this ( CViewSetup::context )
//			*r - 
//			*g - 
//			*b - 
//			*a - 
//			*blend - 
//-----------------------------------------------------------------------------
void CViewEffects::GetFadeParams( byte *r, byte *g, byte *b, byte *a, bool *blend )
{
	// If the intro is overriding our fade, use that instead
	if ( g_pIntroData && g_pIntroData->m_flCurrentFadeColor[3] )
	{
		*r = g_pIntroData->m_flCurrentFadeColor[0];
		*g = g_pIntroData->m_flCurrentFadeColor[1];
		*b = g_pIntroData->m_flCurrentFadeColor[2];
		*a = g_pIntroData->m_flCurrentFadeColor[3];
		*blend = false;
		return;
	}

	FadeCalculate();

	*r = m_FadeColorRGBA[0];
	*g = m_FadeColorRGBA[1];
	*b = m_FadeColorRGBA[2];
	*a = m_FadeColorRGBA[3];
	*blend = m_bModulate;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSave - 
//-----------------------------------------------------------------------------
void CViewEffects::Save( ISave *pSave )
{
	// Save the view fades
	int iCount = m_FadeList.Count();
	pSave->WriteInt( &iCount );
	for ( int i = 0; i < iCount; i++ )
	{
		pSave->StartBlock();
		pSave->WriteAll( m_FadeList[i] );
		pSave->EndBlock();
	}

	// Save the view shakes
	iCount = m_ShakeList.Count();
	pSave->WriteInt( &iCount );
	for ( int i = 0; i < iCount; i++ )
	{
		pSave->StartBlock();
		pSave->WriteAll( m_ShakeList[i] );
		pSave->EndBlock();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pRestore - 
//			fCreatePlayers - 
//-----------------------------------------------------------------------------
void CViewEffects::Restore( IRestore *pRestore, bool fCreatePlayers )
{
	CGameSaveRestoreInfo *pSaveData = pRestore->GetGameSaveRestoreInfo();

	// View effects is a singleton so we only need to restore it once,
	// from the level that we are going into.
	if( !pSaveData->levelInfo.fUseLandmark )
	{
		ClearAllFades();
		ClearAllShakes();

		// Read in the view fades
		int iCount = pRestore->ReadInt();
		for ( int i = 0; i < iCount; i++ )
		{
			screenfade_t *pNewFade = new screenfade_t;

			pRestore->StartBlock();
			pRestore->ReadAll( pNewFade );
			pRestore->EndBlock();

			m_FadeList.AddToTail( pNewFade );
		}

		// Read in the view shakes
		iCount = pRestore->ReadInt();
		for ( int i = 0; i < iCount; i++ )
		{
			screenshake_t *pNewShake = new screenshake_t;

			pRestore->StartBlock();
			pRestore->ReadAll( pNewShake );
			pRestore->EndBlock();

			m_ShakeList.AddToTail( pNewShake );
		}
	}	
}

//====================================================================================================
// CLIENTSIDE VIEW EFFECTS SAVE/RESTORE 
//====================================================================================================
static short VIEWEFFECTS_SAVE_RESTORE_VERSION = 2;

class CViewEffectsSaveRestoreBlockHandler :	public CDefSaveRestoreBlockHandler
{
	struct QueuedItem_t;
public:
	CViewEffectsSaveRestoreBlockHandler()
	{
	}

	const char *GetBlockName()
	{
		return "ViewEffects";
	}

	//---------------------------------

	virtual void PreSave( CSaveRestoreData * ) 
	{
	}
	
	//---------------------------------

	virtual void Save( ISave *pSave ) 
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( 0 );
		GetViewEffects()->Save( pSave );
	}
	
	//---------------------------------

	virtual void WriteSaveHeaders( ISave *pSave )
	{
		pSave->WriteShort( &VIEWEFFECTS_SAVE_RESTORE_VERSION );
	}
	
	//---------------------------------

	virtual void PostSave() 
	{
	}
	
	//---------------------------------

	virtual void PreRestore() 
	{
	}
	
	//---------------------------------

	virtual void ReadRestoreHeaders( IRestore *pRestore )
	{
		// No reason why any future version shouldn't try to retain backward compatability. The default here is to not do so.
		short version = pRestore->ReadShort();
		m_bDoLoad = ( version == VIEWEFFECTS_SAVE_RESTORE_VERSION );
	}

	//---------------------------------
	
	virtual void Restore( IRestore *pRestore, bool fCreatePlayers ) 
	{
		if ( m_bDoLoad )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD( 0 );
			GetViewEffects()->Restore( pRestore, fCreatePlayers );
		}
	}
	
	//---------------------------------
	
	virtual void PostRestore() 
	{
	}
	
private:
	bool	m_bDoLoad;
};

//-----------------------------------------------------------------------------

CViewEffectsSaveRestoreBlockHandler g_ViewEffectsSaveRestoreBlockHandler;

ISaveRestoreBlockHandler *GetViewEffectsRestoreBlockHandler()
{
	return &g_ViewEffectsSaveRestoreBlockHandler;
}
