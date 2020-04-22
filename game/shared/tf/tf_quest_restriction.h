//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_QUEST_RESTRICTION_H
#define TF_QUEST_RESTRICTION_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_shareddefs.h"

#ifdef CLIENT_DLL
#define CTFPlayer C_TFPlayer
#endif // CLIENT_DLL

class CTFQuestEvaluator;
class CTFPlayer;

enum EInvalidReasons_t
{
	INVALID_QUEST_REASON_WRONG_MAP = 0,
	INVALID_QUEST_REASON_WRONG_CLASS,
	INVALID_QUEST_REASON_WRONG_GAME_MODE,
	INVALID_QUEST_REASON_NOT_ENOUGH_PLAYERS,
	INVALID_QUEST_REASON_VALVE_SERVERS_ONLY,
	INVALID_QUEST_REASON_NO_MVM,

	NUM_INVALID_REASONS,
};

struct InvalidReason
{
	InvalidReason() {}

	bool IsValid() const 
	{ 
		return m_bits.IsAllClear();	
	}

	CBitVec< NUM_INVALID_REASONS > m_bits;
};

typedef InvalidReason InvalidReasonsContainer_t;

void GetInvalidReasonsNames( const InvalidReasonsContainer_t&, CUtlVector< CUtlString >& vecStrings );

//-----------------------------------------------------------------------------
// Purpose: base quest condition
//-----------------------------------------------------------------------------
class CTFQuestCondition
{
public:
	DECLARE_CLASS_NOBASE( CTFQuestCondition )

	CTFQuestCondition();
	virtual ~CTFQuestCondition();

	virtual const char *GetBaseName() const = 0;
	virtual const char *GetConditionName() const { return m_pszTypeName; }
	virtual const char *GetValueString() const { return ""; }
	virtual bool BInitFromKV( KeyValues *pKVItem, CUtlVector<CUtlString> *pVecErrors /* = NULL */ );

	virtual void PrintDebugText() const;

	virtual const CTFPlayer *GetQuestOwner() const;
	virtual bool IsValidForPlayer( const CTFPlayer *pOwner, InvalidReasonsContainer_t& invalidReasons ) const;

	virtual bool IsOperator() const { return false; }
	virtual bool IsEvaluator() const { return false; }

	void SetParent( CTFQuestCondition *pParent ) { m_pParent = pParent; }
	CTFQuestCondition *GetParent() const { return m_pParent; }

	virtual CTFQuestCondition* AddChildByName( const char *pszChildName ) { Assert( 0 );  return NULL; }

	virtual int GetChildren( CUtlVector< CTFQuestCondition* >& vecChildren ) { return 0; }
	virtual bool RemoveAndDeleteChild( CTFQuestCondition *pChild ) { return false; }

	virtual void GetRequiredParamKeys( KeyValues *pRequiredKeys ) {}
	virtual void GetOutputKeyValues( KeyValues *pOutputKeys ) {}

	virtual void GetValidTypes( CUtlVector< const char* >& vecOutValidChildren ) const = 0;
	virtual void GetValidChildren( CUtlVector< const char* >& vecOutValidChildren ) const = 0;

	virtual int GetMaxInputCount() const { return 0; }

	virtual const char* GetEventName() const { Assert( 0 ); return NULL; }
	virtual void SetEventName( const char *pszEventName ) { Assert( 0 ); }

	void SetFieldName( const char* pszFieldName ) { m_pszFieldName = pszFieldName; }
	void SetTypeName( const char* pszTypeName ) { m_pszTypeName = pszTypeName; }

protected:

	void GetValidRestrictions( CUtlVector< const char* >& vecOutValidChildren ) const;
	void GetValidEvaluators( CUtlVector< const char* >& vecOutValidChildren ) const;

	const char *m_pszFieldName;
	const char *m_pszTypeName;

private:

	CTFQuestCondition *m_pParent;
};


//-----------------------------------------------------------------------------
// Purpose: base quest restriction
//-----------------------------------------------------------------------------
class CTFQuestRestriction : public CTFQuestCondition
{
public:
	DECLARE_CLASS( CTFQuestRestriction, CTFQuestCondition )

	virtual const char *GetBaseName() const OVERRIDE { return "restriction"; }
	virtual bool PassesRestrictions( IGameEvent *pEvent ) const = 0;
	void SetEventName( const char *pszEventName ) OVERRIDE { m_pszEventName = pszEventName; }
	virtual const char* GetEventName() const OVERRIDE { return m_pszEventName; }

	virtual void GetValidTypes( CUtlVector< const char* >& vecOutValidChildren ) const OVERRIDE;
	virtual void GetValidChildren( CUtlVector< const char* >& vecOutValidChildren ) const OVERRIDE;

protected:

	const char *m_pszEventName;
};


//-----------------------------------------------------------------------------
// Purpose: base quest evaluator
//-----------------------------------------------------------------------------
class CTFQuestEvaluator : public CTFQuestCondition
{
public:
	CTFQuestEvaluator();

	virtual const char *GetBaseName() const OVERRIDE { return "evaluator"; }

	virtual bool IsEvaluator() const OVERRIDE { return true; }

	virtual void EvaluateCondition( CTFQuestEvaluator *pSender, int nScore ) = 0;
	virtual void ResetCondition() = 0;

	virtual void GetRequiredParamKeys( KeyValues *pRequiredKeys ) OVERRIDE;
	virtual void GetOutputKeyValues( KeyValues *pOutputKeys ) OVERRIDE;

	virtual void GetValidTypes( CUtlVector< const char* >& vecOutValidChildren ) const OVERRIDE;
	virtual void GetValidChildren( CUtlVector< const char* >& vecOutValidChildren ) const OVERRIDE;

	void SetAction( const char *pszAction ) { m_pszAction = pszAction; }
	const char *GetAction() const { return m_pszAction; }

private:
	const char *m_pszAction;
};

CTFQuestRestriction *CreateRestrictionByName( const char *pszName, CTFQuestCondition* pParent );
CTFQuestEvaluator *CreateEvaluatorByName( const char *pszName, CTFQuestCondition*);

#endif // TF_QUEST_RESTRICTION_H
