//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef ANIMSETATTRIBUTEVALUE_H
#define ANIMSETATTRIBUTEVALUE_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utldict.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmAttribute;


//-----------------------------------------------------------------------------
// AnimationControlType
//-----------------------------------------------------------------------------
enum AnimationControlType_t
{
	ANIM_CONTROL_INVALID = -1,

	ANIM_CONTROL_VALUE = 0,
	ANIM_CONTROL_BALANCE,
	ANIM_CONTROL_MULTILEVEL,

	ANIM_CONTROL_COUNT,
};


struct AttributeValue_t
{
	AttributeValue_t()
	{
		// Default values
		m_pValue[ANIM_CONTROL_VALUE] = 0.0f;
		m_pValue[ANIM_CONTROL_BALANCE] = 0.5f;
		m_pValue[ANIM_CONTROL_MULTILEVEL] = 0.5f;
	}

	float m_pValue[ANIM_CONTROL_COUNT];
};

struct AnimationControlAttributes_t : public AttributeValue_t
{
	AnimationControlAttributes_t()
	{
		// Default values
		m_pAttribute[ANIM_CONTROL_VALUE] = 0;
		m_pAttribute[ANIM_CONTROL_BALANCE] = 0;
		m_pAttribute[ANIM_CONTROL_MULTILEVEL] = 0;
	}

	CDmAttribute* m_pAttribute[ANIM_CONTROL_COUNT];
};

typedef CUtlDict< AnimationControlAttributes_t, unsigned short > AttributeDict_t;

#endif // ANIMSETATTRIBUTEVALUE_H