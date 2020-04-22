//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef C_OBJ_ARMOR_UPGRADE_H
#define C_OBJ_ARMOR_UPGRADE_H
#ifdef _WIN32
#pragma once
#endif


#include "tf_obj_baseupgrade_shared.h"


class C_ArmorUpgrade : public C_BaseObjectUpgrade
{
public:
	DECLARE_CLASS( C_ArmorUpgrade, C_BaseObjectUpgrade );
	DECLARE_CLIENTCLASS();
	C_ArmorUpgrade();

	virtual int DrawModel( int flags );


private:
	C_ArmorUpgrade( const C_ArmorUpgrade & ) {}
};


#endif // C_OBJ_ARMOR_UPGRADE_H
