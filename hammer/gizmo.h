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

#ifndef GIZMO_H
#define GIZMO_H
#pragma once

#include "MapAtom.h"


#define GIZMO_AXIS_X			0x10
#define GIZMO_AXIS_Y			0x20
#define GIZMO_AXIS_Z			0x40

#define GIZMO_HANDLE_SCALE			0x01
#define GIZMO_HANDLE_ROTATE			0x02
#define GIZMO_HANDLE_TRANSLATE		0x04
#define GIZMO_HANDLE_UNIFORM_SCALE	0x08


class CGizmo : public CMapAtom
{
	public:

		CGizmo(void);
		CGizmo(float x, float y, float z);

		void Initialize(void);

		void Render(CRender3D *pRender);

		inline void SetAxisLength(float fLength); 
		inline void SetPosition(float x, float y, float z);

		void DrawGizmoAxis(CRender3D *pRender, Vector& Origin, Vector& EndPoint, int red, int green, int blue, unsigned int uAxisHandle);

	protected:

		Vector m_Position;
		float m_fAxisLength;
};


//-----------------------------------------------------------------------------
// Purpose: Sets the length of the gizmo's axes.
// Input  : fLength - Axis length in world units.
//-----------------------------------------------------------------------------
void CGizmo::SetAxisLength(float fLength)
{
	m_fAxisLength = fLength;
}


//-----------------------------------------------------------------------------
// Purpose: Sets the gizmo's position.
// Input  : x - 
//			y - 
//			z - 
//-----------------------------------------------------------------------------
void CGizmo::SetPosition(float x, float y, float z)
{
	m_Position[0] = x;
	m_Position[1] = y;
	m_Position[2] = z;
}


#endif // GIZMO_H
