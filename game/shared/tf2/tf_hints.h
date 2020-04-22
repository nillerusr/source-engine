//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_HINTS_H
#define TF_HINTS_H
#ifdef _WIN32
#pragma once
#endif

enum
{
	TF_HINT_UNDEFINED = 0,
	TF_HINT_VOTEFORTECHNOLOGY,
	TF_HINT_BUILDRESOURCEPUMP,
	TF_HINT_BUILDRESOURCEBOX,
	TF_HINT_BUILDZONEINCREASER,
	TF_HINT_BUILDSENTRYGUN_PLASMA,
	TF_HINT_BUILDSANDBAG,
	TF_HINT_BUILDANTIMORTAR,
	TF_HINT_REPAIROBJECT,

	TF_HINT_WEAPONRECEIVED,

	TF_HINT_NEWTECHNOLOGY,

	// Must be at end
	TF_HINT_LASTHINT,
};

#endif // TF_HINTS_H
