//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef MDLLIB_STRIPINFO_H
#define MDLLIB_STRIPINFO_H
#ifdef _WIN32
#pragma once
#endif

#include "mdllib/mdllib.h"
#include "mdllib_utils.h"
#include "UtlSortVector.h"


//
// CMdlStripInfo
//	Implementation of IMdlStripInfo interface
//
class CMdlStripInfo : public IMdlStripInfo
{
public:
	CMdlStripInfo();

	//
	// Serialization
	//
public:
	// Save the strip info to the buffer (appends to the end)
	virtual bool Serialize( CUtlBuffer &bufStorage ) const;

	// Load the strip info from the buffer (reads from the current position as much as needed)
	virtual bool UnSerialize( CUtlBuffer &bufData );

	//
	// Stripping info state
	//
public:
	// Returns the checksums that the stripping info was generated for:
	//	plChecksumOriginal		if non-NULL will hold the checksum of the original model submitted for stripping
	//	plChecksumStripped		if non-NULL will hold the resulting checksum of the stripped model
	virtual bool GetCheckSum( long *plChecksumOriginal, long *plChecksumStripped ) const;

	//
	// Stripping
	//
public:

	//
	// StripHardwareVertsBuffer
	//	The main function that strips the vhv buffer
	//		vhvBuffer		- vhv buffer, updated, size reduced
	//
	virtual bool StripHardwareVertsBuffer( CUtlBuffer &vhvBuffer );

	//
	// StripModelBuffer
	//	The main function that strips the mdl buffer
	//		mdlBuffer		- mdl buffer, updated
	//
	virtual bool StripModelBuffer( CUtlBuffer &mdlBuffer );

	//
	// StripVertexDataBuffer
	//	The main function that strips the vvd buffer
	//		vvdBuffer		- vvd buffer, updated, size reduced
	//
	virtual bool StripVertexDataBuffer( CUtlBuffer &vvdBuffer );

	//
	// StripOptimizedModelBuffer
	//	The main function that strips the vtx buffer
	//		vtxBuffer		- vtx buffer, updated, size reduced
	//
	virtual bool StripOptimizedModelBuffer( CUtlBuffer &vtxBuffer );

	//
	// Release the object with "delete this"
	//
public:
	virtual void DeleteThis();



public:
	void Reset();


public:
	enum Mode
	{
		MODE_UNINITIALIZED	= 0,
		MODE_NO_CHANGE		= 1,
		MODE_STRIP_LOD_1N	= 2,
	};

	//
	// Internal data used for stripping
	//
public:
	int m_eMode;
	long m_lChecksumOld, m_lChecksumNew;
	CGrowableBitVec m_vtxVerts;
	CUtlSortVector< unsigned short, CLessSimple< unsigned short > > m_vtxIndices;

	//
	// Mesh ranges fixup
	//
public:
	struct MdlRangeItem
	{
		/* implicit */ MdlRangeItem( int offOld = 0, int numOld = 0, int offNew = 0, int numNew = 0 ) :
			m_offOld( offOld ), m_offNew( offNew ), m_numOld( numOld ), m_numNew( numNew ) {}

		int m_offOld, m_offNew;
		int m_numOld, m_numNew;

		bool operator < ( MdlRangeItem const &x ) const { return m_offNew < x.m_offNew; }
	};
	CUtlSortVector< CMdlStripInfo::MdlRangeItem, CLessSimple< CMdlStripInfo::MdlRangeItem > > m_vtxMdlOffsets;

};


#endif // #ifndef MDLLIB_STRIPINFO_H
