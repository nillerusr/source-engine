//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef C_POSTPROCESSCONTROLLER_H
#define C_POSTPROCESSCONTROLLER_H
#ifdef _WIN32
#pragma once
#endif

#include "postprocess_shared.h"

//=============================================================================
//
// Class Postprocess Controller:
//
class C_PostProcessController : public C_BaseEntity
{
	DECLARE_CLASS( C_PostProcessController, C_BaseEntity );
public:
	DECLARE_CLIENTCLASS();

	C_PostProcessController();
	virtual ~C_PostProcessController();

	virtual void PostDataUpdate( DataUpdateType_t updateType );

	static C_PostProcessController* GetMasterController() { return ms_pMasterController; }

	PostProcessParameters_t	m_PostProcessParameters;
	
private:
	bool m_bMaster;

	static C_PostProcessController* ms_pMasterController;
};


#endif // C_POSTPROCESSCONTROLLER_H
