//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef MAPPOINT_H
#define MAPPOINT_H
#pragma once


#include "MapAtom.h"


class CMapPoint : public CMapAtom
{
	typedef CMapAtom BaseClass;

	public:

		CMapPoint(void);

		virtual void GetOrigin(Vector& pfOrigin);
		virtual void SetOrigin(Vector& pfOrigin);
		
	protected:

		void DoTransform(const VMatrix &matrix);
		
		Vector m_Origin;
};


#endif // MAPPOINT_H
