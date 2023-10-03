//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef FLEXCYCLER_H
#define FLEXCYCLER_H
#ifdef _WIN32
#pragma once
#endif

#include "baseflex.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

class CFlexCycler : public CBaseFlex
{
private:
	DECLARE_CLASS( CFlexCycler, CBaseFlex );
public:
	DECLARE_DATADESC();

	CFlexCycler() { m_iszSentence = NULL_STRING; m_sentence = 0; }
	void GenericCyclerSpawn(char *szModel, Vector vecMin, Vector vecMax);
	virtual int	ObjectCaps( void ) { return (BaseClass::ObjectCaps() | FCAP_IMPULSE_USE); }
	int OnTakeDamage( const CTakeDamageInfo &info );
	void Spawn( void );
	void Think( void );

	virtual void ProcessSceneEvents( void );

	// Don't treat as a live target
	virtual bool IsAlive( void ) { return FALSE; }

	float m_flextime;
	LocalFlexController_t m_flexnum;
	float m_flextarget[64];
	float m_blinktime;
	float m_looktime;
	Vector m_lookTarget;
	float m_speaktime;
	int	m_istalking;
	int	m_phoneme;

	string_t m_iszSentence;
	int m_sentence;

	void SetFlexTarget( LocalFlexController_t flexnum );
	LocalFlexController_t LookupFlex( const char *szTarget );
};

//
// we should get rid of all the other cyclers and replace them with this.
//
class CGenericFlexCycler : public CFlexCycler
{
public:
	DECLARE_CLASS( CGenericFlexCycler, CFlexCycler );

	void Spawn( void ) { GenericCyclerSpawn( (char *)STRING( GetModelName() ), Vector(-16, -16, 0), Vector(16, 16, 72) ); }
};


#endif // FLEXCYCLER_H
