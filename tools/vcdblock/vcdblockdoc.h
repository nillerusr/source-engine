//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef VCDBLOCKDOC_H
#define VCDBLOCKDOC_H

#ifdef _WIN32
#pragma once
#endif


#include "dme_controls/inotifyui.h"
#include "datamodel/dmehandle.h"
#include "datamodel/dmelement.h"


//-----------------------------------------------------------------------------
// Forward declarations 
//-----------------------------------------------------------------------------
class IVcdBlockDocCallback;
class CVcdBlockDoc;
class CDmeVMFEntity;


//-----------------------------------------------------------------------------
// Contains all editable state 
//-----------------------------------------------------------------------------
class CVcdBlockDoc : public IDmNotify
{
public:
	CVcdBlockDoc( IVcdBlockDocCallback *pCallback );
	~CVcdBlockDoc();

	// Inherited from INotifyUI
	virtual void NotifyDataChanged( const char *pReason, int nNotifySource, int nNotifyFlags );

	// Sets/Gets the file name
	const char *GetBSPFileName();
	const char *GetVMFFileName();
	const char *GetEditFileName();
	void SetVMFFileName( const char *pFileName );
	void SetEditFileName( const char *pFileName );

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
	void	ServerLevelInitPostEntity( void );

	// Returns the entity list
	CDmAttribute *GetEntityList();

	// Adds a new info_target
	void AddNewInfoTarget( void );
 	void AddNewInfoTarget( const Vector &vecOrigin, const QAngle &angAngles );

	// Deletes a commentary node
	void DeleteInfoTarget( CDmeVMFEntity *pNode );

	// Returns the commentary node at the specified location
	CDmeVMFEntity *GetInfoTargetForLocation( Vector &vecOrigin, QAngle &angAbsAngles );

	// Returns the info target that's closest to this line
	CDmeVMFEntity *GetInfoTargetForLocation( Vector &vecStart, Vector &vecEnd );

	// For element choice lists. Return false if it's an unknown choice list type
	virtual bool GetStringChoiceList( const char *pChoiceListType, CDmElement *pElement, 
		const char *pAttributeName, bool bArrayElement, StringChoiceList_t &list );
	virtual bool GetElementChoiceList( const char *pChoiceListType, CDmElement *pElement, 
		const char *pAttributeName, bool bArrayElement, ElementChoiceList_t &list );


	void VerifyAllEdits( const CDmrElementArray<> &entityList );
	void InitializeFromServer( CDmrElementArray<> &entityList );

	bool CopyEditsToVMF( void );

	bool RememberPlayerPosition( void );

private:
	IVcdBlockDocCallback *m_pCallback;
	CDmeHandle< CDmElement > m_hVMFRoot; // VMF file
	CDmeHandle< CDmElement > m_hEditRoot; // VMF Edits file
	char m_pBSPFileName[512];
	char m_pVMFFileName[512];
	char m_pEditFileName[512];
	bool m_bDirty;
};



#endif // VCDBLOCKDOC_H
