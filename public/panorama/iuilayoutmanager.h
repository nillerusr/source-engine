//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef IUILAYOUTMANAGER_H
#define IUILAYOUTMANAGER_H

#ifdef _WIN32
#pragma once
#endif

#include "panoramatypes.h"
#include "panoramasymbol.h"
#include "layout/stylefiletypes.h"

namespace panorama
{

class CStyleAnimation;

class IUILayoutFile
{
public:
	virtual bool BReplaceDefines( CPanoramaSymbol symStyleFile, char *rgchBuffer, uint cubBuffer ) = 0;
	virtual void BuildMatchingStyleList( CUtlVector< CascadeStyleFileInfo_t > &vecStyles, const CPanelIdentifiers &panelID, IUILayoutFile *pPreviousLayoutFile ) = 0;

	virtual CPanoramaSymbol GetStyleFileSymbol( int i ) const = 0;
	virtual CPanoramaSymbol GetLayoutFileSymbol() const = 0;

	virtual const CStyleAnimation *GetAnimation( CPanoramaSymbol symName ) = 0;

	virtual const char *GetDefine( const char *pchName ) = 0;

	virtual bool ApplyMatchedStylesToPanelStyle( IUIPanelStyle *pPanelStyle, const CUtlVector< CascadeStyleFileInfo_t > &vecStyles, EStyleRepaint &eRepaint, bool &bInheritablePropertiesChanged ) = 0;
};

//-----------------------------------------------------------------------------
// Purpose: Public interface to layout mamanger.  Manages layout files, styles, etc.
//-----------------------------------------------------------------------------
class IUILayoutManager 
{
public:

	// in memory updates
	enum EUpdateStyleType
	{
		k_EUpdateStyleStyle = 0,
		k_EUpdateStyleKeyframes = 1,
	};

	virtual IUILayoutFile *GetLayoutFile( const char *pchFile, bool bPartialLayout ) = 0;
	virtual IUILayoutFile *GetLayoutFile( CPanoramaSymbol symPath ) = 0;
	virtual void SaveInMemoryFiles() = 0;
	virtual void RevertInMemoryFiles() = 0;
	virtual bool BHasFilesInMemory() const = 0;
	virtual bool LoadStyleIntoBuffer( CPanoramaSymbol symFile, CUtlBuffer &buffer ) = 0;
	virtual bool UpdateStyleInMemory( EUpdateStyleType eUpdateType, CPanoramaSymbol symStyleFile, uint unLocation, const char *pchUpdatedStyle ) = 0;
	virtual bool BConvertHTTPPathToLocalP4Path( const char *pchFile, CUtlString &strOut ) = 0;
};

} // namespace panorama

#endif // IUILAYOUTMANAGER_H