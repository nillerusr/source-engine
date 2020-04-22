//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "stdafx.h"
#include "hammer.h"
#include "StockSolids.h"
#include "hammer_mathlib.h"
#include "MapSolid.h"


// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


#pragma warning(disable:4244)

//Vector pmPoints[64];

StockSolid::StockSolid(int nFields)
{
	AllocateDataFields(nFields);
	cofs.Init();
}


StockSolid::~StockSolid()
{
	if ( pFields )
	{
		delete[] pFields;
		pFields = NULL;
	}
}


void StockSolid::AllocateDataFields(int nFields_)
{
	pFields = new STSDATAFIELD[nFields_];
	Assert(pFields);
	iMaxFields = nFields_;
	this->nFields = 0;	// none yet
}


void StockSolid::Serialize(std::fstream& file, BOOL bIsStoring)
{
}


int StockSolid::GetFieldCount() const
{
	return nFields;
}


void StockSolid::SetFieldData(int iIndex, int iData)
{
	Assert(iIndex < nFields);

	STSDATAFIELD& field = pFields[iIndex];
	field.iValue = iData;

	if(field.flags & DFFLAG_RANGED)
	{
		Assert(!(iData < field.iRangeLower || iData > field.iRangeUpper));
	}
}


int StockSolid::GetFieldData(int iIndex, int *piData) const
{
	Assert(iIndex < nFields);

	STSDATAFIELD& field = pFields[iIndex];

	if(piData)
		piData[0] = field.iValue;

	return field.iValue;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void StockSolid::SetOrigin(const Vector &o)
{
	origin = o;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void StockSolid::SetCenterOffset(const Vector &ofs)
{
	cofs = ofs;
}


void StockSolid::AddDataField(STSDF_TYPE type, const char *pszName, int iRangeLower, int iRangeUpper)
{
	Assert(nFields < iMaxFields);
	
	STSDATAFIELD& field = pFields[nFields++];

	field.type = type;
	field.flags = 0;
	strcpy(field.szName, pszName);

	if(iRangeLower != -1)
	{
		field.flags |= DFFLAG_RANGED;
		field.iRangeLower = iRangeLower;
		field.iRangeUpper = iRangeUpper;
	}
}


// ----------------------------------------------------------------------------
// StockBlock()
// ----------------------------------------------------------------------------
StockBlock::StockBlock() :
	StockSolid(3)
{
	AddDataField(DFTYPE_INTEGER, "Width (X)");
	AddDataField(DFTYPE_INTEGER, "Depth (Y)");
	AddDataField(DFTYPE_INTEGER, "Height (Z)");
}


void StockBlock::SetFromBox(BoundBox *pBox)
{
	// round floats before converting to integers
	SetFieldData(fieldWidth, (pBox->bmaxs[0] - pBox->bmins[0])+0.5f );
	SetFieldData(fieldDepth, (pBox->bmaxs[1] - pBox->bmins[1])+0.5f );
	SetFieldData(fieldHeight, (pBox->bmaxs[2] - pBox->bmins[2])+0.5f );

	Vector o;
	pBox->GetBoundsCenter(o);

	SetOrigin(o);
}


void StockBlock::CreateMapSolid(CMapSolid *pSolid, TextureAlignment_t eAlignment)
{
	CMapFace Face;

	float fDepth = float(GetFieldData(fieldDepth))/2;
	float fWidth = float(GetFieldData(fieldWidth))/2;
	float fHeight = float(GetFieldData(fieldHeight))/2;

	// create box
	Vector bmins, bmaxs;
	bmins[0] = origin[0] - fWidth + cofs[0];
	bmins[1] = origin[1] - fDepth + cofs[1];
	bmins[2] = origin[2] - fHeight + cofs[2];

	bmaxs[0] = origin[0] + fWidth + cofs[0];
	bmaxs[1] = origin[1] + fDepth + cofs[1];
	bmaxs[2] = origin[2] + fHeight + cofs[2];

	Vector Points[4];

	// x planes - top first
	Points[0][0] = bmins[0];
	Points[0][1] = bmaxs[1];
	Points[0][2] = bmaxs[2];

	Points[1][0] = bmaxs[0];
	Points[1][1] = bmaxs[1];
	Points[1][2] = bmaxs[2];

	Points[2][0] = bmaxs[0];
	Points[2][1] = bmins[1];
	Points[2][2] = bmaxs[2];
	
	Points[3][0] = bmins[0];
	Points[3][1] = bmins[1];
	Points[3][2] = bmaxs[2];

	Face.CreateFace(Points, 4, pSolid->IsCordonBrush());
	pSolid->AddFace(&Face);

	// top - modify heights
	for(int i = 0; i < 4; i++)
	{
		Points[i][2] = bmins[2];
	}

	Face.CreateFace(Points, -4, pSolid->IsCordonBrush());
	pSolid->AddFace(&Face);

	// y planes - left
	Points[0][0] = bmins[0];
	Points[0][1] = bmaxs[1];
	Points[0][2] = bmaxs[2];

	Points[1][0] = bmins[0];
	Points[1][1] = bmins[1];
	Points[1][2] = bmaxs[2];

	Points[2][0] = bmins[0];
	Points[2][1] = bmins[1];
	Points[2][2] = bmins[2];

	Points[3][0] = bmins[0];
	Points[3][1] = bmaxs[1];
	Points[3][2] = bmins[2];

	Face.CreateFace(Points, 4, pSolid->IsCordonBrush());
	pSolid->AddFace(&Face);

	// right - modify xloc
	for(int i = 0; i < 4; i++)
	{
		Points[i][0] = bmaxs[0];
	}

	Face.CreateFace(Points, -4, pSolid->IsCordonBrush());
	pSolid->AddFace(&Face);

	// x planes - farthest
	Points[0][0] = bmaxs[0];
	Points[0][1] = bmaxs[1];
	Points[0][2] = bmaxs[2];

	Points[1][0] = bmins[0];
	Points[1][1] = bmaxs[1];
	Points[1][2] = bmaxs[2];

	Points[2][0] = bmins[0];
	Points[2][1] = bmaxs[1];
	Points[2][2] = bmins[2];

	Points[3][0] = bmaxs[0];
	Points[3][1] = bmaxs[1];
	Points[3][2] = bmins[2];

	Face.CreateFace(Points, 4, pSolid->IsCordonBrush());
	pSolid->AddFace(&Face);

	// nearest - modify yloc
	for(int i = 0; i < 4; i++)
	{
		Points[i][1] = bmins[1];
	}

	Face.CreateFace(Points, -4, pSolid->IsCordonBrush());
	pSolid->AddFace(&Face);

	pSolid->CalcBounds();
	pSolid->InitializeTextureAxes(eAlignment, INIT_TEXTURE_ALL | INIT_TEXTURE_FORCE);
}


// ----------------------------------------------------------------------------
// StockWedge()
// ----------------------------------------------------------------------------
StockWedge::StockWedge() :
	StockSolid(3)
{
	AddDataField(DFTYPE_INTEGER, "Width (X)");
	AddDataField(DFTYPE_INTEGER, "Depth (Y)");
	AddDataField(DFTYPE_INTEGER, "Height (Z)");
}


void StockWedge::SetFromBox(BoundBox *pBox)
{
	SetFieldData(fieldWidth, pBox->bmaxs[0] - pBox->bmins[0]);
	SetFieldData(fieldDepth, pBox->bmaxs[1] - pBox->bmins[1]);
	SetFieldData(fieldHeight, pBox->bmaxs[2] - pBox->bmins[2]);

	Vector o;
	pBox->GetBoundsCenter(o);

	SetOrigin(o);
}


void StockWedge::CreateMapSolid(CMapSolid *pSolid, TextureAlignment_t eTextureAlignment)
{
	CMapFace Face;

	float fDepth = float(GetFieldData(fieldDepth))/2;
	float fWidth = float(GetFieldData(fieldWidth))/2;
	float fHeight = float(GetFieldData(fieldHeight))/2;

	Vector Points[4];

	// x planes - top
	Points[0][0] = origin[0] + fWidth;
	Points[0][1] = origin[1] + fDepth;
	Points[0][2] = origin[2] + fHeight;

	Points[1][0] = origin[0] + fWidth;
	Points[1][1] = origin[1] - fDepth;
	Points[1][2] = origin[2] + fHeight;

	Points[2][0] = origin[0] - fWidth;
	Points[2][1] = origin[1] - fDepth;
	Points[2][2] = origin[2] + fHeight;

	Face.CreateFace(Points, 3);
	pSolid->AddFace(&Face);

	// bottom
	for (int i = 0; i < 3; i++)
	{
		Points[i][2] = origin[2] - fHeight;
	}

	Face.CreateFace(Points, -3);
	pSolid->AddFace(&Face);

	// left (slant)
	Points[0][0] = origin[0] + fWidth;
	Points[0][1] = origin[1] + fDepth;
	Points[0][2] = origin[2] - fHeight;

	Points[1][0] = origin[0] + fWidth;
	Points[1][1] = origin[1] + fDepth;
	Points[1][2] = origin[2] + fHeight;

	Points[2][0] = origin[0] - fWidth;
	Points[2][1] = origin[1] - fDepth;
	Points[2][2] = origin[2] + fHeight;
	
	Points[3][0] = origin[0] - fWidth;
	Points[3][1] = origin[1] - fDepth;
	Points[3][2] = origin[2] - fHeight;

	Face.CreateFace(Points, 4);
	pSolid->AddFace(&Face);

	// south
	Points[0][0] = origin[0] + fWidth;
	Points[0][1] = origin[1] - fDepth;
	Points[0][2] = origin[2] + fHeight;

	Points[1][0] = origin[0] + fWidth;
	Points[1][1] = origin[1] - fDepth;
	Points[1][2] = origin[2] - fHeight;

	Points[2][0] = origin[0] - fWidth;
	Points[2][1] = origin[1] - fDepth;
	Points[2][2] = origin[2] - fHeight;
	
	Points[3][0] = origin[0] - fWidth;
	Points[3][1] = origin[1] - fDepth;
	Points[3][2] = origin[2] + fHeight;

	Face.CreateFace(Points, 4);
	pSolid->AddFace(&Face);

	// right
	Points[0][0] = origin[0] + fWidth;
	Points[0][1] = origin[1] + fDepth;
	Points[0][2] = origin[2] + fHeight;

	Points[1][0] = origin[0] + fWidth;
	Points[1][1] = origin[1] + fDepth;
	Points[1][2] = origin[2] - fHeight;

	Points[2][0] = origin[0] + fWidth;
	Points[2][1] = origin[1] - fDepth;
	Points[2][2] = origin[2] - fHeight;
	
	Points[3][0] = origin[0] + fWidth;
	Points[3][1] = origin[1] - fDepth;
	Points[3][2] = origin[2] + fHeight;

	Face.CreateFace(Points, 4);
	pSolid->AddFace(&Face);

	pSolid->CalcBounds();
	pSolid->InitializeTextureAxes(eTextureAlignment, INIT_TEXTURE_ALL | INIT_TEXTURE_FORCE);
}


// ----------------------------------------------------------------------------
// StockCylinder()
// ----------------------------------------------------------------------------
StockCylinder::StockCylinder()
	: StockSolid(4)
{
	AddDataField(DFTYPE_INTEGER, "Width (X)");
	AddDataField(DFTYPE_INTEGER, "Depth (Y)");
	AddDataField(DFTYPE_INTEGER, "Height (Z)");
	AddDataField(DFTYPE_INTEGER, "Number of Sides");

	SetFieldData(fieldSideCount, 8);
}


void StockCylinder::SetFromBox(BoundBox *pBox)
{
	SetFieldData(fieldWidth, pBox->bmaxs[0] - pBox->bmins[0]);
	SetFieldData(fieldDepth, pBox->bmaxs[1] - pBox->bmins[1]);
	SetFieldData(fieldHeight, pBox->bmaxs[2] - pBox->bmins[2]);

	Vector o;
	pBox->GetBoundsCenter(o);

	SetOrigin(o);
}


void StockCylinder::CreateMapSolid(CMapSolid *pSolid, TextureAlignment_t eTextureAlignment)
{
	CMapFace Face;

	float fDepth = float(GetFieldData(fieldDepth))/2;
	float fWidth = float(GetFieldData(fieldWidth))/2;
	float fHeight = float(GetFieldData(fieldHeight))/2;
	int nSides = GetFieldData(fieldSideCount);

	Vector pmPoints[64];
	polyMake(origin[0] - fWidth, origin[1] - fDepth, origin[0] + fWidth, origin[1] + fDepth, nSides, 0, pmPoints );

	// face 0 - top face
	for(int i = 0; i < nSides+1; i++)
	{
		pmPoints[i][2] = origin[2] - fHeight;
	}

	Face.CreateFace( pmPoints, -nSides);
	pSolid->AddFace(&Face);

	// bottom face
	for(int i = 0; i < nSides+1; i++)
	{
		pmPoints[i][2] = origin[2] + fHeight;
	}

	Face.CreateFace( pmPoints, nSides);
	pSolid->AddFace(&Face);

	// other sides
	Vector Points[4];

	for(int i = 0; i < nSides; i++)
	{
		Points[0][0] = pmPoints[i][0];
		Points[0][1] = pmPoints[i][1];
		Points[0][2] = origin[2] - fHeight;

		Points[1][0] = pmPoints[i+1][0];
		Points[1][1] = pmPoints[i+1][1];
		Points[1][2] = origin[2] - fHeight;

		Points[2][0] = pmPoints[i+1][0];
		Points[2][1] = pmPoints[i+1][1];
		Points[2][2] = origin[2] + fHeight;

		Points[3][0] = pmPoints[i][0];
		Points[3][1] = pmPoints[i][1];
		Points[3][2] = origin[2] + fHeight;

		Face.CreateFace(Points, 4);
		Face.texture.smooth = 1.f;
		pSolid->AddFace(&Face);
	}

	pSolid->CalcBounds();
	pSolid->InitializeTextureAxes(eTextureAlignment, INIT_TEXTURE_ALL | INIT_TEXTURE_FORCE);
}


// ----------------------------------------------------------------------------
// StockSpike()
// ----------------------------------------------------------------------------
StockSpike::StockSpike()
	: StockSolid(4)
{
	AddDataField(DFTYPE_INTEGER, "Width (X)");
	AddDataField(DFTYPE_INTEGER, "Depth (Y)");
	AddDataField(DFTYPE_INTEGER, "Height (Z)");
	AddDataField(DFTYPE_INTEGER, "Number of Sides");

	SetFieldData(fieldSideCount, 8);
}


void StockSpike::SetFromBox(BoundBox *pBox)
{
	SetFieldData(fieldWidth, pBox->bmaxs[0] - pBox->bmins[0]);
	SetFieldData(fieldDepth, pBox->bmaxs[1] - pBox->bmins[1]);
	SetFieldData(fieldHeight, pBox->bmaxs[2] - pBox->bmins[2]);

	Vector o;
	pBox->GetBoundsCenter(o);

	SetOrigin(o);
}


void StockSpike::CreateMapSolid(CMapSolid *pSolid, TextureAlignment_t eTextureAlignment)
{
	float fDepth = float(GetFieldData(fieldDepth))/2;
	float fWidth = float(GetFieldData(fieldWidth))/2;
	float fHeight = float(GetFieldData(fieldHeight))/2;
	int nSides = GetFieldData(fieldSideCount);
	CMapFace NewFace;

	// create bottom poly
	Vector pmPoints[64];
	polyMake(origin[0] - fWidth, origin[1] - fDepth, origin[0] + fWidth, origin[1] + fDepth, nSides, 0, pmPoints);

	// bottom face
	for(int i = 0; i < nSides+1; i++)
	{
		// YWB rounding???
		pmPoints[i][2] = V_rint(origin[2] - fHeight);
	}

	NewFace.CreateFace(pmPoints, -nSides);
	pSolid->AddFace(&NewFace);

	// other sides
	Vector Points[3];

	// get centerpoint
	Points[0][0] = origin[0];
	Points[0][1] = origin[1];
	// YWB rounding???
	Points[0][2] = V_rint(origin[2] + fHeight);

	for(int i = 0; i < nSides; i++)
	{
		Points[1][0] = pmPoints[i][0];
		Points[1][1] = pmPoints[i][1];
		Points[1][2] = pmPoints[i][2];

		Points[2][0] = pmPoints[i+1][0];
		Points[2][1] = pmPoints[i+1][1];
		Points[2][2] = pmPoints[i+1][2];

		NewFace.CreateFace(Points, 3);
		pSolid->AddFace(&NewFace);
	}

	pSolid->CalcBounds();
	pSolid->InitializeTextureAxes(eTextureAlignment, INIT_TEXTURE_ALL | INIT_TEXTURE_FORCE);
}


StockSphere::StockSphere()
	: StockSolid(4)
{
	AddDataField(DFTYPE_INTEGER, "Width (X)");
	AddDataField(DFTYPE_INTEGER, "Depth (Y)");
	AddDataField(DFTYPE_INTEGER, "Height (Z)");
	AddDataField(DFTYPE_INTEGER, "Subdivisions");

	SetFieldData(fieldSideCount, 8);
}


void StockSphere::SetFromBox(BoundBox *pBox)
{
	SetFieldData(fieldWidth, pBox->bmaxs[0] - pBox->bmins[0]);
	SetFieldData(fieldDepth, pBox->bmaxs[1] - pBox->bmins[1]);
	SetFieldData(fieldHeight, pBox->bmaxs[2] - pBox->bmins[2]);

	Vector o;
	pBox->GetBoundsCenter(o);

	SetOrigin(o);
}


//-----------------------------------------------------------------------------
// Purpose: Builds a tesselated sphere.
// Input  : pSolid - Pointer to a solid that will become a sphere.
//-----------------------------------------------------------------------------
void StockSphere::CreateMapSolid(CMapSolid *pSolid, TextureAlignment_t eTextureAlignment)
{
	CMapFace Face;

	float fDepth = (float)GetFieldData(fieldDepth) / 2;
	float fWidth = (float)GetFieldData(fieldWidth) / 2;
	float fHeight = (float)GetFieldData(fieldHeight) / 2;
	int nSides = GetFieldData(fieldSideCount);

	float fAngle = 0;
	float fAngleStep = 180.0 / nSides;

	//
	// Build the sphere by building slices at constant angular intervals.
	// 
	// Each slice is a ring of four-sided faces, except for the top and bottom slices,
	// which are flattened cones.
	//
	// Unrolled, a sphere made with 5 'sides' has 25 faces and looks like this:
	//				
	//			/\  /\  /\  /\  /\
	//		   / 0\/ 1\/ 2\/ 3\/ 4\
	//		  |  5|  6|  7|  8|  9| 	
	//		  | 10| 11| 12| 13| 14| 	
	//		  | 15| 16| 17| 18| 19| 	
	//		   \20/\21/\22/\23/\24/
	//			\/  \/  \/  \/  \/
	//
	for (int nSlice = 0; nSlice < nSides; nSlice++)
	{
		float fAngle1 = fAngle + fAngleStep;

		//
		// Make the upper polygon.
		//
		Vector TopPoints[64];
		float fUpperWidth = fWidth * sin(DEG2RAD(fAngle));
		float fUpperDepth = fDepth * sin(DEG2RAD(fAngle));
		polyMake(origin[0] - fUpperWidth, origin[1] - fUpperDepth, origin[0] + fUpperWidth, origin[1] + fUpperDepth, nSides, 0, TopPoints);

		//
		// Make the lower polygon.
		//
		Vector BottomPoints[64];
		float fLowerWidth = fWidth * sin(DEG2RAD(fAngle1));
		float fLowerDepth = fDepth * sin(DEG2RAD(fAngle1));
		polyMake(origin[0] - fLowerWidth, origin[1] - fLowerDepth, origin[0] + fLowerWidth, origin[1] + fLowerDepth, nSides, 0, BottomPoints);

		//
		// Build the faces that connect the upper and lower polygons.
		//
		Vector Points[4];
		float fUpperHeight = origin[2] + fHeight * cos(DEG2RAD(fAngle));
		float fLowerHeight = origin[2] + fHeight * cos(DEG2RAD(fAngle1));

		for (int i = 0; i < nSides; i++)
		{
			if (nSlice != 0)
			{
				Points[0][0] = TopPoints[i + 1][0];
				Points[0][1] = TopPoints[i + 1][1];
				Points[0][2] = fUpperHeight;
			}
			
			Points[1][0] = TopPoints[i][0];
			Points[1][1] = TopPoints[i][1];
			Points[1][2] = fUpperHeight;

			Points[2][0] = BottomPoints[i][0];
			Points[2][1] = BottomPoints[i][1];
			Points[2][2] = fLowerHeight;

			if (nSlice != nSides - 1)
			{
				Points[3][0] = BottomPoints[i + 1][0];
				Points[3][1] = BottomPoints[i + 1][1];
				Points[3][2] = fLowerHeight;
			}

			//
			// Top and bottom are cones, not rings, so remove one vertex per face.
			//
			if (nSlice == 0)
			{
				Face.CreateFace(&Points[1], 3);
			}
			else if (nSlice == nSides - 1)
			{
				Face.CreateFace(Points, 3);
			}
			else
			{
				Face.CreateFace(Points, 4);
			}

			Face.texture.smooth = 1.f;
			pSolid->AddFace(&Face);
		}
	
		fAngle += fAngleStep;
	}

	pSolid->CalcBounds();
	pSolid->InitializeTextureAxes(eTextureAlignment, INIT_TEXTURE_ALL | INIT_TEXTURE_FORCE);
}


