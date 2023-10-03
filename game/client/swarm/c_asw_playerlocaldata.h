//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $NoKeywords: $
//=============================================================================//

#if !defined( C_ASW_PLAYERLOCALDATA_H )
#define C_ASW_PLAYERLOCALDATA_H
#ifdef _WIN32
#pragma once
#endif


#include "dt_recv.h"

//#include "hl2/hl_movedata.h"

EXTERN_RECV_TABLE( DT_ASWLocal );


class C_ASWPlayerLocalData
{
public:
	DECLARE_PREDICTABLE();
	DECLARE_CLASS_NOBASE( C_ASWPlayerLocalData );
	DECLARE_EMBEDDED_NETWORKVAR();

	C_ASWPlayerLocalData();

	EHANDLE m_hAutoAimTarget;
	Vector	m_vecAutoAimPoint;
	bool	m_bAutoAimTarget;
};


#endif
