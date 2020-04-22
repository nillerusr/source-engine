//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <windows.h>
#include "Render3D.h"
#include "Render3DMS.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


CRender3D *CreateRender3D(Render3DType_t eRender3DType)
{
	switch (eRender3DType)
	{
		case Render3DTypeMaterialSystem:
		{
			return(new CRender3DMS());
		}
	}

	return(NULL);
}
