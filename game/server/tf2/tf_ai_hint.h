//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_AI_HINT_H
#define TF_AI_HINT_H
#ifdef _WIN32
#pragma once
#endif

//=========================================================
// hints - these MUST coincide with the HINTS listed under
// info_node in the FGD file!
//=========================================================
enum TF_Hint_e
{
	HINT_RESOURCE_ZONE_AREA = 2000,

	// The carrier starts at base_area land spot, goes first to
	//  the hover spot, then goes to the dropoff hover spot and
	//  finally to the dropoff landspot
	HINT_AIR_CARRIER_DROPOFF_POINT_LANDSPOT,
	HINT_AIR_CARRIER_DROPOFF_POINT_HOVERSPOT,
	HINT_AIR_CARRIER_BASE_AREA_LANDSPOT,
	HINT_AIR_CARRIER_BASE_AREA_HOVERSPOT,

	// The ground collector needs hints to drive itself
	HINT_GROUNDCOLLECTOR_ZONE_ENTRANCE,
};

#endif // TF_AI_HINT_H
