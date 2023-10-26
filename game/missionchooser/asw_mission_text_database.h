#ifndef _INCLUDED_ASW_MISSION_TEXT_DATABASE_H
#define _INCLUDED_ASW_MISSION_TEXT_DATABASE_H

#ifdef _WIN32
#pragma once
#endif

#include "utlsymbol.h"
#include "utlvector.h"
#include "utllinkedlist.h"
#include "utlstring.h"
#include "convar.h"
#include "missionchooser/iasw_mission_chooser.h"

typedef CUtlSymbol CUtlSymbolMsnTxt;

/// one possible text line
class CASW_MissionTextSpec
{
public:
	/// Call this to just get the text field for a description out of the missionspec
	inline const char *GetShortDescription() const ;
	inline const char *GetLongDescription() const ;


	/// mandatory fields
	CUtlSymbolMsnTxt m_nObjectiveEntityName; 

	/// optional fields
	CUtlSymbolMsnTxt m_nMissionFilename;   // an overall mission name

	/// payload
	CUtlString m_sShortDesc;
	CUtlString m_sLongDesc;

	/// a unique identifier for each mission spec. 
	/// if they're loaded from the same files in the same
	/// order by the database on both server and client,
	/// we can refer to specs by this id across the network.
	// if you change the size of this, also update the m_nObjectiveTextIdx
	// datatable field in asw_objective.cpp. 
	typedef IASW_Mission_Text_Database::ID_t ID_t;
	ID_t m_nId;
	enum { INVALID_INDEX = IASW_Mission_Text_Database::INVALID_INDEX };

	inline bool IsValid() const { return m_nObjectiveEntityName.IsValid(); }

	CASW_MissionTextSpec( ) {};
	inline CASW_MissionTextSpec(  const char *pszShortDescription,
		const char *pszSLongDescription,
		const char *pszObjectiveEntityName,
		const char *pszMissionFilename,
		const ID_t &nId );
	inline void Init(   const char *pszShortDescription,
		const char *pszSLongDescription,
				const char *pszObjectiveEntityName,
				const char *pszMissionFilename,
				const ID_t &nId  );

	static CUtlSymbolMsnTxt SymbolForMissionFilename( const char *pMissionFilename, bool bCreateIfNotFound );
	/// get symbol table for CUtlSymbolMsnTxts
	inline static CUtlSymbolTable &SymTab();
};

class CASW_MissionTextDB : public IASW_Mission_Text_Database
{
public:

	/// load specs from given filename and add it to internal data
	void LoadKeyValuesFile( const char *pFilename );

	/// Delete internal structures
	void Reset();

	/// num of specs in total
	inline int Count(); 

	/// Find the best mission text spec meeting the given parameters. Returns NULL if none found.
	const CASW_MissionTextSpec *Find( const char *pObjectiveEntityName, const char *pMissionFilename = NULL );

	// local symbol table
	static CUtlSymbolTable s_SymTab;
	static inline CUtlSymbolTable &SymTab() { return s_SymTab; } 

	CASW_MissionTextSpec * GetSpecById( CASW_MissionTextSpec::ID_t nId );

	// from IASW_Mission_Text_Database
	virtual const char *GetShortDescriptionByID( unsigned int id ) ;	
	virtual const char *GetLongDescriptionByID( unsigned int id )  ;

	/// find the mission text id for a given entity name, mission filename, other criteria.
	/// you can resolve the ID to textual descriptions with GetShortDescriptionByID() etc.
	virtual ID_t FindMissionTextID( const char *pEntityName, const char *pMissionName );

protected:
	void AddSpec(  const char *pszShortDescription,
		const char *pszSLongDescription,
		const char *pszObjectiveEntityName,
		const char *pszMissionFilename );

	/// @todo replace with hash
	CUtlVector<CASW_MissionTextSpec> m_missionSpecs;
};



inline CASW_MissionTextSpec::CASW_MissionTextSpec(  const char *pszShortDescription,
					 const char *pszSLongDescription,
					 const char *pszObjectiveEntityName,
					 const char *pszMissionFilename,
					 const ID_t &nId  ) 
{
	Init( pszShortDescription, pszSLongDescription, pszObjectiveEntityName, pszMissionFilename, nId );
}

int CASW_MissionTextDB::Count()
{
	return m_missionSpecs.Count();
}



void CASW_MissionTextSpec::Init(  const char *pszShortDescription,
								const char *pszSLongDescription,
										   const char *pszObjectiveEntityName,
										   const char *pszMissionFilename,
										   const ID_t &nId  )
{
	AssertMsg( nId < 65534, "MissionTextSpec IDs have exceeded representable range. Expand CASW_MissionTextSpec::ID_t to 32 bits." );

	m_sShortDesc.Set( pszShortDescription );
	m_sLongDesc.Set( pszSLongDescription );
	m_nObjectiveEntityName = SymTab().AddString( pszObjectiveEntityName );
	m_nMissionFilename = SymbolForMissionFilename( pszMissionFilename, true );
	m_nId = nId;
}


const char *CASW_MissionTextSpec::GetShortDescription() const
{
	return m_sShortDesc.String();
}

const char *CASW_MissionTextSpec::GetLongDescription() const
{
	return m_sLongDesc.String();
}


inline CUtlSymbolTable &CASW_MissionTextSpec::SymTab() 
{ 
	return CASW_MissionTextDB::s_SymTab; 
}



extern CASW_MissionTextDB g_MissionTextDatabase;
#endif //_INCLUDED_ASW_MISSION_TEXT_DATABASE_H