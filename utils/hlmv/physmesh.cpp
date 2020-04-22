//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
/***
*
*	Copyright (c) 1998, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

#include "filesystem.h"
#include "vphysics/constraints.h"
#include "phyfile.h"
#include "physdll.h"
#include "physmesh.h"
#include "mathlib/mathlib.h"
#include <stddef.h>
#include "utlvector.h"
#include "commonmacros.h"
#include "studiomodel.h"
#include "tier1/strtools.h"
#include "bone_setup.h"
#include "fmtstr.h"
#include "vcollide_parse.h"

int FindPhysprop( const char *pPropname );

bool LoadPhysicsProperties( void );
extern int FindBoneIndex( CStudioHdr *pstudiohdr, const char *pName );


struct collisionpair_t
{
	int		object0;
	int		object1;
	collisionpair_t *pNext;
};

class CStudioPhysics : public IStudioPhysics
{
public:
	CStudioPhysics( void )
	{
		m_pList = NULL;
		m_listCount = 0;
		m_mass = 0;
		m_noselfCollisions = false;
		m_pCollisionPairs = NULL;
		memset( &m_edit, 0, sizeof(editparams_t) );
	}

	~CStudioPhysics( void ) 
	{
		if ( physcollision )
		{
			for ( int i = 0; i < m_listCount; i++ )
			{
				physcollision->DestroyDebugMesh( m_pList[i].m_vertCount, m_pList[i].m_pVerts );
				physcollision->DestroyQueryModel( m_pList[i].m_pCollisionModel );
			}
		}
		delete[] m_pList;
	}

	int	Count( void )
	{
		return m_listCount;
	}

	CPhysmesh *GetMesh( int index )
	{
		if ( index < m_listCount )
			return m_pList + index;

		return NULL;
	}

	float	GetMass( void ) { return m_mass; }

	void AddCollisionPair( int index0, int index1 )
	{
		collisionpair_t *pPair = new collisionpair_t;
		pPair->object0 = index0;
		pPair->object1 = index1;
		pPair->pNext = m_pCollisionPairs;
		m_pCollisionPairs = pPair;

	}

	void	Load( MDLHandle_t handle );
	char	*DumpQC( void );
	void	ParseKeydata( void );

	vcollide_t *GetVCollide()
	{
		return g_pMDLCache->GetVCollide( m_MDLHandle );
	}

	CPhysmesh		*m_pList;
	MDLHandle_t		m_MDLHandle;
	int				m_listCount;

	float			m_mass;
	editparams_t	m_edit;
	bool			m_noselfCollisions;
	collisionpair_t	*m_pCollisionPairs;
};


void CPhysmesh::Clear( void )
{
	memset( this, 0, sizeof(*this) );
	memset( &m_constraint, 0, sizeof(m_constraint) );
	m_constraint.parentIndex = -1;
	m_constraint.childIndex = -1;
}


IStudioPhysics *LoadPhysics( MDLHandle_t mdlHandle )
{
	CStudioPhysics *pPhysics = new CStudioPhysics;
	pPhysics->Load( mdlHandle );
	return pPhysics;
}

void DestroyPhysics( IStudioPhysics *pStudioPhysics )
{
	CStudioPhysics *pPhysics = static_cast<CStudioPhysics*>( pStudioPhysics );
	if ( pPhysics )
	{
		delete pPhysics;
	}
}

void CStudioPhysics::Load( MDLHandle_t mdlHandle )
{
	m_MDLHandle = mdlHandle;

	LoadPhysicsProperties();

	vcollide_t *pVCollide = GetVCollide( );
	if ( !pVCollide )
	{
		m_pList = NULL;
		m_listCount = 0;
		return;
	}

	m_pList = new CPhysmesh[pVCollide->solidCount];
	m_listCount = pVCollide->solidCount;

	int i;

	for ( i = 0; i < pVCollide->solidCount; i++ )
	{
		m_pList[i].Clear();
		m_pList[i].m_vertCount = physcollision->CreateDebugMesh( pVCollide->solids[i], &m_pList[i].m_pVerts );
		m_pList[i].m_pCollisionModel = physcollision->CreateQueryModel( pVCollide->solids[i] );
	}

	ParseKeydata();

	CStudioHdr studioHdr( g_pMDLCache->GetStudioHdr( mdlHandle ), g_pMDLCache );
	for ( i = 0; i < pVCollide->solidCount; i++ )
	{
		CPhysmesh *pmesh = m_pList + i;
		int boneIndex = FindBoneIndex( &studioHdr, pmesh->m_boneName );
		if ( boneIndex < 0 )
			continue;

		if ( pmesh->m_constraint.parentIndex >= 0 )
		{
			CPhysmesh *pparent = m_pList + pmesh->m_constraint.parentIndex;
			int parentIndex = FindBoneIndex( &studioHdr, pparent->m_boneName );
			Studio_CalcBoneToBoneTransform( &studioHdr, boneIndex, parentIndex, pmesh->m_matrix );
		}
		else
		{
			MatrixInvert( studioHdr.pBone(boneIndex)->poseToBone, pmesh->m_matrix );
		}
	}

	// doesn't have a root bone?  Make it the first bone
	if ( !m_edit.rootName[0] )
	{
		strcpy( m_edit.rootName, m_pList[0].m_boneName );
	}
}


class CEditParse : public IVPhysicsKeyHandler
{
public:
	virtual void ParseKeyValue( void *pCustom, const char *pKey, const char *pValue )
	{
		editparams_t *pEdit = (editparams_t *)pCustom;
		if ( !strcmpi( pKey, "rootname" ) )
		{
			strncpy( pEdit->rootName, pValue, sizeof(pEdit->rootName) );
		}
		else if ( !strcmpi( pKey, "totalmass" ) )
		{
			pEdit->totalMass = atof( pValue );
		}
		else if ( !strcmpi( pKey, "concave" ) )
		{
			pEdit->concave = atoi( pValue );
		}
		else if ( !strcmpi( pKey, "jointmerge" ) )
		{
			char tmp[1024];
			char parentName[512], childName[512];
			Q_strncpy( tmp, pValue, 1024 );
			char *pWord = strtok( tmp, "," );
			Q_strncpy( parentName, pWord, sizeof(parentName) );
			pWord = strtok( NULL, "," );
			Q_strncpy( childName, pWord, sizeof(childName) );
			if ( pEdit->mergeCount < ARRAYSIZE(pEdit->mergeList) )
			{
				merge_t *pMerge = &pEdit->mergeList[pEdit->mergeCount];
				pEdit->mergeCount++;
				pMerge->parent = g_pStudioModel->FindBone(parentName);
				pMerge->child = g_pStudioModel->FindBone(childName);
			}
		}
	}
	virtual void SetDefaults( void *pCustom )
	{
		editparams_t *pEdit = (editparams_t *)pCustom;
		memset( pEdit, 0, sizeof(*pEdit) );
	}
};

class CRagdollCollisionRulesParse : public IVPhysicsKeyHandler
{
public:
	CRagdollCollisionRulesParse( CStudioPhysics *pStudio ) : m_pStudio(pStudio) 
	{
		pStudio->m_noselfCollisions = false;
	}

	virtual void ParseKeyValue( void *pData, const char *pKey, const char *pValue )
	{
		if ( !strcmpi( pKey, "selfcollisions" ) )
		{
			// keys disabled by default
			Assert( atoi(pValue) == 0 );
			m_pStudio->m_noselfCollisions = true;
		}
		else if ( !strcmpi( pKey, "collisionpair" ) )
		{
			if ( !m_pStudio->m_noselfCollisions )
			{
				char tmp[1024];
				Q_strncpy( tmp, pValue, 1024 );
				char *pWord = strtok( tmp, "," );
				int index0 = atoi(pWord);
				pWord = strtok( NULL, "," );
				int index1 = atoi(pWord);
				m_pStudio->AddCollisionPair( index0, index1 );
			}
			else
			{
				Assert(0);
			}
		}
	}
	virtual void SetDefaults( void *pData ) {}

private:
	CStudioPhysics *m_pStudio;
};

class CSolidParse : public IVPhysicsKeyHandler
{
public:
	virtual void ParseKeyValue( void *pCustom, const char *pKey, const char *pValue )
	{
		hlmvsolid_t *pSolid = (hlmvsolid_t *)pCustom;
		if ( !strcmpi( pKey, "massbias" ) )
		{
			pSolid->massBias = atof( pValue );
		}
		else
		{
			printf("Bad key %s!!\n", pKey);
		}
	}
	virtual void SetDefaults( void *pCustom )
	{
		hlmvsolid_t *pSolid = (hlmvsolid_t *)pCustom;
		pSolid->massBias = 1.0;
	}
};

void CStudioPhysics::ParseKeydata( void )
{
	IVPhysicsKeyParser *pParser = physcollision->VPhysicsKeyParserCreate( GetVCollide()->pKeyValues );

	while ( !pParser->Finished() )
	{
		const char *pBlock = pParser->GetCurrentBlockName();
		if ( !stricmp( pBlock, "solid" ) )
		{
			hlmvsolid_t solid;
			CSolidParse solidParse;

			pParser->ParseSolid( &solid, &solidParse );
			solid.surfacePropIndex = FindPhysprop( solid.surfaceprop );

			if ( solid.index >= 0 && solid.index < m_listCount )
			{
				strcpy( m_pList[solid.index].m_boneName, solid.name );
				memcpy( &m_pList[solid.index].m_solid, &solid, sizeof(solid) );
			}
		}
		else if ( !stricmp( pBlock, "ragdollconstraint" ) )
		{
			constraint_ragdollparams_t constraint;
			pParser->ParseRagdollConstraint( &constraint, NULL );
			if ( constraint.childIndex >= 0 && constraint.childIndex < m_listCount )
			{
				// In the editor / qc these show up as 5X so that 1.0 is the default
				constraint.axes[0].torque *= 5;
				constraint.axes[1].torque *= 5;
				constraint.axes[2].torque *= 5;
				m_pList[constraint.childIndex].m_constraint = constraint;
			}
		}
		else if ( !stricmp( pBlock, "editparams" ) )
		{
			CEditParse editParse;
			pParser->ParseCustom( &m_edit, &editParse );
			m_mass = m_edit.totalMass;
		}
		else if ( !strcmpi( pBlock, "collisionrules" ) )
		{
			CRagdollCollisionRulesParse rules(this);
			pParser->ParseCustom( NULL, &rules );
		}
		else
		{
			pParser->SkipBlock();
		}
	}
	physcollision->VPhysicsKeyParserDestroy( pParser );
}


int FindPhysprop( const char *pPropname )
{
	if ( physprop )
	{
		int count = physprop->SurfacePropCount();
		for ( int i = 0; i < count; i++ )
		{
			if ( !strcmpi( pPropname, physprop->GetPropName(i) ) )
				return i;
		}
	}
	return 0;
}



class CTextBuffer
{
public:
	CTextBuffer( void ) {}
	~CTextBuffer( void ) {}

	inline int GetSize( void ) { return m_buffer.Size(); }
	inline char *GetData( void ) { return m_buffer.Base(); }
	
	void WriteText( const char *pText )
	{
		int len = strlen( pText );
		CopyData( pText, len );
	}

	void Terminate( void ) { CopyData( "\0", 1 ); }

	void CopyData( const char *pData, int len )
	{
		int offset = m_buffer.AddMultipleToTail( len );
		memcpy( m_buffer.Base() + offset, pData, len );
	}

private:
	CUtlVector<char> m_buffer;
};


struct physdefaults_t
{
	int   surfacePropIndex;
	float inertia;
	float damping;
	float rotdamping;
};

//-----------------------------------------------------------------------------
// Purpose: Nasty little routine (that was easy to code) to find the most common
//			value in an array of structs containing that as a member
// Input  : *pStructArray - pointer to head of struct array
//			arrayCount - number of elements in the array
//			structSize - size of each element
//			fieldOffset - offset to the float we're finding
// Output : static T - most common value
//-----------------------------------------------------------------------------
template< class T >
static T FindCommonValue( void *pStructArray, int arrayCount, int structSize, int fieldOffset )
{
	int maxCount = 0;
	T maxVal = 0;

	// BUGBUG: This is O(n^2), but n is really small
	for ( int i = 0; i < arrayCount; i++ )
	{
		// current = struct[i].offset
		T current = *(T *)((char *)pStructArray + (i*structSize) + fieldOffset);
		int currentCount = 0;

		// if everything is set to the default, this is almost O(n)
		if ( current == maxVal )
			continue;

		for ( int j = 0; j < arrayCount; j++ )
		{
			// value = struct[j].offset
			T value = *(T *)((char *)pStructArray + (j*structSize) + fieldOffset);
			if ( value == current )
				currentCount++;
		}

		if ( currentCount > maxCount )
		{
			maxVal = current;
			maxCount = currentCount;
		}
	}

	return maxVal;
}

static void CalcDefaultProperties( CPhysmesh *pList, int listCount, physdefaults_t &defs )
{
	defs.surfacePropIndex = FindCommonValue<int>( pList, listCount, sizeof(CPhysmesh), offsetof(CPhysmesh, m_solid.surfacePropIndex) );
	defs.inertia = FindCommonValue<float>( pList, listCount, sizeof(CPhysmesh), offsetof(CPhysmesh, m_solid.params.inertia) );
	defs.damping = FindCommonValue<float>( pList, listCount, sizeof(CPhysmesh), offsetof(CPhysmesh, m_solid.params.damping) );
	defs.rotdamping = FindCommonValue<float>( pList, listCount, sizeof(CPhysmesh), offsetof(CPhysmesh, m_solid.params.rotdamping) );
}

static void DumpModelProperties( CTextBuffer &out, float mass, physdefaults_t &defs )
{
	char tmpbuf[1024];
	sprintf( tmpbuf, "\t$mass %.1f\r\n", mass );
	out.WriteText( tmpbuf );
	sprintf( tmpbuf, "\t$inertia %.2f\r\n", defs.inertia );
	out.WriteText( tmpbuf );
	sprintf( tmpbuf, "\t$damping %.2f\r\n", defs.damping );
	out.WriteText( tmpbuf );
	sprintf( tmpbuf, "\t$rotdamping %.2f\r\n", defs.rotdamping );
	out.WriteText( tmpbuf );
}

char *CStudioPhysics::DumpQC( void )
{
	if ( !m_listCount )
		return NULL;

	CTextBuffer out;
	physdefaults_t defs;

	CalcDefaultProperties( m_pList, m_listCount, defs );

	if ( m_listCount == 1 )
	{
		out.WriteText( "$collisionmodel ragdoll {\r\n\r\n" );
		if ( m_edit.concave )
		{
			out.WriteText( "\t$concave\r\n" );
		}
		DumpModelProperties( out, m_mass, defs );
	}
	else
	{
		int i;

		out.WriteText( "$collisionjoints ragdoll {\r\n\r\n" );
		DumpModelProperties( out, m_mass, defs );

		// write out the root bone
		if ( m_edit.rootName[0] )
		{
			char tmp[128];
			sprintf( tmp, "\t$rootbone \"%s\"\r\n", m_edit.rootName );
			out.WriteText( tmp );
		}

		for ( i = 0; i < m_edit.mergeCount; i++ )
		{
			char tmp[1024];
			if ( m_edit.mergeList[i].parent >= 0 && m_edit.mergeList[i].child >= 0 )
			{
				char const *pParentName = g_pStudioModel->GetStudioHdr()->pBone(m_edit.mergeList[i].parent)->pszName();
				char const *pChildName = g_pStudioModel->GetStudioHdr()->pBone(m_edit.mergeList[i].child)->pszName();
				Q_snprintf( tmp, sizeof(tmp), "\t$jointmerge \"%s\" \"%s\"\r\n", pParentName, pChildName );
				out.WriteText( tmp );
			}
		}
		char tmpbuf[1024];
		for ( i = 0; i < m_listCount; i++ )
		{
			CPhysmesh *pmesh = m_pList + i;
			char jointname[256];
			sprintf( jointname, "\"%s\"", pmesh->m_boneName );
			if ( pmesh->m_solid.massBias != 1.0 )
			{
				sprintf( tmpbuf, "\t$jointmassbias %s %.2f\r\n", jointname, pmesh->m_solid.massBias );
				out.WriteText( tmpbuf );
			}
			if ( pmesh->m_solid.params.inertia != defs.inertia )
			{
				sprintf( tmpbuf, "\t$jointinertia %s %.2f\r\n", jointname, pmesh->m_solid.params.inertia );
				out.WriteText( tmpbuf );
			}
			if ( pmesh->m_solid.params.damping != defs.damping )
			{
				sprintf( tmpbuf, "\t$jointdamping %s %.2f\r\n", jointname, pmesh->m_solid.params.damping );
				out.WriteText( tmpbuf );
			}
			if ( pmesh->m_solid.params.rotdamping != defs.rotdamping )
			{
				sprintf( tmpbuf, "\t$jointrotdamping %s %.2f\r\n", jointname, pmesh->m_solid.params.rotdamping );
				out.WriteText( tmpbuf );
			}

			if ( pmesh->m_constraint.parentIndex >= 0 )
			{
				for ( int j = 0; j < 3; j++ )
				{
					char *pAxis[] = { "x", "y", "z" };
					sprintf( tmpbuf, "\t$jointconstrain %s %s limit %.2f %.2f %.2f\r\n", jointname, pAxis[j], pmesh->m_constraint.axes[j].minRotation, pmesh->m_constraint.axes[j].maxRotation, pmesh->m_constraint.axes[j].torque );
					out.WriteText( tmpbuf );
				}
			}
			if ( i != m_listCount-1 )
			{
				out.WriteText( "\r\n" );
			}
		}
	}

	if ( m_noselfCollisions )
	{
		out.WriteText( "\t$noselfcollisions\r\n" );
	}
	else if ( m_pCollisionPairs )
	{
		collisionpair_t *pPair = m_pCollisionPairs;
		out.WriteText("\r\n");
		while ( pPair )
		{
			out.WriteText( CFmtStr( "\t$jointcollide %s %s\r\n", m_pList[pPair->object0].m_boneName, m_pList[pPair->object1].m_boneName ) );
			pPair = pPair->pNext;
		}
	}
	out.WriteText( "}\r\n" );
	
	// only need the pose for ragdolls
	if ( m_listCount != 1 )
	{
		out.WriteText( "$sequence ragdoll \t\t\"ragdoll_pose\" \t\tFPS 30 \t\tactivity ACT_DIERAGDOLL 1\r\n" );
	}

	out.Terminate();

	if ( out.GetSize() )
	{
		char *pOutput = new char[out.GetSize()];
		memcpy( pOutput, out.GetData(), out.GetSize() );
		return pOutput;
	}

	return NULL;
}

static const char *pMaterialFilename = "scripts/surfaceproperties.txt";

bool LoadPhysicsProperties( void )
{
	// already loaded
	if ( physprop->SurfacePropCount() )
		return false;

	FileHandle_t fp = g_pFileSystem->Open( pMaterialFilename, "rb" );
	if ( fp == FILESYSTEM_INVALID_HANDLE )
		return false;

	int len = g_pFileSystem->Size( fp );

	char *pText = new char[len+1];
	g_pFileSystem->Read( pText, len, fp );
	g_pFileSystem->Close( fp );
	pText[len]=0;

	physprop->ParseSurfaceData( pMaterialFilename, pText );

	delete[] pText;
	return true;
}
