//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_TF_HINTS_H
#define C_TF_HINTS_H
#ifdef _WIN32
#pragma once
#endif


class C_HintEvent_Base;
class CHintData;

// The HINTNAME_t structures are where data and code are registered for each hint type.
typedef void (*HintEventFn)( CHintData *pData, C_HintEvent_Base *pHint );


class CHintData
{
public:
	char			*name;
	int				id;
	int				timesseen;
	
	HintEventFn		m_pEventFn;
	int				m_ObjectType;	// If this is a hint about an object, this is the object type.
};


extern int			GetNumHintDatas();
extern CHintData*	GetHintData( int i );


#endif // C_TF_HINTS_H
