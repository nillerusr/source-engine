//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_TF_CLASS_PYRO_H
#define C_TF_CLASS_PYRO_H
#ifdef _WIN32
#pragma once
#endif


#include "c_tf_playerclass.h"
#include "dt_recv.h"


class C_PlayerClassPyro : public C_PlayerClass
{
public:

	DECLARE_CLASS( C_PlayerClassPyro, C_PlayerClass );
	DECLARE_PREDICTABLE();


	C_PlayerClassPyro( C_BaseTFPlayer *pPlayer );
	virtual ~C_PlayerClassPyro();

	PlayerClassPyroData_t *GetClassData();


protected:

	PlayerClassPyroData_t	m_ClassData;
};

EXTERN_RECV_TABLE( DT_PlayerClassPyroData )


#endif // C_TF_CLASS_PYRO_H
