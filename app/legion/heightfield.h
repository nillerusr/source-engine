//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Heightfield class
//
// $Revision: $
// $NoKeywords: $
//===========================================================================//

#ifndef HEIGHTFIELD_H
#define HEIGHTFIELD_H

#ifdef _WIN32
#pragma once
#endif

#include "materialsystem/MaterialSystemUtil.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CMeshBuilder;


//-----------------------------------------------------------------------------
// Definition of a heightfield
//-----------------------------------------------------------------------------
class CHeightField
{
public:
	CHeightField( int nPowX, int nPowY, int nPowScale );
	~CHeightField();

	// Loads the heights from a file
	bool LoadHeightFromFile( const char *pFileName );

	// Returns the max range of x, y
	int GetWidth();
	int GetHeight();

	// Returns the height of the field at a paticular (x,y)
	float GetHeight( float x, float y );
	float GetHeightAndSlope( float x, float y, float *dx, float *dy );

	// Draws the heightfield
	void Draw( );

private:
	int m_nPowX;
	int m_nPowY;
	int m_nWidth;
	int m_nHeight;
	int m_nScale;
	int m_nPowScale;
	float m_flOOScale;
	float *m_pHeightField;

	CMaterialReference m_Material;
	CTextureReference m_Texture;
};


//-----------------------------------------------------------------------------
// Returns the max range of x, y (for use in GetHeight)
//-----------------------------------------------------------------------------
inline int CHeightField::GetWidth()
{
	return m_nWidth << m_nPowScale;
}

inline int CHeightField::GetHeight()
{
	return m_nHeight << m_nPowScale;
}


#endif // HEIGHTFIELD_H
