//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "animation.h"
#include "flex_expresser.h"
#include "entitylist.h"
/*
#include "filesystem.h"
#include "studio.h"
#include "choreoevent.h"
#include "choreoscene.h"
#include "choreoactor.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "tier1/strtools.h"
#include "KeyValues.h"
#include "ai_basenpc.h"
#include "ai_navigator.h"
#include "ai_moveprobe.h"
#include "sceneentity.h"
#include "ai_baseactor.h"
#include "datacache/imdlcache.h"
#include "tier1/byteswap.h"
*/

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



CFlexExpresser::CFlexExpresser()
{
	m_pExpresser = NULL;
	m_flThenAnyMaxDist = 0;
}


CFlexExpresser::~CFlexExpresser()
{
	if (m_pExpresser)
		delete m_pExpresser;
	m_pExpresser = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAI_Expresser *CFlexExpresser::CreateExpresser( void )
{
	AssertMsg1( !m_pExpresser, "LEAK: Double-created expresser for FlexExpresser %s", GetDebugName() );

	m_pExpresser = new CAI_ExpresserWithFollowup(this);
	if ( !m_pExpresser )
		return NULL;
	
	m_pExpresser->Connect(this);
	return m_pExpresser;
}

// does not call to base class spawn.
void CFlexExpresser::Spawn( void )
{
	const char *szModel = (char *)STRING( GetModelName() );
	if (!szModel || !*szModel)
	{
		Warning( "WARNING: %s at %.0f %.0f %0.f missing modelname\n", GetDebugName(), GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z );
	}
	else
	{
	    PrecacheModel( szModel );
	    SetModel( szModel );
	}
	Precache();

	if ( m_spawnflags & FCYCLER_NOTSOLID )
	{
		SetSolid( SOLID_NONE );
	}
	else
	{
		SetSolid( SOLID_BBOX );
		AddSolidFlags( FSOLID_NOT_STANDABLE );
	}
	SetMoveType( MOVETYPE_NONE );

	// funcorators are immortal
	m_takedamage		= DAMAGE_NO;
	m_iHealth			= 80000;// no cycler should die

	m_flPlaybackRate	= 1.0f;
	m_flGroundSpeed		= 0;


	SetNextThink( gpGlobals->curtime + 1.0f );

	ResetSequenceInfo( );

	InitBoneControllers();

	/*
	if (GetNumFlexControllers() < 5)
		Warning( "cycler_flex used on model %s without enough flexes.\n", szModel );
	*/

	CreateExpresser();
}

void CFlexExpresser::Think( void )
{
	SetNextThink( gpGlobals->curtime + 0.1f );

	StudioFrameAdvance ( );
}

void CFlexExpresser::InputSpeakResponseConcept( inputdata_t &inputdata )
{
	const char *pInputString = STRING(inputdata.value.StringID());
	// if no params, early out
	if (!pInputString || *pInputString == 0)
	{
		Warning( "empty SpeakResponse input from %s to %s\n", inputdata.pCaller->GetDebugName(), GetDebugName() );
		return;
	}

	char buf[512]; // temporary for tokenizing
	char outputmodifiers[512]; // eventual output to speak
	int outWritten = 0;
	V_strncpy(buf, pInputString, 510);
	buf[511] = 0; // just in case the last character is a comma -- enforce that the 
	// last character in the buffer is always a terminator.
	// special syntax allowing designers to submit inputs with contexts like
	// "concept,context1:value1,context2:value2,context3:value3"
	// except that entity i/o seems to eat commas these days (didn't used to be the case)
	// so instead of commas we have to use spaces in the entity IO, 
	// and turn them into commas here. AWESOME.
	char *pModifiers = const_cast<char *>(V_strnchr(buf, ' ', 510));
	if ( pModifiers )
	{
		*pModifiers = 0;
		++pModifiers;

		// tokenize on spaces
		char *token = strtok(pModifiers, " ");
		while (token)
		{
			// find the start characters for the key and value
			// (seperated by a : which we replace with null)
			char * RESTRICT key = token;
			char * RESTRICT colon = const_cast<char *>(V_strnchr(key, ':', 510)); 
			char * RESTRICT value;
			if (!colon)
			{
				Warning( "faulty context k:v pair in entity io %s\n", pInputString );
				break;
			}

			// write the key and colon to the output string
			int toWrite = colon - key + 1;
			if ( outWritten + toWrite >= 512 )
			{
				Warning( "Speak input to %s had overlong parameter %s", GetDebugName(), pInputString );
				return;
			}
			memcpy(outputmodifiers + outWritten, key, toWrite);
			outWritten += toWrite;

			*colon = 0;
			value = colon + 1;

			// determine if the value is actually a procedural name
			CBaseEntity *pProcedural = gEntList.FindEntityProcedural( value, this, inputdata.pActivator, inputdata.pCaller );

			// write the value to the output -- if it's a procedural name, replace appropriately; 
			// if not, just copy over.
			const char *valString; 
			if (pProcedural)
			{
					valString = STRING(pProcedural->GetEntityName());
				}
			else
			{
				valString = value;
			}
			toWrite = strlen(valString);
			toWrite = MIN( 511-outWritten, toWrite );
			V_strncpy( outputmodifiers + outWritten, valString, toWrite+1 );
			outWritten += toWrite;

			// get the next token
			token = strtok(NULL, " ");
			if (token)
			{
				// if there is a next token, write in a comma
				if (outWritten < 511)
				{
					outputmodifiers[outWritten++]=',';
				}
			}
		}
	}

	// null terminate just in case
	outputmodifiers[outWritten] = 0;

	Speak( buf, outWritten > 0 ? outputmodifiers : NULL );
}

// does nothing. It's important that it does nothing because if it 
// calls through to base class and tries to flinch, it will crash.
int CFlexExpresser::OnTakeDamage( const CTakeDamageInfo &info )
{
	return 0;
}


IResponseSystem *CFlexExpresser::GetResponseSystem( void )
{
	return BaseClass::GetResponseSystem();
}

// because nothing inherits from CFlexExpresser, we can just check classname.
CFlexExpresser * CFlexExpresser::AsFlexExpresser( CBaseEntity *pEntity )
{
	if ( pEntity )
	{
		if ( pEntity->ClassMatches( MAKE_STRING("prop_talker") ) )
		{
			return static_cast< CFlexExpresser * >(pEntity);
		}
	}

	AssertMsg1( pEntity == NULL || dynamic_cast<CFlexExpresser *>(pEntity) == NULL, "%s subclasses prop_talker; update CFlexExpresser::AsFlexExpresser\n", 
		pEntity->GetClassname() );

	return NULL;
}


BEGIN_DATADESC( CFlexExpresser )
DEFINE_INPUTFUNC( FIELD_STRING,	"SpeakResponseConcept",	InputSpeakResponseConcept ),
DEFINE_KEYFIELD( m_flThenAnyMaxDist, FIELD_FLOAT, "maxThenAnyDispatchDist" ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( prop_talker , CFlexExpresser ); 