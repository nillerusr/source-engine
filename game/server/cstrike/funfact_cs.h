//========= Copyright Valve Corporation, All rights reserved. ============//
#ifndef INCLUDED_funfact_cs
#define INCLUDED_funfact_cs
#pragma once

#include "cs_player.h"

struct FunFact
{
	FunFact() :
		id(-1),
		szLocalizationToken(NULL),
		iPlayer(0),
		iData1(0),
		iData2(0),
		iData3(0),
		fMagnitude(0.0f)
		{}
	int id;
	const char* szLocalizationToken;
	int iPlayer;
	int iData1;
	int iData2;
	int iData3;
	float fMagnitude;
};

typedef CUtlVector<FunFact> FunFactVector;

class FunFactEvaluator
{
	DECLARE_CLASS_NOBASE( FunFactEvaluator );
public:
	FunFactEvaluator( int id, const char* szLocalizationToken, float fCoolness ) :
		m_id(id),
		m_pLocalizationToken(szLocalizationToken),
		m_fCoolness(fCoolness)
	{}

	virtual ~FunFactEvaluator() {}

	int GetId() const { return m_id; }
	const char* GetLocalizationToken() const { return m_pLocalizationToken; }
	float GetCoolness() const { return m_fCoolness; }

	virtual bool Evaluate( FunFactVector& results ) const = 0;

private:
	int  m_id;
	const char* m_pLocalizationToken;
	float m_fCoolness;
};


typedef FunFactEvaluator* (*funfactCreateFunc) (void);
class CFunFactHelper
{
public:
	CFunFactHelper ( funfactCreateFunc createFunc )
	{
		m_pfnCreate = createFunc;
		m_pNext = s_pFirst;
		s_pFirst = this;
	}
	funfactCreateFunc m_pfnCreate;
	CFunFactHelper *m_pNext;
	static CFunFactHelper *s_pFirst;
};

#endif // INCLUDED_funfact_cs

