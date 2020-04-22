//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_TF_CLASS_DEFENDER_H
#define C_TF_CLASS_DEFENDER_H
#ifdef _WIN32
#pragma once
#endif

#include "c_tf_playerclass.h"
#include "dt_recv.h"

class C_PlayerClassDefender : public C_PlayerClass
{
	
	DECLARE_CLASS( C_PlayerClassDefender, C_PlayerClass );

public:

	DECLARE_PREDICTABLE();

	C_PlayerClassDefender( C_BaseTFPlayer *pPlayer );
	virtual ~C_PlayerClassDefender();

	PlayerClassDefenderData_t *GetClassData( void ) { return &m_ClassData; } 

protected:

	PlayerClassDefenderData_t	m_ClassData;
};

EXTERN_RECV_TABLE( DT_PlayerClassDefenderData )

#endif // C_TF_CLASS_DEFENDER_H