//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: If you create a control bar, add its ID here. If you want to add it
//			to the View menu, you must make the menu ID the same as the value
//			you assign it here.
//
// $NoKeywords: $
//=============================================================================//

#ifndef CONTROLBARIDS_H
#define CONTROLBARIDS_H
#ifdef _WIN32
#pragma once
#endif

enum
{
	// Must be first!
	IDCB_FIRST = 0xE805,

	IDCB_TEXTUREBROWSER,	// e806
	IDCB_FILTERCONTROL,		// e807
	IDCB_OBJECTBAR,			// e808
	IDCB_MAPTOOLSBAR,		// e809
	IDCB_MAPVIEWBAR,		// e80a
	IDCB_TEXTUREBAR,		// e80b
	IDCB_MAPOPERATIONS,		// e80c
	IDCB_ANIMATIONBAR,		// e80d
    IDCB_DISPEDITTOOLBAR,	// e80e
    IDCB_SELECT_MODE_BAR,	// e80f
	IDCB_UNDO_REDO_BAR = 0xE810,	// e810
	IDCB_MANIFEST_CONTROL,	// e811

	// Must be last!
	IDCB_LAST
};

#endif // CONTROLBARIDS_H
