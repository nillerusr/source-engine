//===== Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ====
//
// Dme version of QC $BoneFlexDriver
//
//===========================================================================//


// Valve includes
#include "datamodel/dmelementfactoryhelper.h"
#include "mdlobjects/dmeBoneFlexDriver.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//===========================================================================//
// CDmeBoneFlexDriverControl
//===========================================================================//
//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeBoneFlexDriverControl, CDmeBoneFlexDriverControl );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeBoneFlexDriverControl::OnConstruction()
{
	m_sFlexControllerName.Init( this, "flexControllerName" );
	m_nBoneComponent.Init( this, "boneComponent" );
	m_flMin.InitAndSet( this, "min", 0.0f );
	m_flMax.InitAndSet( this, "max", 1.0f );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeBoneFlexDriverControl::OnDestruction()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CDmeBoneFlexDriverControl::SetBoneComponent( int nBoneComponent )
{
	// Range [STUDIO_BONE_FLEX_TX, STUDIO_BONE_FLEX_RZ]
	m_nBoneComponent = clamp( nBoneComponent, 0, 5 );
	return m_nBoneComponent.Get();
}


//===========================================================================//
// CDmeBoneFlexDriver
//===========================================================================//
//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeBoneFlexDriver, CDmeBoneFlexDriver );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeBoneFlexDriver::OnConstruction()
{
	m_sBoneName.Init( this, "boneName" );
	m_eControlList.Init( this, "controlList" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeBoneFlexDriver::OnDestruction()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmeBoneFlexDriverControl *CDmeBoneFlexDriver::FindOrCreateControl( const char *pszControlName )
{
	CDmeBoneFlexDriverControl *pDmeBoneFlexDriverControl = NULL;

	for ( int i = 0; i < m_eControlList.Count(); ++i )
	{
		pDmeBoneFlexDriverControl = m_eControlList[i];
		if ( !pDmeBoneFlexDriverControl )
			continue;

		if ( !Q_stricmp( pszControlName, pDmeBoneFlexDriverControl->m_sFlexControllerName.Get() ) )
			return pDmeBoneFlexDriverControl;
	}

	pDmeBoneFlexDriverControl = CreateElement< CDmeBoneFlexDriverControl >( "", GetFileId() );	// Nameless
	if ( !pDmeBoneFlexDriverControl )
		return NULL;

	pDmeBoneFlexDriverControl->m_sFlexControllerName = pszControlName;
	m_eControlList.AddToTail( pDmeBoneFlexDriverControl );

	return pDmeBoneFlexDriverControl;
}


//===========================================================================//
// CDmeBoneFlexDriverList
//===========================================================================//
//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeBoneFlexDriverList, CDmeBoneFlexDriverList );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeBoneFlexDriverList::OnConstruction()
{
	m_eBoneFlexDriverList.Init( this, "boneFlexDriverList" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeBoneFlexDriverList::OnDestruction()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmeBoneFlexDriver *CDmeBoneFlexDriverList::FindOrCreateBoneFlexDriver( const char *pszBoneName )
{
	CDmeBoneFlexDriver *pDmeBoneFlexDriver = NULL;

	for ( int i = 0; i < m_eBoneFlexDriverList.Count(); ++i )
	{
		pDmeBoneFlexDriver = m_eBoneFlexDriverList[i];
		if ( !pDmeBoneFlexDriver )
			continue;

		if ( !Q_stricmp( pszBoneName, pDmeBoneFlexDriver->m_sBoneName.Get() ) )
			return pDmeBoneFlexDriver;
	}

	pDmeBoneFlexDriver = CreateElement< CDmeBoneFlexDriver >( "", GetFileId() );	// Nameless
	if ( !pDmeBoneFlexDriver )
		return NULL;

	pDmeBoneFlexDriver->m_sBoneName = pszBoneName;
	m_eBoneFlexDriverList.AddToTail( pDmeBoneFlexDriver );

	return pDmeBoneFlexDriver;
}
