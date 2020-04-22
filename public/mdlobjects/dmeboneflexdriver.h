//===== Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ====
//
// Dme version of QC $boneflexdriver
//
//===========================================================================


#ifndef BONEFLEXDRIVER_H
#define BONEFLEXDRIVER_H


#ifdef _WIN32
#pragma once
#endif


// Valve includes
#include "mdlobjects/dmemdllist.h"


//-----------------------------------------------------------------------------
// The control for a DmeBoneFlexDriver
//-----------------------------------------------------------------------------
class CDmeBoneFlexDriverControl : public CDmElement
{
	DEFINE_ELEMENT( CDmeBoneFlexDriverControl, CDmElement );

public:
	// Sets the bone component to be in the range [STUDIO_BONE_FLEX_TX, STUDIO_BONE_FLEX_RZ]
	int SetBoneComponent( int nBoneComponent );

	CDmaString m_sFlexControllerName;	// Name of flex controller to drive
	CDmaVar< int > m_nBoneComponent;	// Component of bone to drive flex controller, StudioBoneFlexComponent_t
	CDmaVar< float > m_flMin;			// Min value of bone component mapped to 0 on flex controller
	CDmaVar< float > m_flMax;			// Max value of bone component mapped to 1 on flex controller (inches if T, degress if R)

};


//-----------------------------------------------------------------------------
// $QC boneflexdriver
//-----------------------------------------------------------------------------
class CDmeBoneFlexDriver : public CDmeMdlList
{
	DEFINE_ELEMENT( CDmeBoneFlexDriver, CDmeMdlList );

public:
	virtual CDmAttribute *GetListAttr() { return m_eControlList.GetAttribute(); }

	CDmeBoneFlexDriverControl *FindOrCreateControl( const char *pszControlName );

	CDmaString m_sBoneName;	// Name of bone to drive flex controller
	CDmaElementArray< CDmeBoneFlexDriverControl > m_eControlList;	// List of flex controllers to drive

};


//-----------------------------------------------------------------------------
// A list of DmeBoneFlexDriver elements
//-----------------------------------------------------------------------------
class CDmeBoneFlexDriverList : public CDmeMdlList
{
	DEFINE_ELEMENT( CDmeBoneFlexDriverList, CDmeMdlList );

public:
	virtual CDmAttribute *GetListAttr() { return m_eBoneFlexDriverList.GetAttribute(); }

	CDmeBoneFlexDriver *FindOrCreateBoneFlexDriver( const char *pszBoneName );

	CDmaElementArray< CDmeBoneFlexDriver > m_eBoneFlexDriverList;
};


#endif // BONEFLEXDRIVER_H
