//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef DISPSUBDIV_H
#define DISPSUBDIV_H
#if defined ( _WIN32 )
#pragma once
#endif

class CMapDisp;

//=============================================================================
class IEditDispSubdivMesh
{
public:

	virtual void Init( void ) = 0;
	virtual void Shutdown( void ) = 0;

	virtual void AddDispTo( CMapDisp *pDisp  ) = 0;
	virtual void GetDispFrom( CMapDisp *pDisp ) = 0;

	virtual void DoCatmullClarkSubdivision( void ) = 0;
};

IEditDispSubdivMesh *CreateEditDispSubdivMesh( void );
void DestroyEditDispSubdivMesh( IEditDispSubdivMesh **pSubdivMesh );

#endif // DISPSUBDIV_H