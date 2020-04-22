//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_TF_CLASS_MEDIC_H
#define C_TF_CLASS_MEDIC_H
#ifdef _WIN32
#pragma once
#endif

#include "c_tf_playerclass.h"
#include "dt_recv.h"

class C_PlayerClassMedic : public C_PlayerClass
{
	
	DECLARE_CLASS( C_PlayerClassMedic, C_PlayerClass );

public:

	C_PlayerClassMedic( C_BaseTFPlayer *pPlayer );
	virtual ~C_PlayerClassMedic();

	DECLARE_PREDICTABLE();

	PlayerClassMedicData_t *GetClassData( void ) { return &m_ClassData; } 

protected:

	PlayerClassMedicData_t	m_ClassData;
};

EXTERN_RECV_TABLE( DT_PlayerClassMedicData )

#endif // C_TF_CLASS_MEDIC_H