//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Blixibon
// Purpose:		Complex Q&A used by Bad Cop, Will-E, etc.
//
//=============================================================================//

#ifndef AI_CONCEPT_RESPONSE_H
#define AI_CONCEPT_RESPONSE_H
#pragma once

#include "cbase.h"

#define TLK_CONCEPT_ANSWER "TLK_CONCEPT_ANSWER"

// This macro assumes the following:
// 
//		a. The class has a SetSpeechTarget() method that takes an entity matching the class of the target
//		b. The class has a SpeakIfAllowed() method that takes a concept string and an AI_CriteriaSet.
// 
// Tip: Speech target contexts are appended automatically, so applyContext can be used in the original concept to influence the response.
// 
#define ConceptResponseAnswer(target, concept) \
	if (target) \
	{ \
		AI_CriteriaSet modifiers; \
		SetSpeechTarget(target); \
		modifiers.AppendCriteria("speechtarget_concept", concept); \
		SpeakIfAllowed(TLK_CONCEPT_ANSWER, modifiers); \
	} \

// Uses bRespondingToPlayer in the third parameter
// (important for back-and-forth TLK_CONCEPT_ANSWERs)
#define ConceptResponseAnswer_PlayerAlly(target, concept) \
	if (target) \
	{ \
		AI_CriteriaSet modifiers; \
		SetSpeechTarget(target); \
		modifiers.AppendCriteria("speechtarget_concept", concept); \
		SpeakIfAllowed(TLK_CONCEPT_ANSWER, modifiers, target->IsPlayer()); \
	} \

#endif