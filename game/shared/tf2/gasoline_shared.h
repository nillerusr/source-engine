//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GASOLINE_SHARED_H
#define GASOLINE_SHARED_H
#ifdef _WIN32
#pragma once
#endif


#define PYRO_AMMO_TYPE	"Gasoline"


// Radius in inches of each gasoline blob. They should render to about this size.
#define GASOLINE_BLOB_RADIUS				30

// Blobs start to attract each other when their centerpoints get this close.
#define GASOLINE_ATTRACT_START_DISTANCE		(GASOLINE_BLOB_RADIUS + 20)


// Blobs expire after this long whether they are lit or not.
#define MAX_LIT_GASOLINE_BLOB_LIFETIME			5.0
#define MAX_UNLIT_GASOLINE_BLOB_LIFETIME		20.0


// Heat per second given off by fire.
#define FIRE_DAMAGE_PER_SEC					85


#define BLOBFLAG_LIT			0x01	// This blob is on fire.
#define BLOBFLAG_STOPPED		0x02	// This means it has hit a surface and stopped moving.
#define BLOBFLAG_USE_GRAVITY	0x04
#define NUM_BLOB_FLAGS			3


#endif // GASOLINE_SHARED_H
