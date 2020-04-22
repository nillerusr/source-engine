//========= Copyright Valve Corporation, All rights reserved. ============//
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// $Header: $
// $NoKeywords: $
//
//=============================================================================


// Valve includes
#include "appframework/tier2app.h"
#include "datamodel/idatamodel.h"
#include "filesystem.h"
#include "icommandline.h"
#include "mathlib/mathlib.h"
#include "vstdlib/vstdlib.h"
#include "tier2/p4helpers.h"
#include "p4lib/ip4.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlstringmap.h"
#include "datamodel/dmelement.h"

#ifdef _DEBUG
#include <windows.h>
#endif


//-----------------------------------------------------------------------------
// Standard spew functions
//-----------------------------------------------------------------------------
static SpewRetval_t SpewStdout( SpewType_t spewType, char const *pMsg )
{
	if ( !pMsg )
		return SPEW_CONTINUE;

#ifdef _DEBUG
	OutputDebugString( pMsg );
#endif

	printf( pMsg );
	fflush( stdout );

	return ( spewType == SPEW_ASSERT ) ? SPEW_DEBUGGER : SPEW_CONTINUE; 
}


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CPCFFixApp : public CTier2DmSteamApp
{
	typedef CTier2DmSteamApp BaseClass;

public:
	// Methods of IApplication
	virtual bool Create();
	virtual bool PreInit( );
	virtual int Main();
	virtual void Destroy() {}

	void PrintHelp( );

private:
	void FixupPCFFile( CDmElement *pRoot );
};


DEFINE_CONSOLE_STEAM_APPLICATION_OBJECT( CPCFFixApp );

//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
bool CPCFFixApp::Create()
{
	SpewOutputFunc( SpewStdout );

	AppSystemInfo_t appSystems[] = 
	{
		{ "p4lib.dll",				P4_INTERFACE_VERSION },
		{ "", "" }	// Required to terminate the list
	};

	AddSystems( appSystems );
	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CPCFFixApp::PreInit( )
{
	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f, false, false, false, false );

	if ( !BaseClass::PreInit() )
		return false;

	if ( !g_pFullFileSystem || !g_pDataModel )
	{
		Error( "// ERROR: pcffix is missing a required interface!\n" );
		return false;
	}

	// Add paths...
	if ( !SetupSearchPaths( NULL, false, true ) )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Returns true if an element is equal
//-----------------------------------------------------------------------------
static bool IsScalarAttributesEqual( CDmElement *pElement1, CDmElement *pElement2 )
{
	int nCount = pElement1->AttributeCount();
	if ( nCount != pElement2->AttributeCount() )
		return false;
	for ( CDmAttribute *pAtt1 = pElement1->FirstAttribute(); pAtt1; pAtt1 = pAtt1->NextAttribute() )
	{
		CDmAttribute *pAtt2 = pElement2->GetAttribute( pAtt1->GetName() );
		if ( !pAtt2 )
			return false;
		if ( pAtt1->GetType() != pAtt2->GetType() )
			return false;
		if ( IsArrayType( pAtt1->GetType() ) )
			continue;

		switch( pAtt1->GetType() )
		{
		case AT_FLOAT:
			if ( pAtt1->GetValue<float>() != pAtt2->GetValue<float>() )
				return false;
			break;

		case AT_BOOL:
			if ( pAtt1->GetValue<bool>() != pAtt2->GetValue<bool>() )
				return false;
			break;

		case AT_INT:
			if ( pAtt1->GetValue<int>() != pAtt2->GetValue<int>() )
				return false;
			break;

		case AT_STRING:
			if ( Q_stricmp( pAtt1->GetValueString(), pAtt2->GetValueString() ) )
				return false;
			break;

		case AT_VECTOR2:
			if ( pAtt1->GetValue<Vector2D>() != pAtt2->GetValue<Vector2D>() )
				return false;
			break;

		case AT_VECTOR3:
			if ( pAtt1->GetValue<Vector>() != pAtt2->GetValue<Vector>() )
				return false;
			break;

		case AT_VECTOR4:
			if ( pAtt1->GetValue<Vector4D>() != pAtt2->GetValue<Vector4D>() )
				return false;
			break;

		case AT_COLOR:
			if ( pAtt1->GetValue<Color>() != pAtt2->GetValue<Color>() )
				return false;
			break;
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Returns true if a array attribute is equal
//-----------------------------------------------------------------------------
static bool IsArrayEqual( CDmElement *pElement1, CDmElement *pElement2, const char *pArrayAttribute )
{
	CDmrElementArray<> arr1( pElement1, pArrayAttribute );
	CDmrElementArray<> arr2( pElement2, pArrayAttribute );
	int nCount = arr1.IsValid() ? arr1.Count() : 0;
	int nCount2 = arr2.IsValid() ? arr2.Count() : 0;
	if ( nCount != nCount2 )
		return false;
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !IsScalarAttributesEqual( arr1[i], arr2[i] ) )
			return false;
	}

	return true;

}


//-----------------------------------------------------------------------------
// Returns true if a def is equal
//-----------------------------------------------------------------------------
static bool IsDefEqual( CDmElement *pElement1, CDmElement *pElement2 )
{
	if ( !IsScalarAttributesEqual( pElement1, pElement2 ) )
		return false;
	if ( !IsArrayEqual( pElement1, pElement2, "renderer" ) )
		return false;
	if ( !IsArrayEqual( pElement1, pElement2, "operator" ) )
		return false;
	if ( !IsArrayEqual( pElement1, pElement2, "initializer" ) )
		return false;
	if ( !IsArrayEqual( pElement1, pElement2, "emitter" ) )
		return false;
	if ( !IsArrayEqual( pElement1, pElement2, "forcegenerator" ) )
		return false;
	if ( !IsArrayEqual( pElement1, pElement2, "constraint" ) )
		return false;
	if ( !IsArrayEqual( pElement1, pElement2, "children" ) )
		return false;

	CDmrElementArray<> arr1( pElement1, "children" );
	CDmrElementArray<> arr2( pElement2, "children" );
	int nCount = arr1.IsValid() ? arr1.Count() : 0;
	int nCount2 = arr2.IsValid() ? arr2.Count() : 0;
	if ( nCount != nCount2 )
		return false;
	for ( int i = 0; i < nCount; ++i )
	{
		CDmElement *pChild1 = arr1[i]->GetValueElement<CDmElement>( "child" );
		CDmElement *pChild2 = arr2[i]->GetValueElement<CDmElement>( "child" );
		if ( !pChild1 || !pChild2 )
		{
			if ( pChild1 || pChild2 )
				return false;
			continue;
		}
		if ( !IsDefEqual( pChild1, pChild2 ) )
			return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Finds or adds unique defs
//-----------------------------------------------------------------------------
static CDmElement* FindOrAddDef( CDmElement *pDef, CUtlVector< CDmElement* > &defs, CUtlStringMap< int > &uniqueNames )
{
	int nCount = defs.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( Q_stricmp( pDef->GetName(), defs[i]->GetName() ) )
			continue;

		if ( IsDefEqual( pDef, defs[i] ) )
			return defs[i];

		char pTemp[128], pTemp2[128];
		UniqueIdToString( pDef->GetId(), pTemp, sizeof(pTemp) );
		UniqueIdToString( defs[i]->GetId(), pTemp2, sizeof(pTemp2) );

		Warning( "WARNING: Discovered duplicated systems with different values!\n" );
		Warning( "\tName %s\n\tid #1 %s\n\tid #2 %s\n", pDef->GetName(), pTemp, pTemp2 );
	}

	defs.AddToTail( pDef );
	if ( !uniqueNames.Defined( pDef->GetName() ) )
	{
		uniqueNames[ pDef->GetName() ] = 1;
	}
	else
	{
		int nCopyCount = ++uniqueNames[ pDef->GetName() ];
		char pNewName[512];
		Q_snprintf( pNewName, sizeof(pNewName), "%s Version #%d", pDef->GetName(), nCopyCount );
		pDef->SetName( pNewName );
	}

	return pDef;
}


//-----------------------------------------------------------------------------
// Replace references 
//-----------------------------------------------------------------------------
static void	ReplaceChildReferences( CDmElement *pElement, CDmElement *pOldVersion, CDmElement *pNewVersion )
{
	for ( CDmAttribute *pAtt = pElement->FirstAttribute(); pAtt; pAtt = pAtt->NextAttribute() )
	{
		if ( pAtt->GetType() == AT_ELEMENT )
		{
			CDmElement *pCurrent = pAtt->GetValueElement<CDmElement>();
			if ( pCurrent == pOldVersion )
			{
				pAtt->SetValue( pNewVersion );
			}
			else
			{
				ReplaceChildReferences( pCurrent, pOldVersion, pNewVersion );
			}
			continue;
		}

		if ( pAtt->GetType() == AT_ELEMENT_ARRAY )
		{
			CDmrElementArray<> arr( pAtt );
			int nCount = arr.Count();
			for ( int i = 0; i < nCount; ++i )
			{
				if ( arr[i] == pOldVersion )
				{
					arr.Set( i, pNewVersion );
				}
				else
				{
					ReplaceChildReferences( arr[i], pOldVersion, pNewVersion );
				}
			}
			continue;
		}
	}
}


//-----------------------------------------------------------------------------
// Add unique particle defs 
//-----------------------------------------------------------------------------
static void	AddUniqueElementsToList( CDmElement *pElement, CUtlVector<CDmElement*> &list )
{
	Assert( !Q_stricmp( pElement->GetTypeString(), "DmeParticleSystemDefinition" ) );
	int nCount = list.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( IsUniqueIdEqual( pElement->GetId(), list[i]->GetId() ) )
		{
			if ( pElement != list[i] )
			{
				Warning( "Encountered two different elements with the same unique id!!\n" );
				Warning( "\tElement #1: %s Element #2: %s\n", pElement->GetName(), list[i]->GetName() );
			}
			return;
		}
	}
	list.AddToTail( pElement );

	CDmrElementArray<> children( pElement, "children" );
	if ( !children.IsValid() )
		return;

	int nChildCount = children.Count();
	for ( int i = 0; i < nChildCount; ++i )
	{
		CDmElement *pChild = children[i]->GetValueElement<CDmElement>( "child" );
		if ( !pChild )
			continue;
		AddUniqueElementsToList( pChild, list ); 
	}
}


//-----------------------------------------------------------------------------
// Replace references 
//-----------------------------------------------------------------------------
static void	BuildStartingDefList( CDmElement *pElement, CDmElement *pOldVersion, CDmElement *pNewVersion )
{
	for ( CDmAttribute *pAtt = pElement->FirstAttribute(); pAtt; pAtt = pAtt->NextAttribute() )
	{
		if ( pAtt->GetType() == AT_ELEMENT )
		{
			CDmElement *pCurrent = pAtt->GetValueElement<CDmElement>();
			if ( pCurrent == pOldVersion )
			{
				pAtt->SetValue( pNewVersion );
			}
			else
			{
				ReplaceChildReferences( pCurrent, pOldVersion, pNewVersion );
			}
			continue;
		}

		if ( pAtt->GetType() == AT_ELEMENT_ARRAY )
		{
			CDmrElementArray<> arr( pAtt );
			int nCount = arr.Count();
			for ( int i = 0; i < nCount; ++i )
			{
				if ( arr[i] == pOldVersion )
				{
					arr.Set( i, pNewVersion );
				}
				else
				{
					ReplaceChildReferences( arr[i], pOldVersion, pNewVersion );
				}
			}
			continue;
		}
	}
}


//-----------------------------------------------------------------------------
// Fixes up a PCF file
//-----------------------------------------------------------------------------
void CPCFFixApp::FixupPCFFile( CDmElement *pRoot )
{
	CUtlVector< CDmElement* > startingDefs;
	CUtlVector< CDmElement* > defs;
	CUtlStringMap< int > uniqueNames;

	CDmrElementArray<> rootDefs( pRoot, "particleSystemDefinitions" );
	int nCount = rootDefs.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		AddUniqueElementsToList( rootDefs[i], startingDefs );
	}

	nCount = startingDefs.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmElement *pActualDef = FindOrAddDef( startingDefs[i], defs, uniqueNames );
		if ( pActualDef == startingDefs[i] )
			continue;

		// Now replace all child references to startingDefs[i] with references to pActualDef instead.
		ReplaceChildReferences( pRoot, startingDefs[i], pActualDef );
	}

	// Reconstruct the definition list based on the actual unique ones
	rootDefs.RemoveAll();
	int nUniqueCount = defs.Count();
	for ( int i = 0; i < nUniqueCount; ++i )
	{
		rootDefs.AddToTail( defs[i] );
	}

	if ( nCount != nUniqueCount )
	{
		Warning( "*** Removed %d duplicated particle systems!\n", nCount - nUniqueCount );
	}
	else
	{
		Warning( "Removed no duplicated particle systems. This file started out ok.\n" );
	}
}


//-----------------------------------------------------------------------------
// Print help
//-----------------------------------------------------------------------------
void CPCFFixApp::PrintHelp( )
{
	Msg( "Usage: pcffix -i <in .pcf file> [-nop4]\n" );
	Msg( "\t-i\t: Source .PCF file to fix up (eliminate copied particle system definitions.)\n" );
	Msg( "\t-nop4\t: Disables auto perforce checkout/add.\n" );
	Msg( "\t-vproject\t: Specifies path to a gameinfo.txt file (which mod to build for).\n" );
}


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
int CPCFFixApp::Main()
{
	g_pDataModel->SetUndoEnabled( false );
	g_pDataModel->OnlyCreateUntypedElements( true );
	g_pDataModel->SetDefaultElementFactory( NULL );

	// This bit of hackery allows us to access files on the harddrive
	g_pFullFileSystem->AddSearchPath( "", "LOCAL", PATH_ADD_TO_HEAD ); 

	if ( CommandLine()->CheckParm( "-h" ) || CommandLine()->CheckParm( "-help" ) )
	{
		PrintHelp();
		return 0;
	}

	// Do Perforce Stuff
	if ( CommandLine()->FindParm( "-nop4" ) )
	{
		g_p4factory->SetDummyMode( true );
	}

	g_p4factory->SetOpenFileChangeList( "Fixed PCF files" );

	const char *pPCFFile = CommandLine()->ParmValue( "-i" );
	if ( !pPCFFile )
	{
		PrintHelp();
		return 0;
	}

	CDmElement *pRoot;
	if ( g_pDataModel->RestoreFromFile( pPCFFile, NULL, "pcf", &pRoot, CR_DELETE_NEW ) == DMFILEID_INVALID )
	{
		Error( "Encountered an error reading file \"%s\"!\n", pPCFFile );
		return -1;
	}

	FixupPCFFile( pRoot );

	CP4AutoEditFile checkout( pPCFFile );
	const char *pOutEncoding = g_pDataModel->GetDefaultEncoding( "pcf" );
	if ( !g_pDataModel->SaveToFile( pPCFFile, NULL, pOutEncoding, "pcf", pRoot ) )
	{
		Error( "Encountered an error writing file \"%s\"!\n", pPCFFile );
		return -1;
	}

	g_pDataModel->RemoveFileId( pRoot->GetFileId() );
	return -1;
}