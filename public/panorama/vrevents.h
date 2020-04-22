//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef VREVENTS_H
#define VREVENTS_H

#ifdef _WIN32
#pragma once
#endif

#include "uievent.h"
#include "uieventcodes.h"
#include "layout/stylesymbol.h"
#include "localization/ilocalize.h"


namespace panorama
{
	// dashboard events
	DECLARE_PANORAMA_EVENT2( VRDashboardRequested, vr::TrackedDeviceIndex_t, vr::VROverlayHandle_t );
	DECLARE_PANORAMA_EVENT1( VRDashboardThumbSelected, vr::VROverlayHandle_t );
	DECLARE_PANORAMA_EVENT0( VRResetDashboard );
	DECLARE_PANORAMA_EVENT1( VRRenderToast, vr::VRNotificationId );
}

#endif // VREVENTS_H