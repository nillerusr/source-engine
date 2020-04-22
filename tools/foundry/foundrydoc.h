//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef FOUNDRYDOC_H
#define FOUNDRYDOC_H

#ifdef _WIN32
#pragma once
#endif


#include "dme_controls/inotifyui.h"
#include "datamodel/dmehandle.h"


//-----------------------------------------------------------------------------
// Forward declarations 
//-----------------------------------------------------------------------------
class IFoundryDocCallback;
class CDmeVMFEntity;


//-----------------------------------------------------------------------------
// Contains all editable state 
//-----------------------------------------------------------------------------
class CFoundryDoc : public IDmNotify
{
public:
	CFoundryDoc( IFoundryDocCallback *pCallback );
	~CFoundryDoc();

	// Inherited from INotifyUI
	virtual void NotifyDataChanged( const char *pReason, int nNotifySource, int nNotifyFlags );

	// Sets/Gets the file name
	const char *GetBSPFileName();
	const char *GetVMFFileName();
	void SetVMFFileName( const char *pFileName );

	// Dirty bits (has it changed since the last time it was saved?)
	void	SetDirty( bool bDirty );
	bool	IsDirty() const;

	// Saves/loads from file
	bool	LoadFromFile( const char *pFileName );
	void	SaveToFile( );

	// Returns the root object
	CDmElement *GetRootObject();

	// Called when data changes (see INotifyUI for flags)
	void	OnDataChanged( const char *pReason, int nNotifySource, int nNotifyFlags );

	// Create a text block the engine can parse containing the entity data to spawn
	const char* GenerateEntityData( const char *pActualEntityData );

	// Returns the entity list
	CDmAttribute *GetEntityList();

	// Deletes an entity
	void DeleteEntity( CDmeVMFEntity *pEntity );

private:
	// Always copy the worldspawn and other entities that had data built into them by VBSP out
	void AddOriginalEntities( CUtlBuffer &entityBuf, const char *pActualEntityData );

	// Copy in other entities from the editable VMF
	void AddVMFEntities( CUtlBuffer &entityBuf, const char *pActualEntityData );

	IFoundryDocCallback *m_pCallback;
	CDmeHandle< CDmElement > m_hRoot;
	char m_pBSPFileName[512];
	char m_pVMFFileName[512];
	bool m_bDirty;
};


#endif // FOUNDRYDOC_H
