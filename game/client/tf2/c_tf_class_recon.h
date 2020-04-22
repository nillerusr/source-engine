//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_TF_CLASS_RECON_H
#define C_TF_CLASS_RECON_H
#ifdef _WIN32
#pragma once
#endif

#include "c_tf_playerclass.h"
#include "TFClassData_Shared.h"
#include "dt_recv.h"

class C_PlayerClassRecon : public C_PlayerClass
{
	
	DECLARE_CLASS( C_PlayerClassRecon, C_PlayerClass );

public:

	C_PlayerClassRecon( C_BaseTFPlayer *pPlayer );
	virtual ~C_PlayerClassRecon();

	DECLARE_PREDICTABLE();

	PlayerClassReconData_t *GetClassData( void ) { return &m_ClassData; } 

	PlayerClassReconData_t	m_ClassData;
};

EXTERN_RECV_TABLE( DT_PlayerClassReconData )

#endif // C_TF_CLASS_RECON_H