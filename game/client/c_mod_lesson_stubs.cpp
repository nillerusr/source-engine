//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Stub for custom mod lesson actions. 
//				This is so that mods can do actions 
//				Remove this file from the mod's vpc and include your own.
//
//=============================================================================//

#include "cbase.h"

#include "c_gameinstructor.h"
#include "c_baselesson.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


extern ConVar gameinstructor_verbose;


enum Mod_LessonAction
{
	// Enum starts from end of LessonAction
	LESSON_ACTION_MOD_CUSTOM_ACTION_STUB = LESSON_ACTION_MOD_START,
	
	LESSON_ACTION_TOTAL
};


void CScriptedIconLesson::Mod_PreReadLessonsFromFile( void )
{
	// Add custom actions to the map
	CScriptedIconLesson::LessonActionMap.Insert( "custom action stub", LESSON_ACTION_MOD_CUSTOM_ACTION_STUB );
}


bool CScriptedIconLesson::Mod_ProcessElementAction( int iAction, bool bNot, const char *pchVarName, EHANDLE &hVar, const CGameInstructorSymbol *pchParamName, float fParam, C_BaseEntity *pParam, const char *pchParam, bool &bModHandled )
{
	// Assume we're going to handle the action
	bModHandled = true;

	C_BaseEntity *pVar;
	pVar = hVar.Get();

	switch ( iAction )
	{
		case LESSON_ACTION_MOD_CUSTOM_ACTION_STUB:
		{
			float flStub = 0.0f; //pVar->GetStubValue();

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->GetStubValue() ", pchVarName );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%.1f ", flStub );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( ">= [%s] " ) : ( "< [%s] " ), pchParamName->String() );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%.1f\n", fParam );
			}

			return ( flStub ) ? ( flStub >= fParam ) : ( flStub < fParam );
		}

		default:
			// Didn't handle this action
			bModHandled = false;
			break;
	}

	return false;
}


bool C_GameInstructor::Mod_HiddenByOtherElements( void )
{
	return false;
}
