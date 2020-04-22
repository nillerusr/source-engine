//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Act busy tool; main UI smarts class
//
//=============================================================================

#ifndef ACTBUSYTOOL_H
#define ACTBUSYTOOL_H

#ifdef _WIN32
#pragma once
#endif


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmeEditorTypeDictionary;


//-----------------------------------------------------------------------------
// Singleton interfaces
//-----------------------------------------------------------------------------
extern CDmeEditorTypeDictionary *g_pEditorTypeDict;


//-----------------------------------------------------------------------------
// Allows the doc to call back into the act busy tool
//-----------------------------------------------------------------------------
class IActBusyDocCallback
{
public:
	// Called by the doc when the data changes
	virtual void OnDocChanged( const char *pReason, int nNotifySource, int nNotifyFlags ) = 0;
};


#endif // ACTBUSYTOOL_H
