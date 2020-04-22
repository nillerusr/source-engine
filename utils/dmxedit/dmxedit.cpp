//=============================================================================
//
//========= Copyright Valve Corporation, All rights reserved. ============//
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// $Header: $
// $NoKeywords: $
//
// Converts from any one DMX file format to another
// Can also output SMD or a QCI header from DMX input
//
//=============================================================================



// Standard includes

#include <conio.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>


// Valve includes
#include "vstdlib/cvar.h"
#include "tier1/tier1.h"
#include "tier2/tier2.h"
#include "tier2/tier2dm.h"
#include "tier3/tier3.h"
#include "filesystem.h"
#include "vstdlib/iprocessutils.h"
#include "tier0/icommandline.h"
#include "istudiorender.h"
#include "vphysics_interface.h"
#include "datacache/idatacache.h"
#include "datacache/imdlcache.h"
#include "appframework/AppFramework.h"
#include "dmserializers/idmserializers.h"
#include "datamodel/idatamodel.h"
#include "datamodel/dmelement.h"
#include "movieobjects/dmeanimationset.h"
#include "movieobjects/dmedag.h"
#include "movieobjects/dmemesh.h"
#include "movieobjects/dmevertexdata.h"
#include "movieobjects/dmeselection.h"
#include "movieobjects/dmecombinationoperator.h"
#include "movieobjects/dmobjserializer.h"
#include "movieobjects/dmsmdserializer.h"
#include "movieobjects/dmmeshcomp.h"
#include "movieobjects/dmemodel.h"
#include "movieobjects/dmedccmakefile.h"
#include "movieobjects/dmmeshutils.h"
#include "tier1/utlstring.h"
#include "tier1/utlbuffer.h"
#include "tier2/p4helpers.h"


// Lua includes
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>


// Local includes
#include "dmxedit.h"


//-----------------------------------------------------------------------------
// Statics
//-----------------------------------------------------------------------------
LuaFunc_s *LuaFunc_s::s_pFirstFunc = NULL;
CDmxEdit LuaFunc_s::m_dmxEdit;

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const CDmxEdit::CFalloffType CDmxEdit::STRAIGHT( CDmeMesh::STRAIGHT );
const CDmxEdit::CFalloffType CDmxEdit::LINEAR( CDmeMesh::STRAIGHT );
const CDmxEdit::CFalloffType CDmxEdit::BELL( CDmeMesh::BELL );
const CDmxEdit::CFalloffType CDmxEdit::SMOOTH( CDmeMesh::BELL );
const CDmxEdit::CFalloffType CDmxEdit::SPIKE( CDmeMesh::SPIKE );
const CDmxEdit::CFalloffType CDmxEdit::DOME( CDmeMesh::DOME );

const CDmxEdit::CSelectOp CDmxEdit::ADD( CDmxEdit::CSelectOp::kAdd );
const CDmxEdit::CSelectOp CDmxEdit::SUBTRACT( CDmxEdit::CSelectOp::kSubtract );
const CDmxEdit::CSelectOp CDmxEdit::TOGGLE( CDmxEdit::CSelectOp::kToggle );
const CDmxEdit::CSelectOp CDmxEdit::INTERSECT( CDmxEdit::CSelectOp::kIntersect );
const CDmxEdit::CSelectOp CDmxEdit::REPLACE( CDmxEdit::CSelectOp::kReplace );

const CDmxEdit::CSelectType CDmxEdit::ALL( CDmxEdit::CSelectType::kAll );
const CDmxEdit::CSelectType CDmxEdit::NONE( CDmxEdit::CSelectType::kNone );

const CDmxEdit::CObjType CDmxEdit::ABSOLUTE( CDmxEdit::CObjType::kAbsolute );
const CDmxEdit::CObjType CDmxEdit::RELATIVE( CDmxEdit::CObjType::kRelative );

const CDmxEdit::CDistanceType CDmxEdit::DIST_ABSOLUTE( CDmeMesh::DIST_ABSOLUTE );
const CDmxEdit::CDistanceType CDmxEdit::DIST_RELATIVE( CDmeMesh::DIST_RELATIVE );
const CDmxEdit::CDistanceType CDmxEdit::DIST_DEFAULT( CDmeMesh::DIST_DEFAULT );

const CDmxEdit::CAxisType CDmxEdit::XAXIS( CDmxEdit::CAxisType::kXAxis );
const CDmxEdit::CAxisType CDmxEdit::YAXIS( CDmxEdit::CAxisType::kYAxis );
const CDmxEdit::CAxisType CDmxEdit::ZAXIS( CDmxEdit::CAxisType::kXAxis );

const CDmxEdit::CHalfType CDmxEdit::LEFT( CDmxEdit::CHalfType::kLeft );
const CDmxEdit::CHalfType CDmxEdit::RIGHT( CDmxEdit::CHalfType::kRight );


//=============================================================================
// CDmxEdit definition
//=============================================================================


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmxEdit::CDmxEdit()
: m_pRoot( NULL )
, m_pMesh( NULL )
, m_pCurrentSelection( NULL )
, m_distanceType( CDmeMesh::DIST_ABSOLUTE )
{}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmxEdit::SetScriptFilename( const char *pFilename )
{
	m_scriptFilename = pFilename;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::Load( const char *pFilename, const CObjType &loadType /* = DIST_ABSOLUTE */ )
{
	Unload();

	const int sLen = Q_strlen( pFilename );
	if ( sLen > 4 && !Q_stricmp( pFilename + sLen - 4, ".dmx" ) )
	{
		g_pDataModel->RestoreFromFile( pFilename, NULL, NULL, &m_pRoot );

		if ( !m_pRoot )
			return SetErrorString( "DMX Load Failed" );

		g_p4factory->AccessFile( pFilename )->Add();

		// Find the first mesh with any deltas
		// Look for model

		CDmeDag *pDag = m_pRoot->GetValueElement< CDmeDag >( "model" );
		if ( pDag )
		{
			CDmeMesh *pFirstMesh = NULL;

			CUtlStack< CDmeDag * > traverseStack;
			traverseStack.Push( pDag );

			while ( traverseStack.Count() )
			{
				traverseStack.Pop( pDag );
				if ( !pDag )
					continue;
				
				for ( int i = pDag->GetChildCount() - 1; i >= 0; --i )
				{
					traverseStack.Push( pDag->GetChild( i ) );
				}

				CDmeMesh *pMesh = CastElement< CDmeMesh >( pDag->GetShape() );
				if ( !pMesh )
					continue;

				if ( pFirstMesh == NULL )
				{
					pFirstMesh = pMesh;
				}

				if ( pMesh->DeltaStateCount() )
				{
					m_pMesh = pMesh;
					break;
				}
			}

			if ( !m_pMesh )
			{
				if ( pFirstMesh )
				{
					m_pMesh = pFirstMesh;
					LuaWarning( "Cannot Find A DmeMesh With Any Delta States In File, Using First Found Mesh %s", m_pMesh->GetName() );
				}
				else
				{
					LuaWarning( "Cannot Find A DmeMesh In File" );
				}
			}
		}
		else
		{
			LuaWarning( "Cannot Find A DmeModel As Element Dme Root Object: %s in %s", m_pRoot->GetName(), pFilename );
		}
	}
	else if ( sLen > 4 && !V_stricmp( pFilename + sLen - 4, ".smd" ) )
	{
		m_pRoot = CDmSmdSerializer().ReadSMD( pFilename, &m_pMesh );

		if ( !m_pRoot )
			return SetErrorString( "OBJ Load Failed" );
	}
	else
	{
		m_pRoot = CDmObjSerializer().ReadOBJ( pFilename, &m_pMesh, true, loadType() == CObjType::kAbsolute );

		if ( !m_pRoot )
			return SetErrorString( "OBJ Load Failed" );
	}

	m_filename = pFilename;

	if ( m_pMesh )
	{
		CDmeVertexData *pBind( m_pMesh->FindBaseState( "bind" ) );
		if ( pBind )
		{
			// Ensure "work" isn't saved!
			CDmeVertexData *pWork = m_pMesh->FindOrCreateBaseState( "__dmxEdit_work" );

			if ( pWork )
			{
				pBind->CopyTo( pWork );
				m_pMesh->SetCurrentBaseState( "__dmxEdit_work" );
			}
		}

		m_pCurrentSelection = CreateElement< CDmeSingleIndexedComponent >( "selection", m_pRoot->GetFileId() );
		m_pMesh->SetValue( "selection", m_pCurrentSelection );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Lua Interface to CDmxEdit::Load
//-----------------------------------------------------------------------------
LUA_COMMAND(
	Load,
	"< $file.dmx | $file.obj >, [ $loadType ]",
	"Replaces the current scene with the specified scene.  The current mesh will be set to the first mesh with a combination operator found in the new scene.  $loadType is one of \"absolute\" or \"relative\".  If not specified, \"absolute\" is assumed." )
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	const char *pFilename = luaL_checkstring( pLuaState, 1 );

	const char *pLoadType = NULL;

	if ( lua_isboolean( pLuaState, 2 ) )
	{
		if ( lua_toboolean( pLuaState, 2 ) )
		{
			pLoadType = "Absolute";
		}
		else
		{
			pLoadType = "Relative";
		}
	}
	else if ( lua_isstring( pLuaState, 2 ) )
	{
		pLoadType = lua_tostring( pLuaState, 2 );
	}

	if ( pLoadType )
	{
		if ( LuaFunc_s::m_dmxEdit.Load( pFilename, pLoadType ) )
			return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );
	}
	else
	{
		if ( LuaFunc_s::m_dmxEdit.Load( pFilename ) )
			return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );
	}

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
// Imports the Dmx Model file into the current file
//-----------------------------------------------------------------------------
bool CDmxEdit::Import( const char *pFilename, const char *pParentName )
{
	if ( !m_pRoot )
	{
		return Load( pFilename );
	}

	CDmeModel *pDstModel = m_pRoot->GetValueElement< CDmeModel >( "model" );
	if ( !pDstModel )
		return SetErrorString( "Can't Find Existing Model Node" );

	CDisableUndoScopeGuard sgDisableUndo;

	CDmElement *pRoot = NULL;
	g_pDataModel->RestoreFromFile( pFilename, NULL, NULL, &pRoot, CR_FORCE_COPY );

	if ( !pRoot )
		return SetErrorString( "Can't Load DMX File" );

	g_p4factory->AccessFile( pFilename )->Add();

	CDmeDag *pSrcModel = pRoot->GetValueElement< CDmeDag >( "model" );
	if ( !pSrcModel )
	{
		g_pDataModel->UnloadFile( pRoot->GetFileId() );
		return SetErrorString( "Can't Find \"model\" Element On Root Node In DMX File" );
	}

	int nSkinningJointIndex = -1;

	if ( pParentName )
	{
		CDmeTransform *pJoint = NULL;

		const int nJointTransformCount = pDstModel->GetJointTransformCount();
		for ( int i = 0; i < nJointTransformCount; ++i )
		{
			pJoint = pDstModel->GetJointTransform( i );
			if ( !Q_stricmp( pJoint->GetName(), pParentName ) )
			{
				nSkinningJointIndex = i;
				break;
			}
		}

		if ( nSkinningJointIndex < 0 )
		{
			LuaWarning( "Couldn't Find Parent Bone \"%s\"", pParentName );
		}
	}

	CleanupWork();

	//	CDmeCombinationOperator *pCombo = m_pRoot->GetValueElement< CDmeCombinationOperator >( "combinationOperator" );

	// Initialize traversal stack to just model's children (don't want to touch model)
	CUtlStack< CDmeDag * > traverseStack;
	for ( int i = pSrcModel->GetChildCount() - 1; i >= 0; --i )
	{
		traverseStack.Push( pSrcModel->GetChild( i ) );
	}

	CDmeDag *pDag;
	while ( traverseStack.Count() )
	{
		traverseStack.Pop( pDag );
		if ( !pDag )
			continue;

		for ( int i = pDag->GetChildCount() - 1; i >= 0; --i )
		{
			traverseStack.Push( pDag->GetChild( i ) );
		}

		CDmeMesh *pMesh = CastElement< CDmeMesh >( pDag->GetShape() );
		if ( pMesh )
		{
			CDmMeshUtils::Merge( pMesh, m_pMesh, nSkinningJointIndex );
		}
	}

	g_pDataModel->UnloadFile( pRoot->GetFileId() );

	CreateWork();

	return true;
}


//-----------------------------------------------------------------------------
// Lua Interface for CDmxEdit::Import
//-----------------------------------------------------------------------------
LUA_COMMAND(
	Import,
	"< $file.dmx >, [ $parentBone ]",
	"Imports the specified DMX model into the scene.  The imported model can optionally be parented (which implictly skins it) to a specified bone in the existing scene via $parentBone." )
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	const char *pFilename = luaL_checkstring( pLuaState, 1 );
	const char *pParentBone = lua_isstring( pLuaState, 2 ) ? lua_tostring( pLuaState, 2 ) : NULL;

	if ( LuaFunc_s::m_dmxEdit.Import( pFilename, pParentBone ) )
		return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::ListDeltas()
{
	if ( !m_pMesh )
		return SetErrorString( "No Mesh" );

	const int nDeltas( m_pMesh->DeltaStateCount() );
	if ( nDeltas <= 0 )
		return SetErrorString( "No Deltas Defined On Mesh: %s", m_pMesh->GetName() );

	for ( int i( 0 ); i < nDeltas; ++i )
	{
		Msg( "// Delta %d: %s\n", i, m_pMesh->GetDeltaState( i )->GetName() );
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	ListDeltas,
	"",
	"Prints a list of all of the deltas present in the current mesh" )
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	if ( LuaFunc_s::m_dmxEdit.ListDeltas() )
		return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CDmxEdit::DeltaCount()
{
	if ( !m_pMesh )
		return SetErrorString( "No Mesh" );

	return m_pMesh->DeltaStateCount();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	DeltaCount,
	"",
	"Returns the number of deltas in the current mesh" )
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	lua_pushnumber( pLuaState, LuaFunc_s::m_dmxEdit.DeltaCount() );

	return 1;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const char *CDmxEdit::DeltaName( int nDeltaStateIndex )
{
	if ( !m_pMesh )
	{
		SetErrorString( "No Mesh" );
		return NULL;
	}

	if ( nDeltaStateIndex >= m_pMesh->DeltaStateCount() )
	{
		SetErrorString( "Delta Index Too High" );
		return NULL;
	}

	return m_pMesh->GetDeltaState( nDeltaStateIndex )->GetName();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	DeltaName,
	"< #deltaIndex >",
	"Returns the name of the delta at the specified index.  Use DeltaCount() to get a count of how many deltas are present" )
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	const int nDeltaIndex = luaL_checknumber( pLuaState, 1 );

	const char *pDeltaName = LuaFunc_s::m_dmxEdit.DeltaName( nDeltaIndex );
	if ( pDeltaName )
	{
		lua_pushstring( pLuaState, pDeltaName );
		return 1;
	}

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmxEdit::Unload()
{
	CDisableUndoScopeGuard sgDisableUndo;

	if ( m_pCurrentSelection )
	{
		g_pDataModel->DestroyElement( m_pCurrentSelection->GetHandle() );
	}

	if ( m_pRoot )
	{
		g_pDataModel->UnloadFile( m_pRoot->GetFileId() );
	}

	m_filename = "";
	m_pRoot = NULL;
	m_pMesh = NULL;
	m_pCurrentSelection = NULL;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::ImportComboRules( const char *pFilename, bool bOverwrite /* = true */, bool bPurgeDeltas /* = false */ )
{
	if ( !m_pMesh )
		return SetErrorString( "No Mesh" );

	CDmeCombinationOperator *pDestCombo( FindReferringElement< CDmeCombinationOperator >( m_pMesh, "targets" ) );
	if ( !pDestCombo )
		return SetErrorString( "No DmeCombinationOperator On Mesh \"%s\"", m_pMesh->GetName() );

	CDisableUndoScopeGuard sg;

	CDmElement *pRoot = NULL;
	g_pDataModel->RestoreFromFile( pFilename, NULL, NULL, &pRoot, CR_FORCE_COPY );

	if ( !pRoot )
		return SetErrorString( "File Cannot Be Read" );

	g_p4factory->AccessFile( pFilename )->Add();

	// Try to find a combination system in the file
	CDmeCombinationOperator *pCombo = CastElement< CDmeCombinationOperator >( pRoot );
	if ( !pCombo )
	{
		pCombo = pRoot->GetValueElement< CDmeCombinationOperator >( "combinationOperator" );
	}

	if ( !pCombo )
		return SetErrorString( "No DmeCombinationOperator Found In File" );

	ImportCombinationControls( pCombo, pDestCombo, bOverwrite );
	ImportDominationRules( pDestCombo, pCombo, bOverwrite );

	pDestCombo->SetToDefault();

	g_pDataModel->UnloadFile( pRoot->GetFileId() );

	if ( bPurgeDeltas )
	{
		CDmMeshUtils::PurgeUnusedDeltas( m_pMesh );
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	ImportComboRules,
	"< $rules.dmx >, [ #bOverwrite ], [ #bPurgeDeltas ]",
	"Imports the combintion rules from the specified dmx file and replaces the current rules with those from the file is #bOverwrite specified or is true.  If #bOverwrite is false then any existing controls that are not in the imported rules file are preserved.  If #bPurgeDeltas is specified and is true, then any delta states which are no longer referred to by any combination rule or control will be purged. ")
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	const char *pFilename = luaL_checkstring( pLuaState, 1 );

	if ( lua_isboolean( pLuaState, 2 ) ) 
	{
		if ( lua_isboolean( pLuaState, 3 ) )
		{
			if ( LuaFunc_s::m_dmxEdit.ImportComboRules( pFilename, lua_toboolean( pLuaState, 2 ) != 0, lua_toboolean( pLuaState, 3 ) != 0 ) )
				return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );
		}
		else
		{
			if ( LuaFunc_s::m_dmxEdit.ImportComboRules( pFilename, lua_toboolean( pLuaState, 2 ) != 0 ) )
				return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );
		}
	}
	else
	{
		if ( LuaFunc_s::m_dmxEdit.ImportComboRules( pFilename ) )
			return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );
	}

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
// Import dominance rules from this operator
//-----------------------------------------------------------------------------
void CDmxEdit::ImportDominationRules( CDmeCombinationOperator *pDestComboOp, CDmeCombinationOperator *pSrcComboOp, bool bOverwrite )
{
	if ( bOverwrite )
	{
		pDestComboOp->RemoveAllDominationRules();
	}

	// TODO: Detect conflicts if not overwriting

	// Now deal with dominance rules
	int nRuleCount = pSrcComboOp->DominationRuleCount();
	for ( int i = 0; i < nRuleCount; ++i )
	{
		bool bMismatch = false;

		// Only add dominance rule if *all* raw controls are present and the rule doesn't already exist
		CDmeCombinationDominationRule *pSrcRule = pSrcComboOp->GetDominationRule( i );
		int nDominatorCount = pSrcRule->DominatorCount();
		for ( int j = 0; j < nDominatorCount; ++j )
		{
			const char *pDominatorName = pSrcRule->GetDominator( j );
			if ( !pDestComboOp->HasRawControl( pDominatorName ) )
			{
				bMismatch = true;
				break;
			}
		}

		int nSuppressedCount = pSrcRule->SuppressedCount();
		for ( int j = 0; j < nSuppressedCount; ++j )
		{
			const char *pSuppressedName = pSrcRule->GetSuppressed( j );
			if ( !pDestComboOp->HasRawControl( pSuppressedName ) )
			{
				bMismatch = true;
				break;
			}
		}

		if ( bMismatch )
			continue;

		pDestComboOp->AddDominationRule( pSrcRule );
	}
}


//-----------------------------------------------------------------------------
// Returns true if the the control has only 1 raw control and it's the same
// name as the control
//-----------------------------------------------------------------------------
bool IsDefaultControl( CDmeCombinationOperator *pCombo, int nIndex )
{
	if ( pCombo->GetRawControlCount( nIndex ) == 1 && !Q_strcmp( pCombo->GetControlName( nIndex ), pCombo->GetRawControlName( nIndex, 0 ) ) )
		return true;

	return false;
}


//-----------------------------------------------------------------------------
// Returns true if the all raw controls of A are found in B
//-----------------------------------------------------------------------------
bool IsASubsetOfB( CDmeCombinationOperator *pComboA, int nIndexA, CDmeCombinationOperator *pComboB, int nIndexB )
{
	const int nCountA = pComboA->GetRawControlCount( nIndexA );
	const int nCountB = pComboB->GetRawControlCount( nIndexB );

	for ( int i = 0; i < nCountA; ++i )
	{
		bool bFound = false;

		const char *pNameA = pComboA->GetRawControlName( nIndexA, i );

		for ( int j = 0; j < nCountB; ++j )
		{
			if ( !Q_stricmp( pNameA, pComboB->GetRawControlName( nIndexB, j ) ) )
			{
				bFound = true;
				break;
			}
		}

		if ( !bFound )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Returns true if the two combination controls have the same raw controls
//-----------------------------------------------------------------------------
bool AreControlsTheSame( CDmeCombinationOperator *pComboA, int nIndexA, CDmeCombinationOperator *pComboB, int nIndexB )
{
	const int nCountA = pComboA->GetRawControlCount( nIndexA );
	const int nCountB = pComboB->GetRawControlCount( nIndexB );

	if ( nCountA != nCountB )
		return false;

	// This is a test for equality now since we've verified that A & B has same number of elements
	return IsASubsetOfB( pComboA, nIndexA, pComboB, nIndexB );
}


//-----------------------------------------------------------------------------
// -1 for failure
//-----------------------------------------------------------------------------
int FindControlIndexFromRawControlName( CDmeCombinationOperator *pCombo, const char *pRawControlName )
{
	const int nControlCount = pCombo->GetControlCount();
	for ( int i = 0; i < nControlCount; ++i )
	{
		const int nRawCount = pCombo->GetRawControlCount( i );
		for ( int j = 0; j < nRawCount; ++j )
		{
			if ( !Q_stricmp( pRawControlName, pCombo->GetRawControlName( i, j ) ) )
			{
				return i;
			}
		}
	}

	return -1;
}


//-----------------------------------------------------------------------------
// Import combination rules from this operator
//-----------------------------------------------------------------------------
void CDmxEdit::ImportCombinationControls( CDmeCombinationOperator *pSrcComboOp, CDmeCombinationOperator *pDstComboOp, bool bOverwrite )
{
	if ( bOverwrite )
	{
		// Just remove all existing controls
		pDstComboOp->RemoveAllControls();
	}
	else
	{
		// Remove conflicting controls, i.e. controls in src which are controlling something that's already
		// controlled in dst

		bool bConflict = true;

		while ( bConflict )
		{
			bConflict = false;

			// Remove all controls from dst that conflict with src

			for ( int i = 0; !bConflict && i < pSrcComboOp->GetControlCount(); ++i )
			{
				const int nSrcRawCount = pSrcComboOp->GetRawControlCount( i );
				for ( int j = 0; !bConflict && j < nSrcRawCount; ++j )
				{
					const char *pSrcRawControlName = pSrcComboOp->GetRawControlName( i, j );

					const int nDstControlIndex = FindControlIndexFromRawControlName( pDstComboOp, pSrcRawControlName );
					if ( nDstControlIndex < 0 )
						continue;

					pDstComboOp->RemoveRawControl( nDstControlIndex, pSrcRawControlName );
					bConflict = true;
					break;
				}
			}
		}

		// Purge all empty ones

		bConflict = true;

		while ( bConflict )
		{
			bConflict = false;

			for ( int i = 0; !bConflict && i < pDstComboOp->GetControlCount(); ++i )
			{
				if ( pDstComboOp->GetRawControlCount( i ) == 0 )
				{
					pDstComboOp->RemoveControl( pDstComboOp->GetControlName( i ) );
					bConflict = true;
					break;
				}
			}
		}
	}

	// Iterate through all controls in the imported operator.
	// For each imported control that contains at least 1 raw control
	// for which a delta state exists, create that control
	// All conflicts were resolved previously

	CUtlVectorFixedGrowable< bool, 256 > foundMatch;

	for ( int i = 0; i < pSrcComboOp->GetControlCount(); ++i )
	{
		const char *pControlName = pSrcComboOp->GetControlName( i );

		int nRawControls = pSrcComboOp->GetRawControlCount( i );
		int nMatchCount = 0; 
		foundMatch.EnsureCount( nRawControls );
		for ( int j = 0; j < nRawControls; ++j )
		{
			const char *pRawControl = pSrcComboOp->GetRawControlName( i, j );
			foundMatch[j] = pDstComboOp->DoesTargetContainDeltaState( pRawControl );
			nMatchCount += foundMatch[j];
		}

		// No match? Don't import
		if ( nMatchCount == 0 )
		{
			continue;
		}

		//		bool bPartialMatch = ( nMatchCount != nRawControls );

		// Found a match! Let's create the control and potentially raw control
		pDstComboOp->RemoveControl( pControlName );
		bool bIsStereo = pSrcComboOp->IsStereoControl( i );
		ControlIndex_t index = pDstComboOp->FindOrCreateControl( pControlName, bIsStereo );
		pDstComboOp->SetEyelidControl( index, pSrcComboOp->IsEyelidControl( i ) );
		for ( int j = 0; j < nRawControls; ++j )
		{
			if ( foundMatch[j] )
			{
				const char *pRawControl = pSrcComboOp->GetRawControlName( i, j );
				float flWrinkleScale = pSrcComboOp->GetRawControlWrinkleScale( i, j );

				pDstComboOp->AddRawControl( index, pRawControl );
				pDstComboOp->SetWrinkleScale( index, pRawControl, flWrinkleScale );
			}
		}
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::SetState( const char *pDeltaName )
{
	if ( !m_pMesh )
		return SetErrorString( "No Mesh" );

	CDmeVertexDeltaData *pDelta = FindDeltaState( pDeltaName );
	if ( !pDelta )
		return SetErrorString( "Invalid Delta \"%s\"", pDeltaName );

	return m_pMesh->SetBaseStateToDelta( pDelta );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	SetState,
	"< $delta >",
	"Sets the current mesh to the specified $delta")
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	const char *pDeltaName = luaL_checkstring( pLuaState, 1 );

	if ( LuaFunc_s::m_dmxEdit.SetState( pDeltaName ) )
		return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::ResetState()
{
	if ( !m_pMesh )
		return SetErrorString( "No Mesh" );

	return m_pMesh->SetBaseStateToDelta( NULL );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	ResetState,
	"",
	"Resets the current mesh back to the default base state, i.e. no deltas active")
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	if ( LuaFunc_s::m_dmxEdit.ResetState() )
		return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
// Disambiguation when select is called with a selection type of a string
//-----------------------------------------------------------------------------
bool CDmxEdit::Select( const char *pSelectTypeString, CDmeSingleIndexedComponent *pPassedSelection /* = NULL */, CDmeMesh *pPassedMesh /* = NULL */ )
{
	const CSelectType selectType( pSelectTypeString );

	if ( selectType() == CSelectType::kDelta )
	{
		CDmeVertexDeltaData *pDelta = FindDeltaState( pSelectTypeString, pPassedMesh );
		if ( pDelta)
			return Select( pDelta, pPassedSelection, pPassedMesh );

		return SetErrorString( "Invalid Delta \"%s\"", pSelectTypeString );
	}

	return Select( selectType, pPassedSelection, pPassedMesh );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	Select,
	"< $delta >, [ $type ]",
	"Changes the selection based on the vertices present in the specified $delta.  $type is one of \"add\", \"subtract\", \"toggle\", \"intersect\", \"replace\".  If $type is not specified, \"add\" is assumed.")
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	const char *pSelect1 = luaL_checkstring( pLuaState, 1 );
	const char *pSelect2 = lua_tostring( pLuaState, 2 );

	if ( pSelect2 )
	{
		if ( LuaFunc_s::m_dmxEdit.Select( pSelect1, pSelect2 ) )
			return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );
	}
	else
	{
		if ( LuaFunc_s::m_dmxEdit.Select( pSelect1 ) )
			return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );
	}

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
// Selects vertices of the working mesh that change position in the give delta
//-----------------------------------------------------------------------------
bool CDmxEdit::Select( CDmeVertexDeltaData *pDelta, CDmeSingleIndexedComponent *pPassedSelection /* = NULL */, CDmeMesh *pPassedMesh /* = NULL */ )
{
	if ( !pDelta )
		return SetErrorString( "Invalid Delta" );

	CDmeMesh *pMesh = pPassedMesh ? pPassedMesh : m_pMesh;

	if ( !pMesh )
		return SetErrorString( "No Mesh" );

	CDmeSingleIndexedComponent *pSelection = pPassedSelection ? pPassedSelection : m_pCurrentSelection;

	if ( !pSelection )
		return SetErrorString( "No Selection To Manipulate" );

	pMesh->SelectVerticesFromDelta( pDelta, pSelection );

	return true;
}


//-----------------------------------------------------------------------------
// Selects vertices of the working mesh that change position in the give delta
//-----------------------------------------------------------------------------
bool CDmxEdit::Select( const CSelectType &selectType, CDmeSingleIndexedComponent *pPassedSelection /* = NULL */, CDmeMesh *pPassedMesh /* = NULL */ )
{
	if ( selectType() == CSelectType::kDelta )
		return SetErrorString( "Called Via CSelectType... Wacky!" );

	CDmeSingleIndexedComponent *pSelection = pPassedSelection ? pPassedSelection : m_pCurrentSelection;

	if ( !pSelection )
		return SetErrorString( "No Selection To Manipulate" );

	if ( selectType() == CSelectType::kNone )
	{
		m_pCurrentSelection->Clear();
		return true;
	}

	if ( selectType() != CSelectType::kAll )
		return false;

	CDmeMesh *pMesh = pPassedMesh ? pPassedMesh : m_pMesh;

	if ( !pMesh )
		return SetErrorString( "No Mesh" );

	pMesh->SelectAllVertices( pSelection );

	return true;
}


//-----------------------------------------------------------------------------
// Disambiguation when select is called with a selection type of a string
//-----------------------------------------------------------------------------
bool CDmxEdit::Select( const CSelectOp &selectOp, const char *pSelectTypeString,
					  CDmeSingleIndexedComponent *pPassedSelection /* = NULL */, CDmeMesh *pPassedMesh /* = NULL */ )
{
	const CSelectType selectType( pSelectTypeString );
	if ( selectType() == CSelectType::kDelta )
	{
		CDmeVertexDeltaData *pDelta = FindDeltaState( pSelectTypeString, pPassedMesh );
		if ( pDelta)
			return Select( selectOp, pDelta, pPassedSelection, pPassedMesh );

		return SetErrorString( "Invalid Delta" );
	}

	return Select( selectOp, selectType, pPassedSelection, pPassedMesh );
}


//-----------------------------------------------------------------------------
// Selects vertices of the working mesh that change position in the give delta
//-----------------------------------------------------------------------------
bool CDmxEdit::Select( const CSelectOp &selectOp, CDmeVertexDeltaData *pDelta, CDmeSingleIndexedComponent *pPassedSelection /* = NULL */, CDmeMesh *pPassedMesh /* = NULL */ )
{
	if ( !pDelta )
		return SetErrorString( "Invalid Delta" );

	CDmeSingleIndexedComponent *pSelection = pPassedSelection ? pPassedSelection : m_pCurrentSelection;

	if ( !pSelection )
		return SetErrorString( "No Selection To Manipulate" );

	CDmeSingleIndexedComponent *pTempSelection = CreateElement< CDmeSingleIndexedComponent >( "tempSelection", pSelection->GetFileId() );
	if ( !pTempSelection )
		return false;

	const bool retVal = Select( pDelta, pTempSelection, pPassedMesh );

	if ( retVal )
	{
		Select( selectOp, pSelection, pTempSelection );
	}

	g_pDataModel->DestroyElement( pTempSelection->GetHandle() );

	return retVal;
}


//-----------------------------------------------------------------------------
// Selects vertices of the working mesh that change position in the give delta
//-----------------------------------------------------------------------------
bool CDmxEdit::Select( const CSelectOp &selectOp, const CSelectType &selectType, CDmeSingleIndexedComponent *pPassedSelection /* = NULL */, CDmeMesh *pPassedMesh /* = NULL */ )
{
	CDmeSingleIndexedComponent *pSelection = pPassedSelection ? pPassedSelection : m_pCurrentSelection;

	if ( !pSelection )
		return SetErrorString( "No Selection To Manipulate" );

	CDmeSingleIndexedComponent *pTempSelection = CreateElement< CDmeSingleIndexedComponent >( "tempSelection", pSelection->GetFileId() );
	if ( !pTempSelection )
		return false;

	const bool retVal = Select( selectType, pTempSelection, pPassedMesh );

	if ( retVal )
	{
		Select( selectOp, pSelection, pTempSelection );
	}

	g_pDataModel->DestroyElement( pTempSelection->GetHandle() );

	return retVal;
}


//-----------------------------------------------------------------------------
// Combines the two selections via the selectOp and puts result into pOriginal
//-----------------------------------------------------------------------------
bool CDmxEdit::Select( const CSelectOp &selectOp, CDmeSingleIndexedComponent *pOriginal, const CDmeSingleIndexedComponent *pNew )
{
	if ( !pOriginal || !pNew )
		return false;

	switch ( selectOp() )
	{
	case CSelectOp::kAdd:
		pOriginal->Add( pNew );
		break;
	case CSelectOp::kSubtract:
		pOriginal->Subtract( pNew );
		break;
	case CSelectOp::kToggle:
		{
			CDmeSingleIndexedComponent *pIntersection = CreateElement< CDmeSingleIndexedComponent >( "intersection", pOriginal->GetFileId() );
			if ( !pIntersection )
				return false;

			CDmeSingleIndexedComponent *pNewCopy = CreateElement< CDmeSingleIndexedComponent >( "newCopy", pOriginal->GetFileId() );
			if ( !pNewCopy )
				return false;

			pOriginal->CopyAttributesTo( pIntersection );
			pIntersection->Intersection( pNew );
			pOriginal->Subtract( pIntersection );

			pNew->CopyAttributesTo( pNewCopy );
			pNewCopy->Subtract( pIntersection );
			pOriginal->Add( pNewCopy );
		}
		break;
	case CSelectOp::kIntersect:
		pOriginal->Intersection( pNew );
		break;
	case CSelectOp::kReplace:
		{
			CUtlString originalName = pOriginal->GetName();
			pNew->CopyAttributesTo( pOriginal );
			pOriginal->SetName( originalName );
		}
		break;
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::SelectHalf( const CSelectOp &selectOp, const CHalfType &halfType, CDmeSingleIndexedComponent *pPassedSelection /* = NULL */, CDmeMesh *pPassedMesh /* = NULL */ )
{
	CDmeSingleIndexedComponent *pSelection = pPassedSelection ? pPassedSelection : m_pCurrentSelection;

	if ( !pSelection )
		return SetErrorString( "No Selection To Manipulate" );

	CDmeSingleIndexedComponent *pTempSelection = CreateElement< CDmeSingleIndexedComponent >( "tempSelection", pSelection->GetFileId() );
	if ( !pTempSelection )
		return false;

	const bool retVal = SelectHalf( halfType, pTempSelection, pPassedMesh );

	if ( retVal )
	{
		Select( selectOp, pSelection, pTempSelection );
	}

	g_pDataModel->DestroyElement( pTempSelection->GetHandle() );

	return retVal;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::SelectHalf( const CHalfType &halfType, CDmeSingleIndexedComponent *pPassedSelection /* = NULL */, CDmeMesh *pPassedMesh /* = NULL */ )
{
	CDmeMesh *pMesh = pPassedMesh ? pPassedMesh : m_pMesh;

	if ( !pMesh )
		return SetErrorString( "No Mesh" );

	CDmeSingleIndexedComponent *pSelection = pPassedSelection ? pPassedSelection : m_pCurrentSelection;

	if ( !pSelection )
		return SetErrorString( "No Selection To Manipulate" );

	pMesh->SelectHalfVertices( halfType() == CHalfType::kLeft ? CDmeMesh::kLeft : CDmeMesh::kRight, pSelection );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	SelectHalf,
	"< $halfType >",
	"Selects all the vertices in half the mesh.  Half is one of left or right.  Left means the X value of the position is >= 0, right means that the X value of the position is <= 0.  So it's the character's left (assuming they're staring down -z)")
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	const char *pHalfType = luaL_checkstring( pLuaState, 1 );

	if ( pHalfType && LuaFunc_s::m_dmxEdit.SelectHalf( pHalfType ) )
		return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::GrowSelection( int nSize /* = 1 */ )
{
	if ( !m_pMesh )
		return SetErrorString( "No Mesh" );

	m_pMesh->GrowSelection( nSize, m_pCurrentSelection, NULL );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	GrowSelection,
	"< #size >",
	"Grows the current selection by the specified $size")
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	if ( lua_isnumber( pLuaState, 1 ) )
	{
		const int nSize = lua_tointeger( pLuaState, 1 );
		if ( LuaFunc_s::m_dmxEdit.GrowSelection( nSize ) )
			return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );
	}
	else
	{
		if ( LuaFunc_s::m_dmxEdit.GrowSelection() )
			return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );
	}

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::ShrinkSelection( int nSize /* = 1 */ )
{
	if ( !m_pMesh )
		return SetErrorString( "No Mesh" );

	m_pMesh->ShrinkSelection( nSize, m_pCurrentSelection, NULL );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	ShrinkSelection,
	"< #size >",
	"Shrinks the current selection by the specified $size (integer)")
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	if ( lua_isnumber( pLuaState, 1 ) )
	{
		const int nSize = lua_tointeger( pLuaState, 1 );
		if ( LuaFunc_s::m_dmxEdit.ShrinkSelection( nSize ) )
			return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );
	}
	else
	{
		if ( LuaFunc_s::m_dmxEdit.ShrinkSelection() )
			return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );
	}

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::Add(
	AddType addType,
	const CDmxEditProxy &e,
	float weight /* = 1.0f */,
	float featherDistance /* = 0.0f */,
	const CFalloffType &falloffType,
	const CDistanceType &passedDistanceType )
{
	if ( !m_pMesh )
		return SetErrorString( "No Mesh" );

	if ( m_pCurrentSelection->Count() == 0 )
	{
		LuaWarning( "No Vertices Selected, Selected ALL" );
		Select( ALL );
	}

	const CDistanceType &distanceType = passedDistanceType() == CDmeMesh::DIST_DEFAULT ? m_distanceType : passedDistanceType;

	CDmeSingleIndexedComponent *pNewSelection = featherDistance > 0.0f ? m_pMesh->FeatherSelection( featherDistance, falloffType(), distanceType(), m_pCurrentSelection, NULL ) : NULL;

	const bool retVal =
		addType == kRaw ?
		m_pMesh->AddMaskedDelta( NULL, NULL, weight, pNewSelection ? pNewSelection : m_pCurrentSelection ) :
		m_pMesh->AddCorrectedMaskedDelta( NULL, NULL, weight, pNewSelection ? pNewSelection : m_pCurrentSelection ) ;

	if ( pNewSelection )
	{
		g_pDataModel->DestroyElement( pNewSelection->GetHandle() );
	}

	return retVal;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::Add(
	AddType addType,
	const char *pDeltaName,
	float weight /* = 1.0f */,
	float featherDistance /* = 0.0f */,
	const CFalloffType &falloffType,
	const CDistanceType &passedDistanceType )
{
	if ( !Q_stricmp( "BASE", pDeltaName ) )
		return Add( addType, CDmxEditProxy( *this ), weight, featherDistance, falloffType );

	if ( !m_pMesh )
		return SetErrorString( "No Mesh" );

	if ( m_pCurrentSelection->Count() == 0 )
	{
		LuaWarning( "No Vertices Selected, Selecting ALL" );
		Select( ALL );
	}

	const CDistanceType &distanceType = passedDistanceType() == CDmeMesh::DIST_DEFAULT ? m_distanceType : passedDistanceType;

	CDmeSingleIndexedComponent *pNewSelection = featherDistance > 0.0f ? m_pMesh->FeatherSelection( featherDistance, falloffType(), distanceType(), m_pCurrentSelection, NULL ) : NULL;

	CDmeVertexDeltaData *pDelta = FindDeltaState( pDeltaName );
	if ( !pDelta )
		return SetErrorString( "Invalid Delta \"%s\"", pDeltaName );

	const bool retVal =
		addType == kRaw ?
		m_pMesh->AddMaskedDelta( pDelta, NULL, weight, pNewSelection ) :
		m_pMesh->AddCorrectedMaskedDelta( pDelta, NULL, weight, pNewSelection );

	if ( pNewSelection )
	{
		g_pDataModel->DestroyElement( pNewSelection->GetHandle() );
	}

	return retVal;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	Add,
	"< $delta >, [ #weight ], [ #featherDistance ], < $falloffType >",
	"Adds specified state to the current state of the mesh weighted and feathered by the specified #weight, #featherDistance & $falloffType.  "
	"$falloffType is one of \"straight\", \"spike\", \"dome\", \"bell\".  Note that only the specified delta is added.  "
	"i.e. If Add( \"A_B\" ); is called then just A_B is added, A & B are not.  See AddCorrected();" )
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	const char *pString1 = luaL_checkstring( pLuaState, 1 );
	float weight = lua_isnumber( pLuaState, 2 ) ? lua_tonumber( pLuaState, 2 ) : 1.0f;
	float featherDistance = lua_isnumber( pLuaState, 3 ) ? lua_tonumber( pLuaState, 3 ) : 0.0f;
	const char *pFalloffType = lua_isstring( pLuaState, 4 ) ? lua_tostring( pLuaState, 4 ) : "straight";

	if ( LuaFunc_s::m_dmxEdit.Add( CDmxEdit::kRaw, pString1, weight, featherDistance, pFalloffType ) )
		return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	AddCorrected,
	"< $delta >, [ #weight ], [ #featherDistance ], < $falloffType >",
	"Same as Add() except that the corrected delta is added. i.e. If AddCorrected( \"A_B\" ); is called "
	"then A, B & A_B are all added.  This works similarly to SetState() whereas Add() just adds "
	"the named delta." )
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	const char *pString1 = luaL_checkstring( pLuaState, 1 );
	float weight = lua_isnumber( pLuaState, 2 ) ? lua_tonumber( pLuaState, 2 ) : 1.0f;
	float featherDistance = lua_isnumber( pLuaState, 3 ) ? lua_tonumber( pLuaState, 3 ) : 0.0f;
	const char *pFalloffType = lua_isstring( pLuaState, 4 ) ? lua_tostring( pLuaState, 4 ) : "straight";

	if ( LuaFunc_s::m_dmxEdit.Add( CDmxEdit::kCorrected, pString1, weight, featherDistance, pFalloffType ) )
		return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::Interp( const CDmxEditProxy &e, float weight /* = 1.0f */, float featherDistance /* = 0.0f */, const CFalloffType &falloffType, const CDistanceType &passedDistanceType )
{
	if ( !m_pMesh )
		return SetErrorString( "No Mesh" );

	if ( m_pCurrentSelection->Count() == 0 )
	{
		LuaWarning( "No Vertices Selected, Selecting ALL" );
		Select( ALL );
	}

	const CDistanceType &distanceType = passedDistanceType() == CDmeMesh::DIST_DEFAULT ? m_distanceType : passedDistanceType;

	CDmeSingleIndexedComponent *pNewSelection = featherDistance > 0.0f ? m_pMesh->FeatherSelection( featherDistance, falloffType(), distanceType(), m_pCurrentSelection, NULL ) : NULL;

	const bool retVal = m_pMesh->InterpMaskedDelta( NULL, NULL, weight, pNewSelection ? pNewSelection : m_pCurrentSelection );

	if ( pNewSelection )
	{
		g_pDataModel->DestroyElement( pNewSelection->GetHandle() );
	}

	return retVal;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::Interp( const char *pDeltaName, float weight /* = 1.0f */, float featherDistance /* = 0.0f */, const CFalloffType &falloffType, const CDistanceType &passedDistanceType )
{
	if ( !Q_stricmp( "BASE", pDeltaName ) )
		return Interp( CDmxEditProxy( *this ), weight, featherDistance, falloffType );

	if ( !m_pMesh )
		return SetErrorString( "No Mesh" );

	if ( m_pCurrentSelection->Count() == 0 )
	{
		LuaWarning( "No Vertices Selected, Selecting ALL" );
		Select( ALL );
	}

	const CDistanceType &distanceType = passedDistanceType() == CDmeMesh::DIST_DEFAULT ? m_distanceType : passedDistanceType;

	CDmeSingleIndexedComponent *pNewSelection = featherDistance > 0.0f ? m_pMesh->FeatherSelection( featherDistance, falloffType(), distanceType(), m_pCurrentSelection, NULL ) : NULL;

	CDmeVertexDeltaData *pDelta = FindDeltaState( pDeltaName );
	if ( !pDelta )
		return SetErrorString( "Invalid Delta \"%s\"", pDeltaName );

	const bool retVal = m_pMesh->InterpMaskedDelta( pDelta, NULL, weight, pNewSelection ? pNewSelection : m_pCurrentSelection );

	if ( pNewSelection )
	{
		g_pDataModel->DestroyElement( pNewSelection->GetHandle() );
	}

	return retVal;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	Interp,
	"< $delta >, [ #weight ], [ #featherDistance ], < $falloffType >",
	"Interpolates the current state of the mesh towards the specified state, $weight, #featherDistance and $falloffType.  $falloffType is one of \"straight\", \"spike\", \"dome\", \"bell\"")
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	const char *pString1 = luaL_checkstring( pLuaState, 1 );
	float weight = lua_isnumber( pLuaState, 2 ) ? lua_tonumber( pLuaState, 2 ) : 1.0f;
	float featherDistance = lua_isnumber( pLuaState, 3 ) ? lua_tonumber( pLuaState, 3 ) : 0.0f;
	const char *pFalloffType = lua_isstring( pLuaState, 4 ) ? lua_tostring( pLuaState, 4 ) : "straight";

	if ( LuaFunc_s::m_dmxEdit.Interp( pString1, weight, featherDistance, pFalloffType ) )
		return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::Save( const char *pFilename, const CObjType &saveType /* = ABSOLUTE */, const char *pDeltaName /* = NULL */ )
{
	if ( !pFilename )
		return SetErrorString( "No Filename Specified" );

	if ( !m_pRoot )
		return SetErrorString( "No Dmx Root Object To Save" );

	RemoveExportTags( m_pRoot, "vsDmxIO_exportTags" );
	AddExportTags( m_pRoot, pFilename );
	UpdateMakefile( m_pRoot );

	CleanupWork();

	bool retVal = false;

	const int sLen = Q_strlen( pFilename );
	if ( sLen > 4 && !Q_stricmp( pFilename + sLen - 4, ".dmx" ) )
	{
		retVal = g_p4factory->AccessFile( pFilename )->Edit();
		if ( !retVal )
		{
			retVal = g_p4factory->AccessFile( pFilename )->Add();
		}

		retVal = g_pDataModel->SaveToFile( pFilename, NULL, "keyvalues2", "model", m_pRoot );
		if ( !retVal )
		{
			SetErrorString( "Cannot Write File" );
		}
	}
	else
	{
		bool absolute = true;
		if ( saveType() == CObjType::kRelative )
		{
			// Relative
			absolute = false;
		}

		if ( pDeltaName )
		{
			if ( !Q_stricmp( "base", pDeltaName ) )
			{
				retVal = CDmObjSerializer().WriteOBJ( pFilename, m_pRoot, false, NULL, absolute );
			}
			else
			{
				retVal = CDmObjSerializer().WriteOBJ( pFilename, m_pRoot, true, pDeltaName, absolute );
			}
		}
		else
		{
			retVal = CDmObjSerializer().WriteOBJ( pFilename, m_pRoot, true, NULL, absolute );
		}
	}

	CreateWork();

	return retVal;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmxEdit::UpdateMakefile( CDmElement *pRoot )
{
	if ( !pRoot )
		return;

	CDmeDCCMakefile *pMakefile = CreateElement< CDmeDCCMakefile >( "dmxedit", pRoot->GetFileId() );
	if ( !pMakefile )
		return;

	pRoot->SetValue( "makefile", pMakefile );

	CDmrElementArray< CDmeSourceDCCFile > sources( pMakefile->GetAttribute( "sources" ) );
	sources.AddToTail( CreateElement< CDmeSourceDCCFile >( m_filename, pRoot->GetFileId() ) );
}


//-----------------------------------------------------------------------------
// In winstuff.cpp
//-----------------------------------------------------------------------------
void MyGetUserName( char *pszBuf, unsigned long *pBufSiz );
void MyGetComputerName( char *pszBuf, unsigned long *pBufSiz );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmxEdit::AddExportTags( CDmElement *pRoot, const char *pFilename )
{
	if ( !pRoot )
		return;

	CDmElement *pExportTags = CreateElement< CDmElement >( "dmxedit_exportTags", pRoot->GetFileId() );

	char szTmpBuf[ BUFSIZ ];

	_strdate( szTmpBuf );
	pExportTags->SetValue( "date", szTmpBuf );

	_strtime( szTmpBuf );
	pExportTags->SetValue( "time", szTmpBuf );

	unsigned long dwSize( sizeof( szTmpBuf ) );

	*szTmpBuf ='\0';
	MyGetUserName( szTmpBuf, &dwSize);
	pExportTags->SetValue( "user", szTmpBuf );

	*szTmpBuf ='\0';
	dwSize = sizeof( szTmpBuf );
	MyGetComputerName( szTmpBuf, &dwSize);
	pExportTags->SetValue( "machine", szTmpBuf );

	pExportTags->SetValue( "app", "dmxedit" );

	static const char *pChangeList = "$Change: 633871 $";
	pExportTags->SetValue( "appVersion", pChangeList );

CUtlString cmdLine( "dmxedit " );
	cmdLine += m_scriptFilename;
	cmdLine += "";
	pExportTags->SetValue( "cmdLine", cmdLine );

	pExportTags->SetValue( "Load", m_filename );
	pExportTags->SetValue( "Save", pFilename );

	pRoot->SetValue( "dmxedit_exportTags", pExportTags );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmxEdit::RemoveExportTags( CDmElement *pRoot, const char *pExportTagsName )
{
	if ( !pRoot )
		return;

	pRoot->RemoveAttribute( pExportTagsName );
}


//-----------------------------------------------------------------------------
// Lua Wrapper For CDmeEdit::Save
//-----------------------------------------------------------------------------
LUA_COMMAND(
	Save,
	"[ $file.dmx | $file.obj ], [ $saveType ], [ $delta ]",
	"Saves the current scene to either a single dmx file or a sequence of obj files.  If $file is not specified, the filename used in the last Load() command will be used to save the file.  $saveType is one of \"absolute\" or \"relative\".  If not specified, \"absolute\" is assumed.  If $delta is passed and the save type is OBJ, then only a single OBJ of that delta is saved.  \"base\" is the base state." )
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	const char *pSave1 = lua_tostring( pLuaState, 1 );

	const char *pSaveType = NULL;

	if ( lua_isboolean( pLuaState, 2 ) )
	{
		if ( lua_toboolean( pLuaState, 2 ) )
		{
			pSaveType = "Absolute";
		}
		else
		{
			pSaveType = "Relative";
		}
	}
	else if ( lua_isstring( pLuaState, 2 ) )
	{
		pSaveType = lua_tostring( pLuaState, 2 );
	}

	if ( pSave1 )
	{
		if ( pSaveType )
		{
			const char *pDeltaName = lua_isstring( pLuaState, 3 ) ? lua_tostring( pLuaState, 3 ) : NULL;

			if ( LuaFunc_s::m_dmxEdit.Save( pSave1, pSaveType, pDeltaName ) )
				return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );
		}
		else
		{
			if ( LuaFunc_s::m_dmxEdit.Save( pSave1 ) )
				return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );
		}
	}
	else
	{
		if ( LuaFunc_s::m_dmxEdit.Save() )
			return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );
	}

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::Merge( const char *pInFilename, const char *pOutFilename )
{
	CDmElement *pRoot = NULL;

	CDisableUndoScopeGuard guard0;
	g_pDataModel->RestoreFromFile( pInFilename, NULL, NULL, &pRoot );
	guard0.Release();

	if ( !pRoot )
		return SetErrorString( "Can't Load File \"%s\"", pInFilename );

	g_p4factory->AccessFile( pInFilename )->Add();

	bool retVal = CDmMeshUtils::Merge( m_pMesh, pRoot );

	CDisableUndoScopeGuard guard1;
	if ( retVal )
	{
		bool bPerforce = g_p4factory->AccessFile( pOutFilename )->Edit();
		if ( !bPerforce )
		{
			bPerforce = g_p4factory->AccessFile( pOutFilename )->Add();
		}

		if ( !g_pDataModel->SaveToFile( pOutFilename, NULL, "keyvalues2", "model", pRoot ) )
		{
			retVal = false;
			SetErrorString( "Can't Write File \"%s\"", pOutFilename );
		}
	}
	else
	{
		SetErrorString( "Failed! \"%s\" Unchanged", pOutFilename );
	}

	g_pDataModel->UnloadFile( pRoot->GetFileId() );
	guard1.Release();

	return retVal;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	Merge,
	"< $src.dmx >, < $dst.dmx >",
	"Merges the current mesh onto the specified dmx file and saves to the second specified dmx file")
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	const char *pString1 = luaL_checkstring( pLuaState, 1 );
	const char *pString2 = luaL_checkstring( pLuaState, 2 );
	if ( LuaFunc_s::m_dmxEdit.Merge( pString1, pString2 ) )
		return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::SaveDelta( const char *pDeltaName )
{
	if ( !m_pMesh )
		return SetErrorString( "No Mesh" );

	if ( !Q_stricmp( pDeltaName, "base" ) )
	{
		CDmeVertexData *pCurr = m_pMesh->GetCurrentBaseState();
		if ( !pCurr )
			return SetErrorString( "Couldn't Get Current Base State" );

		CDmeVertexData *pBind = m_pMesh->GetBindBaseState();
		if ( !pBind )
			return SetErrorString( "Couldn't Get Bind Base State" );

		if ( pCurr == pBind )
			return SetErrorString( "Current Is Same As Bind State" );

		pCurr->CopyTo( pBind );

		return true;
	}

	CDmeVertexDeltaData *pDelta = m_pMesh->ModifyOrCreateDeltaStateFromBaseState( pDeltaName );
	if ( pDelta )
		return true;

	return SetErrorString( "Couldn't Create New Delta State From Base State" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	SaveDelta,
	"< $delta >",
	"Saves the current state of the mesh to the named delta state")
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	const char *pString1 = luaL_checkstring( pLuaState, 1 );
	if ( LuaFunc_s::m_dmxEdit.SaveDelta( pString1 ) )
		return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::DeleteDelta( const delta &d )
{
	if ( !m_pMesh )
		return SetErrorString( "No Mesh" );

	if ( !d )
		return SetErrorString( "Invalid Delta" );

	// Only reason it can fail is if Delta doesn't exist...
	// So be silent about it
	m_pMesh->DeleteDeltaState( static_cast< const char * >( d ) );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	DeleteDelta,
	"< $delta >",
	"Deletes the named delta state from the current mesh")
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	const char *pString1 = luaL_checkstring( pLuaState, 1 );
	if ( LuaFunc_s::m_dmxEdit.DeleteDelta( pString1 ) )
		return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::RemapMaterial( int nMaterialIndex, const char *pNewMaterialName )
{
	return CDmMeshUtils::RemapMaterial( m_pMesh, nMaterialIndex, pNewMaterialName );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::RemapMaterial( const char *pOldMaterialName, const char *pNewMaterialName )
{
	return CDmMeshUtils::RemapMaterial( m_pMesh, pOldMaterialName, pNewMaterialName );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	RemapMaterial,
	"< #index | $srcMaterial >, < $dstMaterial >",
	"Remaps the specified material to the new material, material can be specified by index or name")
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	const char *pString2 = luaL_checkstring( pLuaState, 2 );

	// TODO: Bad error checking... but failure to remap isn't really an error
	if ( lua_isnumber( pLuaState, 1 ) )
	{
		const int nMaterialIndex = luaL_checkinteger( pLuaState, 1 );
		if ( !LuaFunc_s::m_dmxEdit.RemapMaterial( nMaterialIndex, pString2 ) )
		{
			LuaFunc_s::m_dmxEdit.LuaWarning( "Invalid Material Index To Remap, Couldn't Find Material %d\n", nMaterialIndex );
		}
	}
	else
	{
		const char *pString1 = luaL_checkstring( pLuaState, 1 );
		if ( !LuaFunc_s::m_dmxEdit.RemapMaterial( pString1, pString2 ) )
		{
			LuaFunc_s::m_dmxEdit.LuaWarning( "Invalid Material To Remap, Couldn't Find Material \"%s\"\n", pString1 );
		}
	}

	return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::RemoveFacesWithMaterial( const char *pMaterialName )
{
	return CDmMeshUtils::RemoveFacesWithMaterial( m_pMesh, pMaterialName );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	RemoveFacesWithMaterial,
	"< $material >",
	"Removes faces from the current mesh which have the specified material")
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	const char *pString1 = luaL_checkstring( pLuaState, 1 );
	if ( LuaFunc_s::m_dmxEdit.RemoveFacesWithMaterial( pString1 ) )
		return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::RemoveFacesWithMoreThanNVerts( int nVertexCount )
{
	CDmMeshUtils::RemoveFacesWithMoreThanNVerts( m_pMesh, nVertexCount );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	RemoveFacesWithMoreThanNVerts,
	"< #vertexCount >",
	"Removes faces from the current mesh which have more than the specified number of faces")
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	const int nVertsCount = luaL_checkinteger( pLuaState, 1 );
	if ( LuaFunc_s::m_dmxEdit.RemoveFacesWithMoreThanNVerts( nVertsCount ) )
		return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::Mirror( CAxisType axisType /* = XAXIS */ )
{
	return CDmMeshUtils::Mirror( m_pMesh, axisType() );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	Mirror,
	"[ $axis ]",
	"Mirrors the mesh in the specified axis.  $axis is one of \"x\", \"y\", \"z\". If $axis is not specified, \"x\" is assumed (i.e. \"x\" == mirror across YZ plane)")
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	if ( lua_isstring( pLuaState, 1 ) )
	{
		const char *pString1 = luaL_checkstring( pLuaState, 1 );
		if ( LuaFunc_s::m_dmxEdit.Mirror( pString1 ) )
			return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );
	}
	else
	{
		if ( LuaFunc_s::m_dmxEdit.Mirror() )
			return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );
	}

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::ComputeNormals()
{
	m_pMesh->ComputeDeltaStateNormals();

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	ComputeNormals,
	"",
	"Computes new smooth normals for the current mesh and all of its delta states.")
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	if ( LuaFunc_s::m_dmxEdit.ComputeNormals() )
		return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::ComputeWrinkles( bool bOverwrite )
{
	if ( !m_pMesh )
		return SetErrorString( "No Mesh" );

	CDmeCombinationOperator *pCombo( FindReferringElement< CDmeCombinationOperator >( m_pMesh, "targets" ) );
	if ( !pCombo )
		return SetErrorString( "No Combination Operator On Mesh \"%s\"", m_pMesh->GetName() );

	pCombo->GenerateWrinkleDeltas( bOverwrite );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	ComputeWrinkles,
	"[ #bOverwrite = false ]",
	"Uses the current combo rules ( i.e. The wrinkleScales, via SetWrinkleScale()) to compute wrinkle delta values.  "
	"If #bOverwrite is false, only wrinkle data that doesn't currently exist on the mesh will be computed.  "
	"If #bOverwrite is true then all wrinkle data will be computed from the combo rules wiping out any existing data computed using ComputeWrinkle().  "
	)
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	const bool bOverwrite = lua_isboolean( pLuaState, 1 ) ? ( lua_toboolean( pLuaState, 1 ) ? true : false ) : false;

	if ( LuaFunc_s::m_dmxEdit.ComputeWrinkles( bOverwrite ) )
		return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::ComputeWrinkle( const char *pDeltaName, float flScale, const char *pOperation )
{
	if ( !m_pMesh )
		return SetErrorString( "No Mesh" );

	CDmeVertexDeltaData *pDelta = FindDeltaState( pDeltaName );
	if ( !pDelta )
		return SetErrorString( "Cannot Find Delta State \"%s\"", pDeltaName );

	CDmeCombinationOperator *pDmeCombo = FindReferringElement< CDmeCombinationOperator >( m_pMesh, "targets" );
	if ( !pDmeCombo )
		return SetErrorString( "No DmeCombinationOperator On Mesh \"%s\"", m_pMesh->GetName() );

	CDmMeshUtils::WrinkleOp wrinkleOp = StringHasPrefix( pOperation, "r" ) ? CDmMeshUtils::kReplace : CDmMeshUtils::kAdd;

	const int nControlCount = pDmeCombo->GetControlCount();
	for ( int nControlIndex = 0; nControlIndex < nControlCount; ++nControlIndex )
	{
		const int nRawControlCount = pDmeCombo->GetRawControlCount( nControlIndex );
		for ( int nRawControlIndex = 0; nRawControlIndex < nRawControlCount; ++nRawControlIndex )
		{
			if ( Q_strcmp( pDeltaName, pDmeCombo->GetRawControlName( nControlIndex, nRawControlIndex ) ) )
				continue;

			pDmeCombo->SetWrinkleScale( nControlIndex, pDeltaName, 1.0f );

			break;
		}
	}

	return CDmMeshUtils::CreateWrinkleDeltaFromBaseState( pDelta, flScale, wrinkleOp, m_pMesh );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	ComputeWrinkle,
	"< $delta > [ , #scale = 1 [ , $operation ] ] ",
	"Updates the wrinkle stretch/compress data for the specified delta based on the position deltas "
	"for the current state scaled by the scale value.  If $operation isn't specified then \"REPLACE\" "
	"is assumed. $operation is one of: \"REPLACE\", \"ADD\".  "
	"Note that Negative (-) values enable the compress map and Positive (+) values enable the stretch map. "
	"This also sets the wrinkle scale to 1"
	)
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	const char *pDeltaName = luaL_checkstring( pLuaState, 1 );
	const float flScale = lua_isnumber( pLuaState, 2 ) ? lua_tonumber( pLuaState, 2 ) : 1.0f;
	const char *pOperation = lua_isstring( pLuaState, 3 ) ? lua_tostring( pLuaState, 3 ) : "REPLACE";

	if ( LuaFunc_s::m_dmxEdit.ComputeWrinkle( pDeltaName, flScale, pOperation ) )
		return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::CreateDeltasFromPresets(
	const char *pPresetFilename,
	bool bPurge /* = true */,
	const CUtlVector< CUtlString > *pPurgeAllButThese /* = NULL */,
	const char *pExpressionFilename /* = NULL */  )
{
	ClearPresetCache();
	CachePreset( pPresetFilename, pExpressionFilename );

	const bool bRetVal = CDmMeshUtils::CreateDeltasFromPresets( m_pMesh, NULL, m_presetCache, bPurge, pPurgeAllButThese );

	ClearPresetCache();

	return bRetVal;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	CreateDeltasFromPresets,
	"< $preset.pre >, [ #bPurgeExisting = true ], [ { \"NoPurge1\", ... } ], [ $expression.txt ]",
	"Creates a new delta state for every present defined in the specified preset file.  #bPurgeExisting specifies whether existing controls should be purged.  If #bPurgeExisting is true, then an optional array of deltas to not purge can be specified.")
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	const char *pPresetFilename = luaL_checkstring( pLuaState, 1 );
	const bool bPurge = lua_isboolean( pLuaState, 2 ) ? lua_toboolean( pLuaState, 2 ) ? true : false : true;
	const char *pExpressionFilename = lua_isstring( pLuaState, 4 ) ? lua_tostring( pLuaState, 4 ) : NULL;

	CUtlVector< CUtlString > purgeAllBut;
	const CUtlVector< CUtlString > *pPurgeAllBut = NULL;

	if ( lua_istable( pLuaState, 3 ) )
	{
		pPurgeAllBut = &purgeAllBut;

		// table is in the stack at index '3'
		lua_pushnil( pLuaState );  //
		while (lua_next( pLuaState, 3 ) != 0) {
			// uses 'key' (at index -2) and 'value' (at index -1)
			purgeAllBut.AddToTail( lua_tostring( pLuaState, -1 ) );

			// removes 'value'; keeps 'key' for next iteration */
			lua_pop( pLuaState, 1 );
		}
	}

	if ( LuaFunc_s::m_dmxEdit.CreateDeltasFromPresets( pPresetFilename, bPurge, pPurgeAllBut, pExpressionFilename ) )
		return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::CachePreset( const char *pPresetFilename, const char *pExpressionFilename /* = NULL  */ )
{
	m_presetCache[ pPresetFilename ] = pExpressionFilename;
	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	CachePreset,
	"< $preset.pre >, [ $expression.txt ]",
	"Caches the specified $preset file for later processing by a call to CreateDeltasFromCachedPresets().  If an $expression.txt file pathname is specified, an expression file will be written out for this preset file" )
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	const char *pPresetFilename = luaL_checkstring( pLuaState, 1 );
	const char *pExpressionFilename = lua_isstring( pLuaState, 2 ) ? lua_tostring( pLuaState, 2 ) : NULL;

	if ( LuaFunc_s::m_dmxEdit.CachePreset( pPresetFilename, pExpressionFilename ) )
		return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::ClearPresetCache()
{
	m_presetCache.Clear();
	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	ClearPresetCache,
	"",
	"Removes all preset filenames that have been specified via CachePreset() calls")
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	if ( LuaFunc_s::m_dmxEdit.ClearPresetCache() )
		return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::CreateDeltasFromCachedPresets(
	bool bPurge /* = true */,
	const CUtlVector< CUtlString > *pPurgeAllButThese /* = NULL */ ) const
{
	return CDmMeshUtils::CreateDeltasFromPresets( m_pMesh, NULL, m_presetCache, bPurge, pPurgeAllButThese );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	CreateDeltasFromCachedPresets,
	"[ #bPurgeExisting = true ], [ { \"NoPurge1\", ... } ]",
	"Creates a new delta state for every preset defined in the preset files specified by previous CachePreset() calls.  Calling this calls ClearPresetCache().  #bPurgeExisting specifies whether existing controls should be purged.  If #bPurgeExisting is true, then an optional array of deltas to not purge can be specified.")
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	const bool bPurge = lua_isboolean( pLuaState, 1 ) ? lua_toboolean( pLuaState, 1 ) ? true : false : true;

	CUtlVector< CUtlString > purgeAllBut;
	const CUtlVector< CUtlString > *pPurgeAllBut = NULL;

	if ( lua_istable( pLuaState, 2 ) )
	{
		pPurgeAllBut = &purgeAllBut;

		// table is in the stack at index '2'
		lua_pushnil( pLuaState );  //
		while (lua_next( pLuaState, 2 ) != 0) {
			// uses 'key' (at index -2) and 'value' (at index -1)
			purgeAllBut.AddToTail( lua_tostring( pLuaState, -1 ) );

			// removes 'value'; keeps 'key' for next iteration */
			lua_pop( pLuaState, 1 );
		}
	}

	bool bRetVal = LuaFunc_s::m_dmxEdit.CreateDeltasFromCachedPresets( bPurge, pPurgeAllBut );
	LuaFunc_s::m_dmxEdit.ClearPresetCache();

	if ( bRetVal )
		return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::CreateExpressionFileFromPresets( const char *pPresetFilename, const char *pExpressionFilename )
{
	ClearPresetCache();
	CachePreset( pPresetFilename, pExpressionFilename );
	const bool bRetVal = CreateExpressionFilesFromCachedPresets();
	ClearPresetCache();

	return bRetVal;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	CreateExpressionFileFromPresets,
	"< $preset.pre >, < $expression.txt >",
	"Creates a faceposer expression.txt and expression.vfe file for the specified preset file")
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	const char *pPresetFilename = luaL_checkstring( pLuaState, 1 );
	const char *pExpressionFilename = luaL_checkstring( pLuaState, 2 );

	if ( LuaFunc_s::m_dmxEdit.CreateExpressionFileFromPresets( pPresetFilename, pExpressionFilename ) )
		return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::CreateExpressionFilesFromCachedPresets() const
{
	bool bRetVal = true;

	CDisableUndoScopeGuard sgDisableUndo;

	char buf[ MAX_PATH ];
	char buf1[ MAX_PATH ];

	if ( !m_pMesh )
		return false;

	CDmeCombinationOperator *pComboOp( FindReferringElement< CDmeCombinationOperator >( m_pMesh, "targets" ) );
	if ( !pComboOp )
		return false;

	for ( int i = 0; i < m_presetCache.GetNumStrings(); ++i )
	{
		const char *pPresetFilename = m_presetCache.String( i );

		CDmElement *pRoot = NULL;
		g_p4factory->AccessFile( pPresetFilename )->Add();
		g_pDataModel->RestoreFromFile( pPresetFilename, NULL, NULL, &pRoot );

		if ( !pRoot )
		{
			bRetVal = false;
			continue;
		}

		CDmePresetGroup *pPresetGroup = CastElement< CDmePresetGroup >( pRoot );
		if ( pPresetGroup )
		{
			const CUtlString &expressionFilename = m_presetCache[ i ];
			if ( expressionFilename.IsEmpty() )
			{
				Warning( "// No expression file specified for preset %s\n", pPresetFilename );
				continue;
			}

			const char *pExpressionFilename = expressionFilename.Get();

			Q_strncpy( buf, pExpressionFilename, sizeof( buf ) );
			Q_SetExtension( buf, ".txt", sizeof( buf ) );
			Q_ExtractFilePath( buf, buf1, sizeof( buf1 ) );
			Q_FixSlashes( buf1 );
			g_pFullFileSystem->CreateDirHierarchy( buf1 );

			if ( !g_p4factory->AccessFile( buf )->Edit() )
			{
				g_p4factory->AccessFile( buf )->Add();
			}

			pPresetGroup->ExportToTXT( buf, NULL, pComboOp );

			Q_SetExtension( buf, ".vfe", sizeof( buf ) );
			Q_ExtractFilePath( buf, buf1, sizeof( buf1 ) );
			Q_FixSlashes( buf1 );
			g_pFullFileSystem->CreateDirHierarchy( buf1 );

			if ( !g_p4factory->AccessFile( buf )->Edit() )
			{
				g_p4factory->AccessFile( buf )->Add();
			}

			pPresetGroup->ExportToVFE( buf, NULL, pComboOp );
		}

		g_pDataModel->UnloadFile( pRoot->GetFileId() );
	}

	return bRetVal;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	CreateExpressionFilesFromCachedPresets,
	"",
	"Creates a faceposer expression.txt and expression.vfe file for each of the preset/expression file pairs specified by previous CachePreset calls")
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	if ( LuaFunc_s::m_dmxEdit.CreateExpressionFilesFromCachedPresets() )
		return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void ScaleDeltaPositions( const CDmrArrayConst< Vector > &bindPosData, CDmeVertexDeltaData *pDelta, float sx, float sy, float sz )
{
	const int nPosIndex = pDelta->FindFieldIndex( CDmeVertexData::FIELD_POSITION );
	if ( nPosIndex < 0 )
		return;

	CDmrArray< Vector > posData = pDelta->GetVertexData( nPosIndex );
	const int nPosDataCount = posData.Count();
	if ( nPosDataCount <= 0 )
		return;

	Vector *pPosArray = reinterpret_cast< Vector * >( alloca( nPosDataCount * sizeof( Vector ) ) );

	for ( int j = 0; j < nPosDataCount; ++j )
	{
		const Vector &s = posData.Get( j );
		Vector &d = pPosArray[ j ];
		d.x = s.x * sx;
		d.y = s.y * sy;
		d.z = s.z * sz;
	}

	posData.SetMultiple( 0, nPosDataCount, pPosArray );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::Scale( float sx, float sy, float sz )
{
	int nArraySize = 0;
	Vector *pPosArray = NULL;

	const int nBaseStateCount = m_pMesh->BaseStateCount();
	for ( int i = 0; i < nBaseStateCount; ++i )
	{
		CDmeVertexData *pBase = m_pMesh->GetBaseState( i );
		const int nPosIndex = pBase->FindFieldIndex( CDmeVertexData::FIELD_POSITION );
		if ( nPosIndex < 0 )
			continue;

		CDmrArray< Vector > posData = pBase->GetVertexData( nPosIndex );
		const int nPosDataCount = posData.Count();
		if ( nPosDataCount <= 0 )
			continue;

		if ( nArraySize < nPosDataCount || pPosArray == NULL )
		{
			pPosArray = reinterpret_cast< Vector * >( alloca( nPosDataCount * sizeof( Vector ) ) );
			if ( pPosArray )
			{
				nArraySize = nPosDataCount;
			}
		}

		if ( nArraySize < nPosDataCount )
			continue;

		for ( int j = 0; j < nPosDataCount; ++j )
		{
			const Vector &s = posData.Get( j );
			Vector &d = pPosArray[ j ];
			d.x = s.x * sx;
			d.y = s.y * sy;
			d.z = s.z * sz;
		}

		posData.SetMultiple( 0, nPosDataCount, pPosArray );
	}

	{
		CDmeVertexData *pBind = m_pMesh->GetBindBaseState();
		const int nPosIndex = pBind ? pBind->FindFieldIndex( CDmeVertexData::FIELD_POSITION ) : -1;

		if ( !pBind || nPosIndex < 0 )
		{
			Warning( "// Can't Scale Deltas!\n" );
			return false;
		}

		const CDmrArrayConst< Vector > posData = pBind->GetVertexData( nPosIndex );

		const int nDeltaStateCount = m_pMesh->DeltaStateCount();
		for ( int i = 0; i < nDeltaStateCount; ++i )
		{
			ScaleDeltaPositions( posData, m_pMesh->GetDeltaState( i ), sx, sy, sz );
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	Scale,
	"< #xScale >, [ #yScale, #zScale ]",
	"Scales the position values by the specified amount.  If only 1 scale value is supplied a uniform scale in all three dimensions is applied")
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	const float sx = luaL_checknumber( pLuaState, 1 );
	float sy = sx;
	float sz = sx;

	if ( lua_isnumber( pLuaState, 2 ) && lua_isnumber( pLuaState, 3 ) )
	{
		sy = lua_tonumber( pLuaState, 2 );
		sz = lua_tonumber( pLuaState, 3 );
	}

	if ( LuaFunc_s::m_dmxEdit.Scale( sx, sy, sz ) )
		return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::SetDistanceType( const CDistanceType &distanceType )
{
	m_distanceType = distanceType;
	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	SetDistanceType,
	"< $distanceType >",
	"Sets the way distances will be interpreted after the command.  $distanceType is one of \"absolute\" or \"relative\".  By default distances are \"absolute\".  All functions that work with distances (Add, Interp and Translate) work on the currently selected vertices.  \"absolute\" means use the distance as that number of units, \"relative\" means use the distance as a scale of the radius of the bounding sphere of the selected vertices.")
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	const char *pDistanceType = luaL_checkstring( pLuaState, 1 );

	if ( LuaFunc_s::m_dmxEdit.SetDistanceType( pDistanceType ) )
		return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::Translate(
	Vector t,
	float featherDistance,
	const CFalloffType &falloffType,
	const CDistanceType &passedDistanceType,
	CDmeMesh *pPassedMesh,
	CDmeVertexData *pPassedBase,
	CDmeSingleIndexedComponent *pPassedSelection )
{
	const CDistanceType &distanceType = passedDistanceType() == CDmeMesh::DIST_DEFAULT ? m_distanceType : passedDistanceType;

	CDmeMesh *pMesh = pPassedMesh ? pPassedMesh : m_pMesh;
	if ( !m_pMesh )
		return SetErrorString( "No Mesh" );

	CDmeVertexData *pBase = pPassedBase ? pPassedBase : pMesh->GetCurrentBaseState();

	CDmeSingleIndexedComponent *pSelection = pPassedSelection ? pPassedSelection : m_pCurrentSelection;
	CDmeSingleIndexedComponent *pTmpSelection = NULL;

	if ( !pSelection || pSelection->Count() == 0 )
	{
		pTmpSelection = CreateElement< CDmeSingleIndexedComponent >( "__selectAll", DMFILEID_INVALID );
		pSelection = pTmpSelection;
		pMesh->SelectAllVertices( pSelection );
	}

	CDmeSingleIndexedComponent *pNewSelection = ( pSelection && featherDistance > 0.0f ) ? m_pMesh->FeatherSelection( featherDistance, falloffType(), distanceType(), pSelection, NULL ) : pSelection;

	int nArraySize = 0;
	Vector *pPosArray = NULL;

	const int nPosIndex = pBase->FindFieldIndex( CDmeVertexData::FIELD_POSITION );
	if ( nPosIndex < 0 )
		return false;

	CDmrArray< Vector > posData = pBase->GetVertexData( nPosIndex );
	const int nPosDataCount = posData.Count();
	if ( nPosDataCount <= 0 )
		return false;

	if ( nArraySize < nPosDataCount || pPosArray == NULL )
	{
		pPosArray = reinterpret_cast< Vector * >( alloca( nPosDataCount * sizeof( Vector ) ) );
		if ( pPosArray )
		{
			nArraySize = nPosDataCount;
		}
	}

	if ( nArraySize < nPosDataCount )
		return false;


	if ( distanceType() == CDmeMesh::DIST_RELATIVE )
	{
		Vector vCenter;
		float fRadius;

		pMesh->GetBoundingSphere( vCenter, fRadius, pBase, pSelection );

		t *= fRadius;
	}

	if ( pNewSelection )
	{
		memcpy( pPosArray, posData.Base(), nPosDataCount * sizeof( Vector ) );

		const int nSelectionCount = pNewSelection->Count();
		int nIndex;
		float fWeight;
		for ( int j = 0; j < nSelectionCount; ++j )
		{
			pNewSelection->GetComponent( j, nIndex, fWeight );
			const Vector &s = posData.Get( nIndex );
			Vector &d = pPosArray[ nIndex ];
			d = s + t * fWeight;
		}
	}
	else
	{
		for ( int j = 0; j < nPosDataCount; ++j )
		{
			const Vector &s = posData.Get( j );
			Vector &d = pPosArray[ j ];
			d = s + t;
		}
	}

	posData.SetMultiple( 0, nPosDataCount, pPosArray );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	Translate,
	"< #xTranslate, #yTranslate, #zTranslate >, [ #falloffDistance, $falloffType ]",
	"Translates the selected vertices of the mesh by the specified amount")
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	const float tx = luaL_checknumber( pLuaState, 1 );
	const float ty = luaL_checknumber( pLuaState, 2 );
	const float tz = luaL_checknumber( pLuaState, 3 );

	if ( lua_gettop( pLuaState ) < 4 )
	{
		if ( LuaFunc_s::m_dmxEdit.Translate( Vector( tx, ty, tz ) ) )
			return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );
	}
	else
	{
		float flFeatherDistance = 0.0f;
		const char *pFalloffType = "STRAIGHT";
		const char *pDistanceType = "DEFAULT";

		if ( lua_gettop( pLuaState ) == 4 && lua_isstring( pLuaState, 4 ) )
		{
			pDistanceType = lua_tostring( pLuaState, 4 );
		}
		else
		{
			flFeatherDistance = lua_isnumber( pLuaState, 4 ) ? lua_tonumber( pLuaState, 4 ) : flFeatherDistance;
			pFalloffType = lua_isstring( pLuaState, 5 ) ? lua_tostring( pLuaState, 5 ) : pFalloffType;
			pDistanceType = lua_isstring( pLuaState, 6 ) ? lua_tostring( pLuaState, 6 ) : pDistanceType;
		}

		if ( LuaFunc_s::m_dmxEdit.Translate( Vector( tx, ty, tz ), flFeatherDistance, pFalloffType, pDistanceType ) )
			return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );
	}

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::Rotate(
	Vector r,	// X, Y, Z Euler angles in degrees specified by user
	Vector o,	// Offset
	float featherDistance /* = 0.0f */,
	const CFalloffType &falloffType /* = STRAIGHT */,
	const CDistanceType &passedDistanceType /* = DIST_DEFAULT */,
	CDmeMesh *pPassedMesh /* = NULL */,
	CDmeVertexData *pPassedBase /* = NULL */,
	CDmeSingleIndexedComponent *pPassedSelection /* = NULL */ )
{
	const CDistanceType &distanceType = passedDistanceType() == CDmeMesh::DIST_DEFAULT ? m_distanceType : passedDistanceType;

	CDmeMesh *pMesh = pPassedMesh ? pPassedMesh : m_pMesh;
	if ( !m_pMesh )
		return SetErrorString( "No Mesh" );

	CDmeVertexData *pBase = pPassedBase ? pPassedBase : pMesh->GetCurrentBaseState();

	CDmeSingleIndexedComponent *pSelection = pPassedSelection ? pPassedSelection : m_pCurrentSelection;
	CDmeSingleIndexedComponent *pTmpSelection = NULL;

	if ( !pSelection || pSelection->Count() == 0 )
	{
		pTmpSelection = CreateElement< CDmeSingleIndexedComponent >( "__selectAll", DMFILEID_INVALID );
		pSelection = pTmpSelection;
		pMesh->SelectAllVertices( pSelection );
	}

	CDmeSingleIndexedComponent *pNewSelection = ( pSelection && featherDistance > 0.0f ) ? m_pMesh->FeatherSelection( featherDistance, falloffType(), distanceType(), pSelection, NULL ) : pSelection;

	int nArraySize = 0;
	Vector *pPosArray = NULL;

	const int nPosIndex = pBase->FindFieldIndex( CDmeVertexData::FIELD_POSITION );
	if ( nPosIndex < 0 )
		return false;

	CDmrArray< Vector > posData = pBase->GetVertexData( nPosIndex );
	const int nPosDataCount = posData.Count();
	if ( nPosDataCount <= 0 )
		return false;

	if ( nArraySize < nPosDataCount || pPosArray == NULL )
	{
		pPosArray = reinterpret_cast< Vector * >( alloca( nPosDataCount * sizeof( Vector ) ) );
		if ( pPosArray )
		{
			nArraySize = nPosDataCount;
		}
	}

	if ( nArraySize < nPosDataCount )
		return false;

	Vector vCenter;
	float fRadius;

	pMesh->GetBoundingSphere( vCenter, fRadius, pBase, pSelection );

	if ( distanceType() == CDmeMesh::DIST_RELATIVE )
	{
		r *= fRadius;
	}

	VectorAdd( vCenter, o, vCenter );

	matrix3x4_t rpMat;
	SetIdentityMatrix( rpMat );
	PositionMatrix( vCenter, rpMat );

	matrix3x4_t rpiMat;
	SetIdentityMatrix( rpiMat );
	PositionMatrix( -vCenter, rpiMat );

	matrix3x4_t rMat;
	SetIdentityMatrix( rMat );

	if ( pNewSelection )
	{
		memcpy( pPosArray, posData.Base(), nPosDataCount * sizeof( Vector ) );

		const int nSelectionCount = pNewSelection->Count();
		int nIndex;
		float fWeight;
		for ( int j = 0; j < nSelectionCount; ++j )
		{
			pNewSelection->GetComponent( j, nIndex, fWeight );
			const Vector &s = posData.Get( nIndex );
			Vector &d = pPosArray[ nIndex ];
			AngleMatrix( RadianEuler( DEG2RAD( r.x * fWeight ), DEG2RAD( r.y * fWeight ), DEG2RAD( r.z * fWeight ) ), rMat );

			ConcatTransforms( rpMat, rMat, rMat );
			ConcatTransforms( rMat, rpiMat, rMat );

			VectorTransform( s, rMat, d );
		}
	}
	else
	{
		AngleMatrix( RadianEuler( DEG2RAD( r.x ), DEG2RAD( r.y ), DEG2RAD( r.z ) ), rMat );

		ConcatTransforms( rpMat, rMat, rMat );
		ConcatTransforms( rMat, rpiMat, rMat );

		for ( int j = 0; j < nPosDataCount; ++j )
		{
			const Vector &s = posData.Get( j );
			Vector &d = pPosArray[ j ];
			VectorTransform( s, rMat, d );
		}
	}

	posData.SetMultiple( 0, nPosDataCount, pPosArray );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	Rotate,
	"< #xRotate, #yRotate, #zRotate >, [ #xOffset, #yOffset, #zOffset ], [ #falloffDistance, $falloffType ]",
	"Rotates the current selection around the center of the selection by the specified Euler angles (in degrees).  The optional offset is added to the computed pivot point.")
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	const float rx = luaL_checknumber( pLuaState, 1 );
	const float ry = luaL_checknumber( pLuaState, 2 );
	const float rz = luaL_checknumber( pLuaState, 3 );

	if ( lua_gettop( pLuaState ) < 4 )
	{
		if ( LuaFunc_s::m_dmxEdit.Rotate( Vector( rx, ry, rz ), Vector( 0.0f, 0.0f, 0.0f ) ) )
			return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );
	}
	else
	{
		const float ox = luaL_checknumber( pLuaState, 4 );
		const float oy = luaL_checknumber( pLuaState, 5 );
		const float oz = luaL_checknumber( pLuaState, 6 );

		if ( lua_gettop( pLuaState ) < 7 )
		{
			if ( LuaFunc_s::m_dmxEdit.Rotate( Vector( rx, ry, rz ), Vector( ox, oy, oz ) ) )
				return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );
		}
		else
		{
			float flFeatherDistance = 0.0f;
			const char *pFalloffType = "STRAIGHT";
			const char *pDistanceType = "DEFAULT";

			flFeatherDistance = lua_isnumber( pLuaState, 7 ) ? lua_tonumber( pLuaState, 7 ) : flFeatherDistance;
			pFalloffType = lua_isstring( pLuaState, 8 ) ? lua_tostring( pLuaState, 8 ) : pFalloffType;
			pDistanceType = lua_isstring( pLuaState, 9 ) ? lua_tostring( pLuaState, 9 ) : pDistanceType;

			if ( LuaFunc_s::m_dmxEdit.Rotate( Vector( rx, ry, rz ), Vector( ox, oy, oz ), flFeatherDistance, pFalloffType, pDistanceType ) )
				return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );
		}
	}

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	FileExists,
	"< $filename >",
	"Returns true if the given filename exists on disk, false otherwise")
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	const char *pFilename = luaL_checkstring( pLuaState, 1 );

	if ( g_pFullFileSystem->FileExists( pFilename ) )
		return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState, "File \"%s\" Doesn't Exist", pFilename );
}


//-----------------------------------------------------------------------------
// If it finds a duplicate control name, reports an error message and returns it found one
//-----------------------------------------------------------------------------
bool HasDuplicateControlName(
	CDmeCombinationOperator *pDmeCombo,
	const char *pControlName,
	CUtlVector< const char * > &retiredControlNames )
{
	int i;
	int nRetiredControlNameCount = retiredControlNames.Count();
	for ( i = 0; i < nRetiredControlNameCount; ++i )
	{
		if ( !Q_stricmp( retiredControlNames[i], pControlName ) )
			break;
	}

	if ( i == nRetiredControlNameCount )
	{
		if ( pDmeCombo->FindControlIndex( pControlName ) >= 0 )
			return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::GroupControls( const char *pGroupName, CUtlVector< const char * > &rawControlNames )
{
	if ( !m_pMesh )
		return SetErrorString( "No Mesh" );

	CDmeCombinationOperator *pDmeCombo = FindReferringElement< CDmeCombinationOperator >( m_pMesh, "targets" );
	if ( !pDmeCombo )
		return SetErrorString( "No DmeCombinationOperator On Mesh \"%s\"", m_pMesh->GetName() );

	// Loop through controls to see if any are already group controls, warn and remove

	CUtlVector< const char * > validControlNames;
	bool bStereo = false;
	bool bEyelid = false;

	for ( int i = 0; i < rawControlNames.Count(); ++i )
	{
		ControlIndex_t nControlIndex = pDmeCombo->FindControlIndex( rawControlNames[ i ] );
		if ( nControlIndex < 0 )
		{
			LuaWarning( "Control \"%s\" Doesn't Exist, Ignoring", pGroupName );
			continue;
		}

		if ( pDmeCombo->GetRawControlCount( nControlIndex ) > 1 )
		{
			LuaWarning( "Control \"%s\" Isn't A Raw Control, Ignoring", pGroupName );
			continue;
		}

		validControlNames.AddToTail( rawControlNames[ i ] );

		if ( pDmeCombo->IsStereoControl( nControlIndex ) )
		{
			bStereo = true;
		}

		if ( pDmeCombo->IsEyelidControl( nControlIndex ) )
		{
			bEyelid = true;
		}
	}

	if ( HasDuplicateControlName( pDmeCombo, pGroupName, validControlNames ) )
		return SetErrorString( "Duplicate Control \"%s\" Found", pGroupName );

	if ( validControlNames.Count() <= 0 )
		return SetErrorString( "No Valid Controls Specified" );

	// Remove the old controls
	for ( int i = 0; i < validControlNames.Count(); ++i )
	{
		pDmeCombo->RemoveControl( validControlNames[i] );
	}

	// Create new control
	ControlIndex_t nNewControl = pDmeCombo->FindOrCreateControl( pGroupName, bStereo );
	pDmeCombo->SetEyelidControl( nNewControl, bEyelid );
	for ( int i = 0; i < validControlNames.Count(); ++i )
	{
		pDmeCombo->AddRawControl( nNewControl, validControlNames[i] );
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	GroupControls,
	"< $groupName > [ , $rawControlName ... ]",
	"Groups the specified raw controls under a common group control.  If the group control already exists, the raw controls are added to it" )
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	const char *pGroupName = luaL_checkstring( pLuaState, 1 );

	CUtlVector< const char * > rawControlNames;

	for ( int i = 2; i <= lua_gettop( pLuaState ); ++i )
	{
		if ( !lua_isstring( pLuaState, i ) )
			break;

		rawControlNames.AddToTail( lua_tostring( pLuaState, i ) );
	}

	if ( rawControlNames.Count() <= 0 )
		return LuaFunc_s::m_dmxEdit.LuaError( pLuaState, "No Raw Controls Specified" );

	if ( LuaFunc_s::m_dmxEdit.GroupControls( pGroupName, rawControlNames ) )
		return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::ReorderControls( CUtlVector< CUtlString > &controlNames )
{
	if ( !m_pMesh )
		return SetErrorString( "No Mesh" );

	CDmeCombinationOperator *pDmeCombo = FindReferringElement< CDmeCombinationOperator >( m_pMesh, "targets" );
	if ( !pDmeCombo )
		return SetErrorString( "No DmeCombinationOperator On Mesh \"%s\"", m_pMesh->GetName() );

	// Loop through controls to see if any are already group controls, warn and remove

	CUtlVector< const char * > validControlNames;

	for ( int i = 0; i < controlNames.Count(); ++i )
	{
		ControlIndex_t nControlIndex = pDmeCombo->FindControlIndex( controlNames[ i ] );
		if ( nControlIndex < 0 )
		{
			LuaWarning( "Control \"%s\" doesn't exist, ignoring", controlNames[ i ].Get() );
			continue;
		}

		validControlNames.AddToTail( controlNames[ i ] );
	}

	if ( validControlNames.Count() <= 0 )
		return SetErrorString( "No Valid Controls Specified" );

	for ( int i = 0; i < validControlNames.Count(); ++i )
	{
		pDmeCombo->MoveControlBefore( validControlNames[ i ], pDmeCombo->GetControlName( i ) );
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	ReorderControls,
	"< $controlName [ , ... ] >",
	"Reorders the controls in the specified order.  "
	"The specified controls will be moved to the front of the list of controls.  "
	"Not all of the controls need to be specified.  "
	"Unspecified controls will be left in the order they already are after the specified controls " )
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	CUtlVector< CUtlString > controlNames;

	for ( int i = 1; i <= lua_gettop( pLuaState ); ++i )
	{
		if ( lua_istable( pLuaState, i ) )
		{
			const int nCount = luaL_getn( pLuaState, i );
			for ( int j = 1; j <= nCount; ++j )
			{
				lua_rawgeti( pLuaState, i, j );
				if ( lua_isstring( pLuaState, -1 ) )
				{
					controlNames.AddToTail( lua_tostring( pLuaState, -1 ) );
				}
				lua_pop( pLuaState, 1 );
			}

			continue;
		}

		if ( !lua_isstring( pLuaState, i ) )
			continue;

		controlNames.AddToTail( lua_tostring( pLuaState, i ) );
	}

	if ( controlNames.Count() <= 0 )
		return LuaFunc_s::m_dmxEdit.LuaError( pLuaState, "No Controls Specified" );

	if ( LuaFunc_s::m_dmxEdit.ReorderControls( controlNames ) )
		return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::AddDominationRule( CUtlVector< CUtlString > &dominators, CUtlVector< CUtlString > &supressed )
{
	if ( !m_pMesh )
		return SetErrorString( "No Mesh" );

	CDmeCombinationOperator *pDmeCombo = FindReferringElement< CDmeCombinationOperator >( m_pMesh, "targets" );
	if ( !pDmeCombo )
		return SetErrorString( "No DmeCombinationOperator On Mesh \"%s\"", m_pMesh->GetName() );

	CUtlVector< const char * > tmpDominators;
	for ( int i = 0; i < dominators.Count(); ++i )
	{
		tmpDominators.AddToTail( dominators[ i ].Get() );
	}

	CUtlVector< const char * > tmpSupressed;
	for ( int i = 0; i < supressed.Count(); ++i )
	{
		tmpSupressed.AddToTail( supressed[ i ].Get() );
	}

	pDmeCombo->AddDominationRule(
		tmpDominators.Count(), ( const char ** )tmpDominators.Base(),
		tmpSupressed.Count(), ( const char ** )tmpSupressed.Base() );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	AddDominationRule,
	"{ < $dominator > [ , ... ] }, { < $supressed > [ , ... ] }",
	"Create a domination rule.  A lua array of strings is passed as the 1st argument which are "
	"the controls which are the dominators and a lua array of strings is passed as the 2nd "
	"argument which are the controls to be supressed by the dominators."
	"e.g. AddDominationRule( { \"foo\" }, { \"bar\", \"bob\" } );" )
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	CUtlVector< CUtlString > dominators;

	if ( lua_istable( pLuaState, 1 ) )
	{
		const int nCount = luaL_getn( pLuaState, 1 );
		for ( int i = 1; i <= nCount; ++i )
		{
			lua_rawgeti( pLuaState, 1, i );
			if ( lua_isstring( pLuaState, -1 ) )
			{
				dominators.AddToTail( lua_tostring( pLuaState, -1 ) );
			}
			lua_pop( pLuaState, 1 );
		}
	}

	if ( dominators.Count() <= 0 )
		return LuaFunc_s::m_dmxEdit.LuaError( pLuaState, "No Dominator Controls Specified" );

	CUtlVector< CUtlString > supressed;

	if ( lua_istable( pLuaState, 2 ) )
	{
		const int nCount = luaL_getn( pLuaState, 2 );
		for ( int i = 1; i <= nCount; ++i )
		{
			lua_rawgeti( pLuaState, 2, i );
			if ( lua_isstring( pLuaState, -1 ) )
			{
				supressed.AddToTail( lua_tostring( pLuaState, -1 ) );
			}
			lua_pop( pLuaState, 1 );
		}
	}

	if ( supressed.Count() <= 0 )
		return LuaFunc_s::m_dmxEdit.LuaError( pLuaState, "No Supressed Controls Specified" );

	if ( LuaFunc_s::m_dmxEdit.AddDominationRule( dominators, supressed ) )
		return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::SetStereoControl( const char *pControlName, bool bStereo )
{
	if ( !m_pMesh )
		return SetErrorString( "No Mesh" );

	CDmeCombinationOperator *pDmeCombo = FindReferringElement< CDmeCombinationOperator >( m_pMesh, "targets" );
	if ( !pDmeCombo )
		return SetErrorString( "No DmeCombinationOperator On Mesh \"%s\"", m_pMesh->GetName() );

	const ControlIndex_t nControlIndex = pDmeCombo->FindControlIndex( pControlName );
	if ( nControlIndex < 0 )
		return SetErrorString( "No Control Named \"%s\" On DmeCombinationOperator \"%s\" On Mesh \"%s\"", pControlName, pDmeCombo->GetName(), m_pMesh->GetName() );

	pDmeCombo->SetStereoControl( nControlIndex, bStereo );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	SetStereoControl,
	" < $controlName > [ , < true | false > ]",
	"Sets the specified morph control to be stereo if the 2nd argument is true."
	"If the 2nd argument is omitted, true is assumed" )
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	const char *pControlName = luaL_checkstring( pLuaState, 1 );
	const bool bStereo = lua_isboolean( pLuaState, 2 ) ? lua_toboolean( pLuaState, 2 ) != 0 : true;

	if ( LuaFunc_s::m_dmxEdit.SetStereoControl( pControlName, bStereo ) )
		return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::SetEyelidControl( const char *pControlName, bool bEyelid )
{
	if ( !m_pMesh )
		return SetErrorString( "No Mesh" );

	CDmeCombinationOperator *pDmeCombo = FindReferringElement< CDmeCombinationOperator >( m_pMesh, "targets" );
	if ( !pDmeCombo )
		return SetErrorString( "No DmeCombinationOperator On Mesh \"%s\"", m_pMesh->GetName() );

	const ControlIndex_t nControlIndex = pDmeCombo->FindControlIndex( pControlName );
	if ( nControlIndex < 0 )
		return SetErrorString( "No Control Named \"%s\" On DmeCombinationOperator \"%s\" On Mesh \"%s\"", pControlName, pDmeCombo->GetName(), m_pMesh->GetName() );

	pDmeCombo->SetEyelidControl( nControlIndex, bEyelid );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	SetEyelidControl,
	" < $controlName > [ , < true | false > ]",
	"Sets the specified morph control to be an eyelid control if the 2nd argument is true."
	"If the 2nd argument is omitted, true is assumed" )
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	const char *pControlName = luaL_checkstring( pLuaState, 1 );
	const bool bStereo = lua_isboolean( pLuaState, 2 ) ? lua_toboolean( pLuaState, 2 ) != 0 : true;

	if ( LuaFunc_s::m_dmxEdit.SetEyelidControl( pControlName, bStereo ) )
		return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
float CDmxEdit::MaxDeltaDistance( const char *pDeltaName )
{
	CDmeVertexDeltaData *pDelta = FindDeltaState( pDeltaName );

	if ( !pDelta)
		return 0.0f;

	float fSqMaxDelta = 0.0f;
	float fTmpSqLength;

	const CUtlVector< Vector > &positions = pDelta->GetPositionData();

	for ( int i = 0; i < positions.Count(); ++i )
	{
		fTmpSqLength = positions[ i ].LengthSqr();
		if ( fTmpSqLength < fSqMaxDelta )
			continue;

		fSqMaxDelta = fTmpSqLength;
	}

	return sqrt( fSqMaxDelta );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	MaxDeltaDistance,
	" < $delta >",
	"Returns the maximum distance any vertex moves in the specified delta.  Returns 0 if the delta specified doesn't exist" )
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	const char *pDeltaName = luaL_checkstring( pLuaState, 1 );

	lua_pushnumber( pLuaState, LuaFunc_s::m_dmxEdit.MaxDeltaDistance( pDeltaName ) );

	return 1;
}


struct DVec
{
	double x;
	double y;
	double z;

	DVec()
		: x( 0.0 )
		, y( 0.0 )
		, z( 0.0 )
	{
	}

	DVec &operator=( const Vector &rhs )
	{
		x = rhs.x;
		y = rhs.y;
		z = rhs.z;

		return *this;
	}
};

//-----------------------------------------------------------------------------
//
// An Efficient Bounding Sphere
// by Jack Ritter
// from "Graphics Gems", Academic Press, 1990
//
// Routine to calculate tight bounding sphere over
// a set of points in 3D
// This contains the routine find_bounding_sphere(),
// the struct definition, and the globals used for parameters.
// The abs() of all coordinates must be < BIGNUMBER
// Code written by Jack Ritter and Lyle Rains.
//-----------------------------------------------------------------------------
void FindBoundingSphere( CUtlVector< Vector > &points, Vector &cen, float &fRad )
{
	if ( points.Count() <= 0 )
	{
		cen.Zero();
		fRad = 0.0f;

		return;
	}

	double dx,dy,dz;
	double rad_sq,xspan,yspan,zspan,maxspan;
	double old_to_p,old_to_p_sq,old_to_new;
	Vector xmin,xmax,ymin,ymax,zmin,zmax,dia1,dia2;

//	DVec cen;
	double rad;

	cen = points[ 0 ];
	fRad = 0.0f;

	xmin.x=ymin.y=zmin.z= FLT_MAX; /* initialize for min/max compare */
	xmax.x=ymax.y=zmax.z= -FLT_MAX;

	for ( int i = 0; i < points.Count(); ++i )
	{
		const Vector &caller_p = points[ i ];

		if (caller_p.x<xmin.x)
			xmin = caller_p; /* New xminimum point */
		if (caller_p.x>xmax.x)
			xmax = caller_p;
		if (caller_p.y<ymin.y)
			ymin = caller_p;
		if (caller_p.y>ymax.y)
			ymax = caller_p;
		if (caller_p.z<zmin.z)
			zmin = caller_p;
		if (caller_p.z>zmax.z)
			zmax = caller_p;
	}

	/* Set xspan = distance between the 2 points xmin & xmax (squared) */
	dx = xmax.x - xmin.x;
	dy = xmax.y - xmin.y;
	dz = xmax.z - xmin.z;
	xspan = dx*dx + dy*dy + dz*dz;

	/* Same for y & z spans */
	dx = ymax.x - ymin.x;
	dy = ymax.y - ymin.y;
	dz = ymax.z - ymin.z;
	yspan = dx*dx + dy*dy + dz*dz;

	dx = zmax.x - zmin.x;
	dy = zmax.y - zmin.y;
	dz = zmax.z - zmin.z;
	zspan = dx*dx + dy*dy + dz*dz;

	/* Set points dia1 & dia2 to the maximally separated pair */
	dia1 = xmin; dia2 = xmax; /* assume xspan biggest */
	maxspan = xspan;
	if (yspan>maxspan)
	{
		maxspan = yspan;
		dia1 = ymin; dia2 = ymax;
	}
	if (zspan>maxspan)
	{
		dia1 = zmin; dia2 = zmax;
	}

	/* dia1,dia2 is a diameter of initial sphere */
	/* calc initial center */
	cen.x = (dia1.x+dia2.x)/2.0;
	cen.y = (dia1.y+dia2.y)/2.0;
	cen.z = (dia1.z+dia2.z)/2.0;
	/* calculate initial radius**2 and radius */
	dx = dia2.x-cen.x; /* x component of radius vector */
	dy = dia2.y-cen.y; /* y component of radius vector */
	dz = dia2.z-cen.z; /* z component of radius vector */
	rad_sq = dx*dx + dy*dy + dz*dz;
	rad = sqrt(rad_sq);

	/* SECOND PASS: increment current sphere */

	for ( int i = 0; i < points.Count(); ++i )
	{
		const Vector &caller_p = points[ i ];

		dx = caller_p.x-cen.x;
		dy = caller_p.y-cen.y;
		dz = caller_p.z-cen.z;
		old_to_p_sq = dx*dx + dy*dy + dz*dz;
		if (old_to_p_sq > rad_sq) 	/* do r**2 test first */
		{ 	/* this point is outside of current sphere */
			old_to_p = sqrt(old_to_p_sq);
			/* calc radius of new sphere */
			rad = (rad + old_to_p) / 2.0;
			rad_sq = rad*rad; 	/* for next r**2 compare */
			old_to_new = old_to_p - rad;
			/* calc center of new sphere */
			cen.x = (rad*cen.x + old_to_new*caller_p.x) / old_to_p;
			cen.y = (rad*cen.y + old_to_new*caller_p.y) / old_to_p;
			cen.z = (rad*cen.z + old_to_new*caller_p.z) / old_to_p;
		}
	}

	/*
	fCen.x = cen.x;
	fCen.y = cen.y;
	fCen.z = cen.z;
	*/
	fRad = rad;
}


//-----------------------------------------------------------------------------
// Take each delta value, add it to the base state, find the bounding sphere
// of all of the resulting vertices, return the distance from the center to
// the 
//-----------------------------------------------------------------------------
float CDmxEdit::DeltaRadius( const char *pDeltaName )
{
	CDmeVertexData *pBind = GetBindState();
	if ( !pBind )
		return 0.0f;

	CDmeVertexDeltaData *pDelta = FindDeltaState( pDeltaName );

	if ( !pDelta )
		return 0.0f;

	const CUtlVector< Vector > &bindPos = pBind->GetPositionData();
	const CUtlVector< int > &deltaPosIndices = pDelta->GetVertexIndexData( CDmeVertexData::FIELD_POSITION );
	const CUtlVector< Vector > &deltaPos = pDelta->GetPositionData(); 

	Assert( deltaPosIndices.Count() == deltaPos.Count() );

	CUtlVector< Vector > newPos;
	newPos.SetSize( deltaPos.Count() );

	for ( int i = 0; i < newPos.Count(); ++i )
	{
		newPos[ i ] = bindPos[ deltaPosIndices[ i ] ] + deltaPos[ i ];
	}

	Vector center;
	float radius = 0.0;

	FindBoundingSphere( newPos, center, radius );

	return radius;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	DeltaRadius,
	" < $delta >",
	"Returns the radius of the specified delta, if it exists.  Radius of a delta is defined as the radius of the tight bounding sphere containing all of the deltas added to their base state values" )
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	const char *pDeltaName = luaL_checkstring( pLuaState, 1 );

	lua_pushnumber( pLuaState, LuaFunc_s::m_dmxEdit.DeltaRadius( pDeltaName ) );

	return 1;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
float CDmxEdit::SelectionRadius()
{
	if ( !m_pCurrentSelection || m_pCurrentSelection->Count() == 0 )
		return 0.0f;

	if ( !m_pMesh )
		return 0.0f;

	CDmeVertexData *pBase = m_pMesh->GetCurrentBaseState();
	if ( !pBase )
		return 0.0f;

	CUtlVector< int > selection;
	m_pCurrentSelection->GetComponents( selection );
	const CUtlVector< Vector > &pos = pBase->GetPositionData();

	CUtlVector< Vector > newPos;
	newPos.SetSize( selection.Count() );

	for ( int i = 0; i < newPos.Count(); ++i )
	{
		newPos[ i ] = pos[ selection[ i ] ];
	}

	Vector center;
	float radius = 0.0;

	FindBoundingSphere( newPos, center, radius );

	return radius;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	SelectionRadius,
	"",
	"Returns the radius of the currently selected vertices, if nothing is selected, returns 0.  Radius of a the vertices defined as the radius of the tight bounding sphere containing all of the vertices" )
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	lua_pushnumber( pLuaState, LuaFunc_s::m_dmxEdit.SelectionRadius() );

	return 1;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEdit::SetWrinkleScale( const char *pControlName, const char *pRawControlName, float flScale )
{
	if ( !m_pMesh )
		return SetErrorString( "No Mesh" );

	CDmeCombinationOperator *pDmeCombo = FindReferringElement< CDmeCombinationOperator >( m_pMesh, "targets" );
	if ( !pDmeCombo )
		return SetErrorString( "No DmeCombinationOperator On Mesh \"%s\"", m_pMesh->GetName() );

	const ControlIndex_t nControlIndex = pDmeCombo->FindControlIndex( pControlName );

	if ( nControlIndex < 0 )
		return SetErrorString( "Cannot Find Control \"%s\"", pControlName );

	// Check to see if the raw control exists

	bool bFoundRawControl = false;
	for ( int nRawControlIndex = 0; nRawControlIndex < pDmeCombo->GetRawControlCount( nControlIndex ); ++nRawControlIndex )
	{
		if ( !Q_strcmp( pRawControlName, pDmeCombo->GetRawControlName( nControlIndex, nRawControlIndex ) ) )
		{
			bFoundRawControl = true;
			break;
		}
	}

	if ( !bFoundRawControl )
	{
		CUtlString rawControls;
		for ( int nRawControlIndex = 0; nRawControlIndex < pDmeCombo->GetRawControlCount( nControlIndex ); ++nRawControlIndex )
		{
			if ( rawControls.Length() > 0 )
			{
				rawControls += ", ";
			}
			rawControls += pDmeCombo->GetRawControlName( nControlIndex, nRawControlIndex );
		}

		return SetErrorString( "Control \"%s\" Does Not Have Raw Control \"%s\", Raw Controls: %s ", pControlName, pRawControlName, rawControls.Get() );
	}

	pDmeCombo->SetWrinkleScale( nControlIndex, pRawControlName, flScale );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
LUA_COMMAND(
	SetWrinkleScale,
	"< $controlName >, < $rawControlName >, < #scale >",
	"Sets the wrinkle scale of the raw control of the specified control. "
	"NOTE: SetWrinkleScale merely sets the wrinkle scale value of the specified delta on the combination operator.  "
	"This value is only used if ComputeWrinkles() is called after all SetWrinkleScale() calls are made.  "
	"A wrinkle scale of 0 means there will be no wrinkle deltas.  "
	"The function to compute a single delta's wrinkle values, ComputeWrinkle() does not use the wrinkle scale value at all.  "
	"Negative (-) is compress, Positive (+) is stretch."
	)
{
	LuaFunc_s::m_dmxEdit.SetFuncString( pLuaState );

	const char *pControlName = luaL_checkstring( pLuaState, 1 );
	const char *pRawControlName = luaL_checkstring( pLuaState, 2 );
	const double flScale = luaL_checknumber( pLuaState, 3 );
	const float fScale = flScale;

	if ( LuaFunc_s::m_dmxEdit.SetWrinkleScale( pControlName, pRawControlName, fScale ) )
		return LuaFunc_s::m_dmxEdit.LuaOk( pLuaState );

	return LuaFunc_s::m_dmxEdit.LuaError( pLuaState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmxEdit::Error( const tchar *pMsgFormat, ... )
{
	static Color outColor( 1, 1, 1, 1 );
	SetErrorState();
	va_list args;
	va_start( args, pMsgFormat );
	tchar pTempBuffer[5020];
	assert( _tcslen( pMsgFormat ) < sizeof( pTempBuffer) ); // check that we won't artificially truncate the string
	_vsntprintf( pTempBuffer, sizeof( pTempBuffer ) - 1, pMsgFormat, args );
	pTempBuffer[ ARRAYSIZE(pTempBuffer) - 1 ] = 0;
	printf( "%s", pTempBuffer );
	fflush( stdout );
	va_end(args);

	if ( CommandLine()->FindParm( "-coe" ) || CommandLine()->FindParm( "-continueOnError" ) )
		return;

	if ( Plat_IsInDebugSession() )
	{
		Msg( "\nPress q To Quit, d To Debug Or Any Key To Continue After Error..." );
	}
	else
	{
		Msg( "\nPress q To Quit Or Any Key To Continue After Error..." );
	}

	int ch = getch();
	if ( ch == 'q' || ch == 'Q' )
	{
		Unload();
		exit( 1 );
	}
	else if ( Plat_IsInDebugSession() && ch == 'd' || ch == 'D' )
	{
		DebuggerBreakIfDebugging();
	}
	Msg( "\n" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CPauseOnScopeExit
{
public:
	CPauseOnScopeExit( CDmxEdit &dmxEdit )
	: m_dmxEdit( dmxEdit )
	{
	}

	~CPauseOnScopeExit()
	{
		m_dmxEdit.Unload();

		if ( m_dmxEdit.ErrorState() )
		{
			Msg( "\nPress Any Key To Continue...\n" );
			getch();
		}
	}

protected:

	CDmxEdit &m_dmxEdit;
};


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
void CDmxEdit::DoIt()
{
	ResetErrorState();

	// Just for sugary sweet syntax
	CDmxEditProxy base( *this );

	CPauseOnScopeExit scopePause( *this );

	CDisableUndoScopeGuard guard;

	return;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmxEdit::CleanupWork()
{
	if ( m_pMesh )
	{
		// Cleanup
		m_pMesh->RemoveAttribute( "selection" );
		if ( m_pCurrentSelection )
		{
			g_pDataModel->DestroyElement( m_pCurrentSelection->GetHandle() );
			m_pCurrentSelection = NULL;
		}

		CDmeVertexData *pBind( m_pMesh->FindBaseState( "bind" ) );

		if ( pBind )
		{
			m_pMesh->SetCurrentBaseState( "bind" );
		}

		CDmeVertexData *pWork( m_pMesh->FindBaseState( "__dmxEdit_work" ) );

		if ( pWork )
		{
			m_pMesh->DeleteBaseState( "__dmxEdit_work" );
		}
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmxEdit::CreateWork()
{
	if ( m_pMesh )
	{
		// Undo the cleanup work performed above
		CDmeVertexData *pBind = m_pMesh->FindBaseState( "bind" );

		if ( pBind )
		{
			// Ensure "work" isn't saved!
			CDmeVertexData *pWork = m_pMesh->FindOrCreateBaseState( "__dmxEdit_work" );

			if ( pWork )
			{
				pBind->CopyTo( pWork );
				m_pMesh->SetCurrentBaseState( "__dmxEdit_work" );
			}
		}

		m_pCurrentSelection = CreateElement< CDmeSingleIndexedComponent >( "selection", m_pRoot->GetFileId() );
		m_pMesh->SetValue( "selection", m_pCurrentSelection );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmxEdit::GetFuncArg( lua_State *pLuaState, int nIndex, CUtlString &funcString )
{
	switch ( lua_type( pLuaState, nIndex ) )
	{
		case LUA_TNUMBER:
			funcString += lua_tonumber( pLuaState, nIndex );
			break;
		case LUA_TBOOLEAN:
			funcString += ( lua_toboolean( pLuaState, nIndex ) ? "true" : "false" );
			break;
		case LUA_TSTRING:
			funcString += "\"";
			funcString += lua_tostring( pLuaState, nIndex );
			funcString += "\"";
			break;
		case LUA_TTABLE:
			{
				funcString += "{ ";
				const int nCount = luaL_getn( pLuaState, nIndex );
				for ( int i = 1; i <= nCount; ++i )
				{
					lua_rawgeti( pLuaState, nIndex, i );
					if ( i > 1 )
					{
						funcString += ", ";
					}
					GetFuncArg( pLuaState, lua_gettop( pLuaState ), funcString );
					lua_pop( pLuaState, 1 );
				}
				funcString += " }";
			}
			break;
		case LUA_TNIL:
		case LUA_TFUNCTION:
		case LUA_TUSERDATA:
		case LUA_TTHREAD:
		case LUA_TLIGHTUSERDATA:
			funcString += "<";
			funcString += lua_typename( pLuaState, nIndex );
			funcString += ">";
			break;
	}
}


//-----------------------------------------------------------------------------
// Sets the current function string and line number
//-----------------------------------------------------------------------------
const CUtlString &CDmxEdit::SetFuncString( lua_State *pLuaState )
{
	m_funcString.SetLength( 0 );

	lua_Debug ar;

	lua_getstack( pLuaState, 0, &ar );
	lua_getinfo( pLuaState, "Sln", &ar );

	const char *pFuncName = ar.name;
	if ( pFuncName )
	{
		m_funcString = pFuncName;
	}
	else
	{
		m_funcString = "Unknown";
	}

	m_funcString += "(";
	if ( lua_gettop( pLuaState ) >= 1 )
		m_funcString += " ";

	for ( int i = 1; i <= lua_gettop( pLuaState ); ++i )
	{
		if ( i > 1 )
		{
			m_funcString += ", ";
		}

		GetFuncArg( pLuaState, i, m_funcString );
	}

	if ( lua_gettop( pLuaState ) >= 1 )
		m_funcString += " ";
	m_funcString += ");";

	lua_getstack( pLuaState, 1, &ar );
	lua_getinfo( pLuaState, "Sln", &ar );

	m_lineNo = ar.currentline;

	m_sourceFile.SetLength( 0 );
	if ( ar.source )
	{
		if ( *( ar.source ) == '@' )
		{
			m_sourceFile = ar.source + 1;
		}
		else
		{
			m_sourceFile = ar.source;
		}
	}

	return m_funcString;
}


//=============================================================================
// Lua Interface
//=============================================================================


//=============================================================================
// CDmxEditLua
//=============================================================================


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmxEditLua::CDmxEditLua()
: m_pLuaState( lua_open() )
{
	luaL_openlibs( m_pLuaState );

	for ( LuaFunc_s *pFunc = LuaFunc_s::s_pFirstFunc; pFunc; pFunc = pFunc->m_pNextFunc )
	{
		lua_pushcfunction( m_pLuaState, pFunc->m_pFunc );
		lua_setglobal( m_pLuaState, pFunc->m_pFuncName );
	}

	const char *pLuaInit =
	"vsLuaDir = os.getenv( \"VPROJECT\" );\n"
	"vsLuaDir = string.gsub( vsLuaDir, \"[\\\\/]\", \"/\" );\n"
	"vsLuaDir = string.gsub( vsLuaDir, \"/*$\", \"\" );\n"
	"vsLuaDir = ( string.gsub( vsLuaDir, \"[^/]+$\", \"sdktools/lua\" ) .. \"/?.lua\" );\n"
	"package.path = ( package.path .. \";\" .. vsLuaDir );\n"
	"require( \"vs\" );\n";

	if ( luaL_loadstring( m_pLuaState, pLuaInit ) || lua_pcall( m_pLuaState, 0, LUA_MULTRET, 0 ) )
	{
		Error( "Error: %s\n", lua_tostring( m_pLuaState, -1 ) );
	}

}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEditLua::DoIt( const char *pFilename )
{
	CDisableUndoScopeGuard guard;

	bool retVal = true;

	LuaFunc_s::m_dmxEdit.SetScriptFilename( pFilename );

	if ( luaL_dofile( m_pLuaState, pFilename ) )
	{
		Error( "Error: %s\n", lua_tostring( m_pLuaState, -1 ) );
		retVal = false;
	}

	lua_close( m_pLuaState );

	LuaFunc_s::m_dmxEdit.Unload();

	return retVal;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmxEditLua::SetVar( const CUtlString &var, const CUtlString &val )
{
	CUtlString sLuaCmd( var );
	sLuaCmd += " = ";
	sLuaCmd += val;
	sLuaCmd += ";";

	if ( luaL_loadstring( m_pLuaState, sLuaCmd.Get() ) || lua_pcall( m_pLuaState, 0, LUA_MULTRET, 0 ) )
	{
		Error( "Error: %s\n", lua_tostring( m_pLuaState, -1 ) );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmxEditLua::SetGame( const CUtlString &game )
{
	CUtlString sLuaCmd( "vs.SetGame( " );
	sLuaCmd += game;
	sLuaCmd += " );";

	if ( luaL_loadstring( m_pLuaState, sLuaCmd.Get() ) || lua_pcall( m_pLuaState, 0, LUA_MULTRET, 0 ) )
	{
		Error( "Error: %s\n", lua_tostring( m_pLuaState, -1 ) );
	}
}