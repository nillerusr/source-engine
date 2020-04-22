//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_HINT_EVENTS_H
#define C_HINT_EVENTS_H
#ifdef _WIN32
#pragma once
#endif


class CHintData;


typedef enum
{
	HINTEVENT_OBJECT_BUILT_BY_LOCAL_PLAYER=0	// C_HintEvent_ObjectBuiltByLocalPlayer
} HintEventType;


// All hint events derive from this.
class C_HintEvent_Base
{
public:
	// Find out what kind of event this is.
	virtual HintEventType	GetType() = 0;
};


// Fire a global hint event. It goes to all hint types so they can determine if 
// they want to react.
void GlobalHintEvent( C_HintEvent_Base *pEvent );


// Hint callbacks for each type of hint.
void HintEventFn_BuildObject( CHintData *pData, C_HintEvent_Base *pEvent );



// This notifies the hint system that an object has been built by the local player so
// it can disable all further hints referring to objects of this type.
class C_BaseObject;

class C_HintEvent_ObjectBuiltByLocalPlayer : public C_HintEvent_Base
{
public:
							C_HintEvent_ObjectBuiltByLocalPlayer( C_BaseObject *pObj )
							{
								m_pObject = pObj;
							}
	
	virtual HintEventType	GetType()	{ return HINTEVENT_OBJECT_BUILT_BY_LOCAL_PLAYER; }

public:
	C_BaseObject			*m_pObject;	// The object just built.
};


#endif // C_HINT_EVENTS_H
