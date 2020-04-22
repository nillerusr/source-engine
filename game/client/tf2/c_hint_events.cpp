//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_hint_events.h"
#include "c_tf_hints.h"
#include "c_tf_hintmanager.h"
#include <KeyValues.h>
#include "c_baseobject.h"


void GlobalHintEvent( C_HintEvent_Base *pEvent )
{
	// Call the static registered functions for each hint type.
	for ( int i=0; i < GetNumHintDatas(); i++ )
	{
		CHintData *pData = GetHintData( i );
		if ( pData && pData->m_pEventFn )
			pData->m_pEventFn( pData, pEvent );
	}
}


void HintEventFn_BuildObject( CHintData *pData, C_HintEvent_Base *pEvent )
{
	if ( pEvent->GetType() == HINTEVENT_OBJECT_BUILT_BY_LOCAL_PLAYER )
	{
		C_BaseObject *pObj = ((C_HintEvent_ObjectBuiltByLocalPlayer*)pEvent)->m_pObject;
		
		if ( pObj->GetType() == pData->m_ObjectType )
		{
			// Ok, they just built the object that any hints of this type are referring to, so disable 
			// all further hints of this type.
			KeyValues *pkvStats = GetHintDisplayStats();
			if ( pkvStats )
			{
				KeyValues *pkvStatSection = pkvStats->FindKey( pData->name, true );
				if ( pkvStatSection )
				{
					pkvStatSection->SetString( "times_shown", VarArgs( "%i", 100 ) );
				}
			}
		}
	}
}

