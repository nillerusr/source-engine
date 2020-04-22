//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef ACTBUSYDOC_H
#define ACTBUSYDOC_H

#ifdef _WIN32
#pragma once
#endif


#include "dme_controls/inotifyui.h"
#include "datamodel/dmehandle.h"


//-----------------------------------------------------------------------------
// Forward declarations 
//-----------------------------------------------------------------------------
class IActBusyDocCallback;


//-----------------------------------------------------------------------------
// Contains all editable state 
//-----------------------------------------------------------------------------
class CActBusyDoc : public IDmNotify
{
public:
	CActBusyDoc( IActBusyDocCallback *pCallback );
	~CActBusyDoc();

	// Inherited from INotifyUI
	virtual void NotifyDataChanged( const char *pReason, int nNotifySource, int nNotifyFlags );

	// Sets/Gets the file name
	const char *GetFileName();
	void SetFileName( const char *pFileName );

	// Dirty bits (has it changed since the last time it was saved?)
	void	SetDirty( bool bDirty );
	bool	IsDirty() const;

	// Creates a new act busy list
	void	CreateNew();

	// Saves/loads from file
	bool	LoadFromFile( const char *pFileName );
	void	SaveToFile( );

	// Returns the root object
	CDmElement *GetRootObject();

	// Called when data changes
	void	OnDataChanged( const char *pReason, int nNotifySource, int nNotifyFlags );

	// Creates a new actbusy
	void	CreateActBusy();

private:
	IActBusyDocCallback *m_pCallback;
	CDmeHandle< CDmElement > m_hRoot;
	char m_pFileName[512];
	bool m_bDirty;
};


#endif // ACTBUSYDOC_H
