//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef PETDOC_H
#define PETDOC_H

#ifdef _WIN32
#pragma once
#endif


#include "dme_controls/inotifyui.h"
#include "datamodel/dmehandle.h"
#include "datamodel/dmelement.h"

//-----------------------------------------------------------------------------
// Forward declarations 
//-----------------------------------------------------------------------------
class IPetDocCallback;
class CPetDoc;
class CDmeParticleSystemDefinition;


//-----------------------------------------------------------------------------
// The file format for particle system definitions 
//-----------------------------------------------------------------------------
#define PET_FILE_FORMAT "pcf"


typedef CDmrElementArray<CDmeParticleSystemDefinition> CDmrParticleSystemList;


//-----------------------------------------------------------------------------
// Contains all editable state 
//-----------------------------------------------------------------------------
class CPetDoc : public IDmNotify, CBaseElementPropertiesChoices
{
public:
	CPetDoc( IPetDocCallback *pCallback );
	~CPetDoc();

	// Inherited from INotifyUI
	virtual void NotifyDataChanged( const char *pReason, int nNotifySource, int nNotifyFlags );
	virtual bool GetIntChoiceList( const char *pChoiceListType, CDmElement *pElement, 
		const char *pAttributeName, bool bArrayElement, IntChoiceList_t &list );

	// Sets/Gets the file name
	const char *GetFileName();
	void SetFileName( const char *pFileName );

	// Dirty bits (has it changed since the last time it was saved?)
	void	SetDirty( bool bDirty );
	bool	IsDirty() const;

	// Creates a new document
	void	CreateNew();

	// Saves/loads from file
	bool	LoadFromFile( const char *pFileName );
	void	SaveToFile( );

	// Returns the root object
	CDmElement *GetRootObject();

	// Returns the root object fileid
	DmFileId_t GetFileId();

	// Called when data changes (see INotifyUI for flags)
	void	OnDataChanged( const char *pReason, int nNotifySource, int nNotifyFlags );

	// Returns the particle system definition list
	CDmAttribute *GetParticleSystemDefinitionList();

	// add a new definition we've created
	void AddNewParticleSystemDefinition( CDmeParticleSystemDefinition *pNew, 
										 CUndoScopeGuard &Guard );

	// Adds a new particle system definition
	CDmeParticleSystemDefinition *AddNewParticleSystemDefinition( const char *pName );

	// Deletes a particle system definition
	void DeleteParticleSystemDefinition( CDmeParticleSystemDefinition *pParticleSystem );

	// find particle system def by name
	CDmeParticleSystemDefinition *FindParticleSystemDefinition( const char *pName );

	// Replace any particle system with the same name as the passed-in definition
	// with the passed-in definition
	void ReplaceParticleSystemDefinition( CDmeParticleSystemDefinition *pParticleSystem );

	// Does a particle system exist already?
	bool IsParticleSystemDefined( const char *pName );

	// For element choice lists. Return false if it's an unknown choice list type
	virtual bool GetStringChoiceList( const char *pChoiceListType, CDmElement *pElement, 
		const char *pAttributeName, bool bArrayElement, StringChoiceList_t &list );
	virtual bool GetElementChoiceList( const char *pChoiceListType, CDmElement *pElement, 
		const char *pAttributeName, bool bArrayElement, ElementChoiceList_t &list );

	// Updates a specific particle defintion
	void UpdateParticleDefinition( CDmeParticleSystemDefinition *pDef );

	// Update all particle definitions
	void UpdateAllParticleSystems( );

private:
	// Creates the root element
	bool CreateRootElement();

	IPetDocCallback *m_pCallback;
	CDmeHandle< CDmElement > m_hRoot;
	char m_pFileName[MAX_PATH];
	bool m_bDirty;
};


#endif // PETDOC_H
