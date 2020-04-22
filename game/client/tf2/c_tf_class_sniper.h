//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_TF_CLASS_SNIPER_H
#define C_TF_CLASS_SNIPER_H
#ifdef _WIN32
#pragma once
#endif

#include "c_tf_playerclass.h"
#include "dt_recv.h"

class C_PlayerClassSniper : public C_PlayerClass
{
	
	DECLARE_CLASS( C_PlayerClassSniper, C_PlayerClass );

public:

	C_PlayerClassSniper( C_BaseTFPlayer *pPlayer );
	virtual ~C_PlayerClassSniper();

	DECLARE_PREDICTABLE();

	PlayerClassSniperData_t *GetClassData( void ) { return &m_ClassData; } 

protected:

	PlayerClassSniperData_t	m_ClassData;
};

EXTERN_RECV_TABLE( DT_PlayerClassSniperData )

#endif // C_TF_CLASS_SNIPER_H