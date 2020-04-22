//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef COMMEDITDOC_H
#define COMMEDITDOC_H

#ifdef _WIN32
#pragma once
#endif


#include "dme_controls/inotifyui.h"
#include "datamodel/dmehandle.h"
#include "datamodel/dmelement.h"

//-----------------------------------------------------------------------------
// Forward declarations 
//-----------------------------------------------------------------------------
class ICommEditDocCallback;
class CCommEditDoc;
class CDmeCommentaryNodeEntity;

typedef CDmrElementArray<CDmeCommentaryNodeEntity> CDmrCommentaryNodeEntityList;


//-----------------------------------------------------------------------------
// Contains all editable state 
//-----------------------------------------------------------------------------
class CCommEditDoc : public IDmNotify
{
public:
	CCommEditDoc( ICommEditDocCallback *pCallback );
	~CCommEditDoc();

	// Inherited from INotifyUI
	virtual void NotifyDataChanged( const char *pReason, int nNotifySource, int nNotifyFlags );

	// Sets/Gets the file name
	const char *GetTXTFileName();
	void SetTXTFileName( const char *pFileName );

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

	// Returns the entity list
	CDmAttribute *GetEntityList();

	// Adds a new info_target
	void AddNewInfoTarget( void );
 	void AddNewInfoTarget( const Vector &vecOrigin, const QAngle &angAngles );

	// Adds a new commentary node
	void AddNewCommentaryNode( void );
	void AddNewCommentaryNode( const Vector &vecOrigin, const QAngle &angAngles );

	// Deletes a commentary node
	void DeleteCommentaryNode( CDmElement *pNode );

	// Returns the commentary node at the specified location
	CDmeCommentaryNodeEntity *GetCommentaryNodeForLocation( Vector &vecOrigin, QAngle &angAbsAngles );

	// For element choice lists. Return false if it's an unknown choice list type
	virtual bool GetStringChoiceList( const char *pChoiceListType, CDmElement *pElement, 
		const char *pAttributeName, bool bArrayElement, StringChoiceList_t &list );
	virtual bool GetElementChoiceList( const char *pChoiceListType, CDmElement *pElement, 
		const char *pAttributeName, bool bArrayElement, ElementChoiceList_t &list );

private:
	ICommEditDocCallback *m_pCallback;
	CDmeHandle< CDmElement > m_hRoot;
	char m_pTXTFileName[512];
	bool m_bDirty;
};


#endif // COMMEDITDOC_H
