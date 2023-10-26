//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: A CBaseFlex, with an Expresser so that it can 
//
// $NoKeywords: $
//=============================================================================//

#ifndef FLEX_EXPRESSER_H
#define FLEX_EXPRESSER_H
#ifdef _WIN32
#pragma once
#endif

#include "flexcycler.h"
#include "ai_speech.h"

#define FCYCLER_NOTSOLID 1

class CFlexExpresserShim : public CFlexCycler
{
public:
	inline CAI_Expresser *GetExpresser( void ) { return m_pExpresser; }
	inline const CAI_Expresser *GetMultiplayerExpresser( void ) const { return m_pExpresser; }

protected:
	CAI_Expresser *m_pExpresser;
};


class CFlexExpresser : public CAI_ExpresserHost<CFlexExpresserShim>
{
	DECLARE_CLASS( CFlexExpresser, CAI_ExpresserHost<CFlexExpresserShim> );

public:
	DECLARE_DATADESC();
	CFlexExpresser();
	~CFlexExpresser();

	inline CAI_Expresser *GetExpresser( void ) { return m_pExpresser; }
	inline const CAI_Expresser *GetMultiplayerExpresser( void ) const { return m_pExpresser; }
	virtual IResponseSystem *GetResponseSystem( void );
	virtual void Spawn();
	void Think( void );
	virtual int OnTakeDamage( const CTakeDamageInfo &info ); //< stubbed out, does nothing, flexexpressers are immortal
	void InputSpeakResponseConcept( inputdata_t &inputdata );

	/// given a pointer to a CBaseEntity, return a CFlexExpresser * if that base entity is a prop_talker, NULL otherwise
	/// (hopefully faster than dynamic_cast). Passing NULL is safe.
	static CFlexExpresser *AsFlexExpresser( CBaseEntity *pEntity );

	float m_flThenAnyMaxDist; //< if nonzero, override the maximum dispatch distance for a THEN ANY followup.
protected:
	CAI_Expresser *CreateExpresser( void );

};




#endif // FLEX_EXPRESSER_H
