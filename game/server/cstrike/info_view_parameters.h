//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef INFO_VIEW_PARAMETERS_H
#define INFO_VIEW_PARAMETERS_H
#ifdef _WIN32
#pragma once
#endif


class CInfoViewParameters : public CBaseEntity
{
public:
	DECLARE_CLASS( CInfoViewParameters, CBaseEntity );
	DECLARE_DATADESC();

	int m_nViewMode;
};


#endif // INFO_VIEW_PARAMETERS_H
