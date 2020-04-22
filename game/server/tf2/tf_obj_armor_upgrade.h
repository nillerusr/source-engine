//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef TF_OBJ_ARMOR_UPGRADE_H
#define TF_OBJ_ARMOR_UPGRADE_H
#ifdef _WIN32
#pragma once
#endif


#include "tf_obj_baseupgrade_shared.h"


// This class shows a temporary model until you get to an object it can upgrade.
// Then it draws a shell over the model you want to upgrade.
class CArmorUpgrade : public CBaseObjectUpgrade
{
public:
	DECLARE_CLASS( CArmorUpgrade, CBaseObjectUpgrade );
	DECLARE_SERVERCLASS();


	CArmorUpgrade();


	virtual void Spawn();
};


#endif // TF_OBJ_ARMOR_UPGRADE_H
