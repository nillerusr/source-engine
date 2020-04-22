//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_TF_CLASS_INFILTRATOR_H
#define C_TF_CLASS_INFILTRATOR_H
#ifdef _WIN32
#pragma once
#endif

#include "c_tf_playerclass.h"
#include "dt_recv.h"

class C_PlayerClassInfiltrator : public C_PlayerClass
{
	
	DECLARE_CLASS( C_PlayerClassInfiltrator, C_PlayerClass );

public:

	C_PlayerClassInfiltrator( C_BaseTFPlayer *pPlayer );
	virtual ~C_PlayerClassInfiltrator();
	
	DECLARE_PREDICTABLE();

	PlayerClassInfiltratorData_t *GetClassData( void ) { return &m_ClassData; } 

protected:

	PlayerClassInfiltratorData_t	m_ClassData;
};

EXTERN_RECV_TABLE( DT_PlayerClassInfiltratorData )

#endif // C_TF_CLASS_INFILTRATOR_H