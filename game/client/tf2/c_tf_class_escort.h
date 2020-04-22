//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_TF_CLASS_ESCORT_H
#define C_TF_CLASS_ESCORT_H
#ifdef _WIN32
#pragma once
#endif

#include "c_tf_playerclass.h"
#include "dt_recv.h"

class C_PlayerClassEscort : public C_PlayerClass
{
	
	DECLARE_CLASS( C_PlayerClassEscort, C_PlayerClass );

public:

	C_PlayerClassEscort( C_BaseTFPlayer *pPlayer );
	virtual ~C_PlayerClassEscort();

	DECLARE_PREDICTABLE();
	PlayerClassEscortData_t *GetClassData( void ) { return &m_ClassData; } 

protected:

	PlayerClassEscortData_t	m_ClassData;
};

EXTERN_RECV_TABLE( DT_PlayerClassEscortData )

#endif // C_TF_CLASS_ESCORT_H