//========= Copyright ??? 1996-2006, Valve LLC, All rights reserved. ============
//
// Purpose: Shared memory for panorama communications.
//
// $NoKeywords: $
//=============================================================================

#ifndef PANORAMA_UIOVERLAYBUFFERSETTYPE_H
#define PANORAMA_UIOVERLAYBUFFERSETTYPE_H
#ifdef _WIN32
#pragma once
#endif

namespace panorama
{


// Panorama "top level windows", specified in the order of occlusion
// priority, bottom windows go first, followed by more visible windows
// that will occlude the ones below
enum EPanoramaOverlayBufferSetType
{
	k_EPanoramaOverlayBufferSetType_MainWindow = 0,
	k_EPanoramaOverlayBufferSetType_Notifications = 1,
	k_EPanoramaOverlayBufferSetType_TotalCount = 2
};


};

#endif // PANORAMA_UIOVERLAYBUFFERSETTYPE_H
