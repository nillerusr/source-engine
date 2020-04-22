//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: VMT tool; main UI smarts class
//
//=============================================================================

#ifndef VMTTOOL_H
#define VMTTOOL_H

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
// Allows the doc to call back into the VMT editor tool
//-----------------------------------------------------------------------------
class IVMTDocCallback
{
public:
	// Called by the doc when the data changes
	virtual void OnDocChanged( const char *pReason, int nNotifySource, int nNotifyFlags ) = 0;

	// Update the editor dict based on the current material parameters
	virtual void AddShaderParameter( const char *pParam, const char *pWidget, const char *pTextType ) = 0;

	// Update the editor dict based on the current material parameters
	virtual void RemoveShaderParameter( const char *pParam ) = 0;

	// Adds flags, tool parameters
	virtual void AddFlagParameter( const char *pParam ) = 0;
	virtual void AddToolParameter( const char *pParam, const char *pWidget = NULL, const char *pTextType = NULL ) = 0;
	virtual void RemoveAllFlagParameters() = 0;
	virtual void RemoveAllToolParameters() = 0;
};


#endif // VMTTOOL_H
