//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANORAMA_STYLEFILETYPES_H
#define PANORAMA_STYLEFILETYPES_H

#ifdef _WIN32
#pragma once
#endif

#include "../iuipanel.h"

namespace panorama
{
class IUILayoutFile;
class IUIPanel;

//-----------------------------------------------------------------------------
// Purpose: Validate statics
//-----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
	void ValidateStylePropertyFactory( CValidator &validator );
#endif

//-----------------------------------------------------------------------------
// Purpose: Used to sort styles to apply by cascade order
//-----------------------------------------------------------------------------
struct StyleFromFile_t;
class CLayoutFile;
struct CascadeStyleFileInfo_t
{
	const StyleFromFile_t *m_pStyleFromFile;
	panorama::IUILayoutFile *m_pLayoutFile;		// layout file
	uint m_iStyleFile;							// layout file index	
	uint m_unSelectorSpecificity;				// score for this selector (high = overrides lower valued selectors)
};

//-----------------------------------------------------------------------------
// Purpose: All the info needed to identify a panel. Used when looking up a style w/o a IUIPanel*
//-----------------------------------------------------------------------------
class CPanelIdentifiers
{
public:
	CPanelIdentifiers();
	CPanelIdentifiers( IUIPanel *pPanel );

	CPanoramaSymbol m_symPanelType;
	uint m_unStyleFlags;
	const CPanoramaSymbol *m_psymClasses;
	uint m_csymClasses;
	const char *m_pchID;
	bool m_bTreatPanelAsParent;
	IUIPanel *m_pPanel;
};

} // namespace panorama


#endif //PANORAMA_STYLEFILETYPES_H
