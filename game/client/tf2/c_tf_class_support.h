//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_TF_CLASS_SUPPORT_H
#define C_TF_CLASS_SUPPORT_H
#ifdef _WIN32
#pragma once
#endif

#include "c_tf_playerclass.h"
#include "dt_recv.h"

class C_PlayerClassSupport : public C_PlayerClass
{
	
	DECLARE_CLASS( C_PlayerClassSupport, C_PlayerClass );

public:

	C_PlayerClassSupport( C_BaseTFPlayer *pPlayer );
	virtual ~C_PlayerClassSupport();

	DECLARE_PREDICTABLE();

	PlayerClassSupportData_t *GetClassData( void ) { return &m_ClassData; } 

protected:

	PlayerClassSupportData_t	m_ClassData;
};

EXTERN_RECV_TABLE( DT_PlayerClassSupportData )

#endif // C_TF_CLASS_SUPPORT_H