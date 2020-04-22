//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef STOCKSOLIDS_H
#define STOCKSOLIDS_H
#ifdef _WIN32
#pragma once
#endif

#pragma warning(push, 1)
#pragma warning(disable:4701 4702 4530)
#include <fstream>
#pragma warning(pop)
#include "mathlib/vector.h"
#include "MapFace.h"


class BoundBox;
class CMapSolid;
class Vector;


class StockSolid
{
	public:
		void Serialize(std::fstream& file, BOOL bIsStoring);
		int GetFieldCount() const;
		void SetFieldData(int iIndex, int iData);
		int GetFieldData(int iIndex, int *piData = NULL) const;
		void GetFieldRange(int iIndex, int *piRangeLower, int *piRangeUpper);
		void SetOrigin(const Vector &o);
		void SetCenterOffset(const Vector &ofs);
		
		virtual void SetFromBox(BoundBox * pBox) {}

		virtual void CreateMapSolid(CMapSolid *pSolid, TextureAlignment_t eAlignment) = 0;

		~StockSolid();

	protected:
		StockSolid(int nFields);

		typedef enum
		{
			DFTYPE_INTEGER,
			DFTYPE_BOOLEAN
		} STSDF_TYPE;

		void AddDataField(STSDF_TYPE type, const char *pszName, int iRangeLower = -1,
			int iRangeUpper = -1);

		Vector origin;
		Vector cofs;

	private:
		void AllocateDataFields(int nFields);

		enum
		{
			DFFLAG_RANGED = 0x01
		};

		typedef struct
		{
			unsigned flags;
			STSDF_TYPE type;
			char szName[128];

		// range:
			int iRangeLower;
			int iRangeUpper;

		// value:
			int iValue;

		} STSDATAFIELD;

		STSDATAFIELD *pFields;
		int nFields;
		int iMaxFields;
};


class StockBlock : public StockSolid
{
	public:

		StockBlock();

		void CreateMapSolid(CMapSolid *pSolid, TextureAlignment_t eTextureAlignment);
		void SetFromBox(BoundBox *pBox);

		enum { fieldWidth, fieldDepth, fieldHeight };
};


class StockWedge : public StockSolid
{
	public:

		StockWedge();

		void CreateMapSolid(CMapSolid *pSolid, TextureAlignment_t eTextureAlignment);
		void SetFromBox(BoundBox *pBox);

		enum { fieldWidth, fieldDepth, fieldHeight };
};


class StockCylinder : public StockSolid
{
	public:

		StockCylinder();

		void CreateMapSolid(CMapSolid *pSolid, TextureAlignment_t eTextureAlignment);
		void SetFromBox(BoundBox *pBox);

		enum { fieldWidth, fieldDepth, fieldHeight, fieldSideCount };
};


class StockSpike : public StockSolid
{
	public:

		StockSpike();

		void CreateMapSolid(CMapSolid *pSolid, TextureAlignment_t eTextureAlignment);
		void SetFromBox(BoundBox *pBox);

		enum { fieldWidth, fieldDepth, fieldHeight, fieldSideCount };
};


class StockSphere : public StockSolid
{
	public:

		StockSphere();

		void CreateMapSolid(CMapSolid *pSolid, TextureAlignment_t eTextureAlignment);
		void SetFromBox(BoundBox *pBox);

		enum { fieldWidth, fieldDepth, fieldHeight, fieldSideCount };
};


#endif // STOCKSOLIDS_H
