//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose : Singleton manager for color correction on the client
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "tier0/vprof.h"
#include "asw_client_entities.h"
#include "c_asw_camera_volume.h"
#include "c_asw_snow_volume.h"
#include "c_asw_scanner_noise.h"

static CASW_Client_Entities s_ASW_Client_Entities;

CASW_Client_Entities::CASW_Client_Entities()
{
}


void CASW_Client_Entities::LevelInitPostEntity()
{
	//C_ASW_Camera_Volume::RecreateAll();
	C_ASW_Snow_Volume::RecreateAll();
	//C_Sprite::RecreateAll();
	C_ASW_Scanner_Noise::RecreateAll();
}

void CASW_Client_Entities::LevelShutdownPreEntity()
{
	//C_ASW_Camera_Volume::DestroyAll();	
	C_ASW_Snow_Volume::DestroyAll();	
	//C_Sprite::DestroyAll();
	C_ASW_Scanner_Noise::DestroyAll();
}