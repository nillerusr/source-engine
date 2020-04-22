//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "stdafx.h"
#include "hammer_mathlib.h"
#include "MainFrm.h"
#include "ObjectProperties.h" 
#include "Box3D.h"
#include "BSPFile.h"
#include "const.h"
#include "MapDefs.h"		// dvs: For COORD_NOTINIT
#include "MapEntity.h"
#include "MapInstance.h"
#include "Manifest.h"
#include "Render2D.h"
#include "Render3D.h"
#include "MapView2D.h"
#include "MapView3D.h"
#include "hammer.h"
#include "Texture.h"
#include "TextureSystem.h"
#include "materialsystem/imesh.h"
#include "Material.h"
#include "Options.h"
#include "camera.h"
#include "MapWorld.h"
#include "mapview.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


IMPLEMENT_MAPCLASS(CMapInstance)

char CMapInstance::m_InstancePath[ MAX_PATH ] = "";


//-----------------------------------------------------------------------------
// Purpose: Factory function. Used for creating a CMapInstance.
// Input  : pHelperInfo - Pointer to helper info class which gives us information
//			about how to create the class.
//			pParent - the owning entity ( func_instance )
// Output : Returns a pointer to the class, NULL if an error occurs.
//-----------------------------------------------------------------------------
CMapClass *CMapInstance::Create( CHelperInfo *pHelperInfo, CMapEntity *pParent )
{
	char		FileName[ MAX_PATH ];
	const char	*FileNameKey = pParent->GetKeyValue( "file" );
	CMapDoc		*pDoc = CMapDoc::GetActiveMapDoc();

	if ( FileNameKey )
	{
		V_strcpy_safe( FileName, pParent->GetKeyValue( "file" ) );
	}
	else
	{
		FileName[ 0 ] = 0;
	}

	CMapInstance *pInstance = new CMapInstance( pDoc->GetPathName(), FileName );

	return pInstance;
}


//-----------------------------------------------------------------------------
// Purpose: this function will set a secondary lookup path for instances.
// Input  : pszInstancePath - the secondary lookup path
//-----------------------------------------------------------------------------
void CMapInstance::SetInstancePath( const char *pszInstancePath )
{
	strcpy( m_InstancePath, pszInstancePath );
	V_strlower( m_InstancePath );
	V_FixSlashes( m_InstancePath );
}


//-----------------------------------------------------------------------------
// Purpose: This function will attempt to find a full path given the base and relative names.
// Input  : pszBaseFileName - the base file that referenced this instance
//			pszInstanceFileName - the relative file name of this instance
// Output : Returns true if it was able to locate the file
//			pszOutFileName - the full path to the file name if located
//-----------------------------------------------------------------------------
bool CMapInstance::DeterminePath( const char *pszBaseFileName, const char *pszInstanceFileName, char *pszOutFileName )
{
	char		szInstanceFileNameFixed[ MAX_PATH ];
	const char *pszMapPath = "\\maps\\";

	strcpy( szInstanceFileNameFixed, pszInstanceFileName );
	V_SetExtension( szInstanceFileNameFixed, ".vmf", sizeof( szInstanceFileNameFixed ) );
	V_FixSlashes( szInstanceFileNameFixed );

	// first, try to find a relative location based upon the Base file name
	strcpy( pszOutFileName, pszBaseFileName );
	V_StripFilename( pszOutFileName );

	strcat( pszOutFileName, "\\" );
	strcat( pszOutFileName, szInstanceFileNameFixed );

	if ( g_pFullFileSystem->FileExists( pszOutFileName ) )
	{
		return true;
	}

	// second, try to find the master 'maps' directory and make it relative from that
	strcpy( pszOutFileName, pszBaseFileName );
	V_StripFilename( pszOutFileName );
	V_RemoveDotSlashes( pszOutFileName );
	V_FixDoubleSlashes( pszOutFileName );
	V_strlower( pszOutFileName );
	strcat( pszOutFileName, "\\" );

	char *pos = strstr( pszOutFileName, pszMapPath );
	if ( pos )
	{
		pos += strlen( pszMapPath );
		*pos = 0;
		strcat( pszOutFileName, szInstanceFileNameFixed );

		if ( g_pFullFileSystem->FileExists( pszOutFileName ) )
		{
			return true;
		}
	}

	if ( m_InstancePath[ 0 ] != 0 )
	{
		sprintf( szInstanceFileNameFixed, "%s%s", m_InstancePath, pszInstanceFileName );

		if ( g_pFullFileSystem->FileExists( szInstanceFileNameFixed, "GAME" ) )
		{
			char FullPath[ MAX_PATH ];
			g_pFullFileSystem->RelativePathToFullPath( szInstanceFileNameFixed, "GAME", FullPath, sizeof( FullPath ) );
			strcpy( pszOutFileName, FullPath );

			return true;
		}
	}

	pszOutFileName[ 0 ] = 0;

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CMapInstance::CMapInstance( void )
{
	Initialize();
}


//-----------------------------------------------------------------------------
// Purpose: Constructor.
// Input  : pszBaseFileName - the root path of where the instance will be loaded from.
//			pszInstanceFileName - the relative name of the instance to be loaded.
// Output : 
//-----------------------------------------------------------------------------
CMapInstance::CMapInstance( const char *pszBaseFileName, const char *pszInstanceFileName )
{
	Initialize();

	if ( pszInstanceFileName[ 0 ] && DeterminePath( pszBaseFileName, pszInstanceFileName, m_FileName ) )
	{
		bool	bSaveVisible = CHammer::IsNewDocumentVisible();
		CMapDoc	*activeDoc = CMapDoc::GetActiveMapDoc();

		CHammer::SetIsNewDocumentVisible( false );
		m_pInstancedMap = ( CMapDoc * )APP()->OpenDocumentFile( m_FileName );
		if ( m_pInstancedMap )
		{
			m_pInstancedMap->AddReference();
			m_pInstancedMap->Update();
		}

		CMapDoc::SetActiveMapDoc( activeDoc );
		CHammer::SetIsNewDocumentVisible( bSaveVisible );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CMapInstance::~CMapInstance(void)
{
	if ( m_pInstancedMap )
	{
		m_pInstancedMap->RemoveReference();
		m_pInstancedMap = NULL;
	}
}


GDIV_TYPE CMapInstance::GetFieldType( const char *pInstanceValue )
{
	CMapEntityList	entityList;
	GDinputvariable	TempVar;

	m_pInstancedMap->FindEntitiesByClassName( entityList, "func_instance_parms", false );
	if ( entityList.Count() != 1 )
	{
		return ivBadType;
	}

	CMapEntity *pInstanceParmsEntity = entityList.Element( 0 );

	const char *InstancePos = strchr( pInstanceValue, ' ' );
	if ( InstancePos == NULL )
	{
		return ivBadType;
	}
	int		len = InstancePos - pInstanceValue;

	for ( int i = pInstanceParmsEntity->GetFirstKeyValue(); i != pInstanceParmsEntity->GetInvalidKeyValue(); i = pInstanceParmsEntity->GetNextKeyValue( i ) )
	{
		LPCTSTR	pKey = pInstanceParmsEntity->GetKey( i );
		LPCTSTR	pValue = pInstanceParmsEntity->GetKeyValue( i );

		if ( strnicmp( pKey, "parm", strlen( "parm" ) ) == 0 )
		{
			const char *InstanceParmsPos = strchr( pValue, ' ' );
			if ( InstanceParmsPos == NULL )
			{
				continue;
			}

			if ( strnicmp( pInstanceValue, pValue, len ) == 0 )
			{
				return TempVar.GetTypeFromToken( InstanceParmsPos + 1 );
			}
		}
	}

	return ivBadType;
}


void CMapInstance::FindTargetNames( CUtlVector< const char * > &Names )
{
	CMapEntity *pEntity = dynamic_cast< CMapEntity * >( GetParent() );

	for ( int j = pEntity->GetFirstKeyValue(); j != pEntity->GetInvalidKeyValue(); j = pEntity->GetNextKeyValue( j ) )
	{
		LPCTSTR	pInstanceKey = pEntity->GetKey( j );
		LPCTSTR	pInstanceValue = pEntity->GetKeyValue( j );
		if ( strnicmp( pInstanceKey, "replace", strlen( "replace" ) ) == 0 )
		{
			GDIV_TYPE	FieldType = GetFieldType( pInstanceValue );

			if ( FieldType == ivTargetDest ||
				FieldType == ivTargetNameOrClass ||
				FieldType == ivTargetSrc )
			{
				const char *pszInstancePos = strchr( pInstanceValue, ' ' );

				if ( pszInstancePos )
				{
					pszInstancePos++;

					char	*temp = new char[ strlen( pszInstancePos ) + 1 ];
					strcpy( temp, pszInstancePos );

					Names.AddToTail( temp );
				}
			}
		}
	}

}


void CMapInstance::ReplaceTargetname( const char *szOldName, const char *szNewName )
{
	BaseClass::ReplaceTargetname( szOldName, szNewName );

	CMapEntity *pEntity = dynamic_cast< CMapEntity * >( GetParent() );

	for ( int j = pEntity->GetFirstKeyValue(); j != pEntity->GetInvalidKeyValue(); j = pEntity->GetNextKeyValue( j ) )
	{
		LPCTSTR	pInstanceKey = pEntity->GetKey( j );
		LPCTSTR	pInstanceValue = pEntity->GetKeyValue( j );
		if ( strnicmp( pInstanceKey, "replace", strlen( "replace" ) ) == 0 )
		{
			const char *InstancePos = strchr( pInstanceValue, ' ' );
			if ( InstancePos == NULL )
			{
				continue;
			}

			int	nLen = InstancePos - pInstanceValue;

			if ( strcmp( szOldName, InstancePos + 1 ) == 0 )
			{
				nLen++;

				char	*pszResult = ( char * )stackalloc( nLen + strlen( szNewName ) + 1 );

				strncpy( pszResult, pInstanceValue, nLen );
				strcpy( &pszResult[ nLen ], szNewName );

				pEntity->SetKeyValue( pInstanceKey, pszResult );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: This function is called when the owning entity's Key/Value pairs have
//			been updated.  This will attempt to load a new instance if the map has
//			been changed.
// Input  : none.
// Output : none.
//-----------------------------------------------------------------------------
bool CMapInstance::OnApply( void )
{
	CString	MapFileName;
	char	FileName[ MAX_PATH ];
	CMapDoc	*activeDoc = CMapDoc::GetActiveMapDoc();

	MapFileName = activeDoc->GetPathName();

	CMapEntity *ent = dynamic_cast< CMapEntity * >( GetParent() );
	if ( m_pInstancedMap )
	{
		m_pInstancedMap->RemoveReference();
	}
	if ( ent && ent->GetKeyValue( "file" ) )
	{
		DeterminePath( MapFileName, ent->GetKeyValue( "file" ), FileName );
		if ( strcmpi( FileName, m_FileName ) != 0 ) 
		{
			bool	bSaveVisible = CHammer::IsNewDocumentVisible();

			CHammer::SetIsNewDocumentVisible( false );
			strcpy( m_FileName, FileName );
			m_pInstancedMap = ( CMapDoc * )APP()->OpenDocumentFile( m_FileName );

			CHammer::SetIsNewDocumentVisible( bSaveVisible );
		}
	}
	else
	{
		m_pInstancedMap = NULL;
	}

	if ( m_pInstancedMap == NULL )
	{
		m_FileName[ 0 ] = 0;
	}
	else
	{
		m_pInstancedMap->AddReference();
		m_pInstancedMap->Update();
	}

	GetMainWnd()->pObjectProperties->MarkDataDirty();

	// loading this instance will bring it forward in the MDI - we want to show the original map though
	CMapDoc::ActivateMapDoc( activeDoc );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Calculates our bounding box based on instance map's dimensions.
// Input  : bFullUpdate - Whether we should recalculate our children's bounds.
// Output : none.
//-----------------------------------------------------------------------------
void CMapInstance::CalcBounds(BOOL bFullUpdate)
{
	CMapClass::CalcBounds(bFullUpdate);
	
	//
	// Build our bounds for frustum culling in the 3D view.
	//
	if ( m_pInstancedMap && GetParent() && m_pInstancedMap->GetMapWorld() )
	{
		Vector		vecMins, vecMaxs, vecExpandedMins, vecExpandedMaxs;
		matrix3x4_t	Instance3x4Matrix;

		CMapClass *pParent = GetParent();
		pParent->GetOrigin( m_Origin );
		AngleMatrix( m_Angles, m_Origin, Instance3x4Matrix );
		m_pInstancedMap->GetMapWorld()->CalcBounds( true );

#if 0
		m_pInstancedMap->GetMapWorld()->GetCullBox( vecMins, vecMaxs );
		TransformAABB( Instance3x4Matrix, vecMins, vecMaxs, vecExpandedMins, vecExpandedMaxs );
		m_CullBox.UpdateBounds( vecExpandedMins, vecExpandedMaxs );
#endif

		m_pInstancedMap->GetMapWorld()->GetBoundingBox( vecMins, vecMaxs );
		TransformAABB( Instance3x4Matrix, vecMins, vecMaxs, vecExpandedMins, vecExpandedMaxs );
		m_CullBox.UpdateBounds( vecExpandedMins, vecExpandedMaxs );
		m_BoundingBox.UpdateBounds( vecExpandedMins, vecExpandedMaxs );
		m_Render2DBox.UpdateBounds( vecExpandedMins, vecExpandedMaxs );
	}
	else
	{
		Vector		vecExpandedMins, vecExpandedMaxs;

		vecExpandedMins.Init( -32.0f, -32.0f, -32.0f );
		vecExpandedMins += m_Origin;
		vecExpandedMaxs.Init( 32.0f, 32.0f, 32.0f );
		vecExpandedMaxs += m_Origin;

		m_CullBox.UpdateBounds( vecExpandedMins, vecExpandedMaxs );
		m_BoundingBox.UpdateBounds( vecExpandedMins, vecExpandedMaxs );
		m_Render2DBox.UpdateBounds( vecExpandedMins, vecExpandedMaxs );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Will calculate the bounding box of the instance as a child has changed
// Input  : pChild - Pointer to the object that changed.
//-----------------------------------------------------------------------------
void CMapInstance::UpdateChild(CMapClass *pChild)
{
	CalcBounds( TRUE );
}


//-----------------------------------------------------------------------------
// Purpose: this function will attempt to find a child inside of the instance.  If the bool and matrix
//			are supplied, the localized matrix will be built.
// Input  : key - the key field to lookup
//			value - the value to find
// Output : returns the entity found 
//			bIsInInstance - optional parameter to indicate if the found entity is inside of an instance
//			InstanceMatrix - optional parameter to set the localized matrix of the instance stack
//-----------------------------------------------------------------------------
CMapEntity *CMapInstance::FindChildByKeyValue( const char* key, const char* value, bool *bIsInInstance, VMatrix *InstanceMatrix )
{
	if ( m_pInstancedMap && bIsInInstance )
	{
		CMapEntity *result = m_pInstancedMap->GetMapWorld()->FindChildByKeyValue( key, value );
		if ( result )
		{
			if ( ( *bIsInInstance ) == false )
			{
				*bIsInInstance = true;
				if ( InstanceMatrix )
				{
					InstanceMatrix->Identity();
				}
			}

			if ( InstanceMatrix )
			{
				VMatrix	LocalInstanceMatrix, Result;

				LocalInstanceMatrix.SetupMatrixOrgAngles( m_Origin, m_Angles );
				Result = ( *InstanceMatrix ) * LocalInstanceMatrix;
				*InstanceMatrix = Result;
			}
		}

		return result;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: this function is called for when an instance has moved
//-----------------------------------------------------------------------------
void CMapInstance::InstanceMoved( void )
{
	if ( m_pInstancedMap )
	{
		m_pInstancedMap->InstanceMoved();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns a copy of this object.
// Output : Pointer to the new object.
//-----------------------------------------------------------------------------
CMapClass *CMapInstance::Copy(bool bUpdateDependencies)
{
	CMapInstance *pCopy = new CMapInstance;

	if (pCopy != NULL)
	{
		pCopy->CopyFrom(this, bUpdateDependencies);
	}

	return(pCopy);
}


//-----------------------------------------------------------------------------
// Purpose: Turns this into a duplicate of the given object.
// Input  : pObject - Pointer to the object to copy from.
// Output : Returns a pointer to this object.
//-----------------------------------------------------------------------------
CMapClass *CMapInstance::CopyFrom(CMapClass *pObject, bool bUpdateDependencies)
{
	CMapInstance *pFrom = dynamic_cast<CMapInstance *>(pObject);
	Assert(pObject != NULL);

	if (pObject != NULL)
	{
		CMapClass::CopyFrom(pObject, bUpdateDependencies);

		m_Angles = pFrom->m_Angles;
		strcpy( m_FileName, pFrom->m_FileName );
		m_pInstancedMap = pFrom->m_pInstancedMap;
		if ( m_pInstancedMap )
		{
			m_pInstancedMap->AddReference();
			m_pInstancedMap->Update();
		}
	}

	return(this);
}


//-----------------------------------------------------------------------------
// Purpose: Set's the map instance's angles
// Input  : Angles - the angles to set to
//-----------------------------------------------------------------------------
void CMapInstance::GetAngles(QAngle &Angles)
{
	Angles = m_Angles;
}


//-----------------------------------------------------------------------------
// Purpose: Initialized the map instance
//-----------------------------------------------------------------------------
void CMapInstance::Initialize(void)
{
	m_Angles.Init();
	m_pInstancedMap = NULL;
	m_FileName[ 0 ] = 0;
	m_pManifestMap = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Sets the manifest that loaded this instance
// Input  : pManifestMap - the manifest
//-----------------------------------------------------------------------------
void CMapInstance::SetManifest( CManifestMap *pManifestMap )
{
	Initialize();

	m_pManifestMap = pManifestMap;
	m_pInstancedMap = m_pManifestMap->m_Map;
	strcpy( m_FileName, m_pManifestMap->m_AbsoluteMapFileName );
}


//-----------------------------------------------------------------------------
// Purpose: This will render the map instance into the 3d view.
// Input  : pRender - the 3d render
//-----------------------------------------------------------------------------
void CMapInstance::Render3D(CRender3D *pRender)
{
	if ( m_pInstancedMap )
	{
		pRender->RenderInstanceMapClass( this, m_pInstancedMap->GetMapWorld(), m_Origin, m_Angles );
	}
}


//-----------------------------------------------------------------------------
// Purpose: this function will display an instance map window.  it will also set the
//			views to the approx same camera position.
//-----------------------------------------------------------------------------
void CMapInstance::SwitchTo( void )
{
	if ( !m_pInstancedMap )
	{
		return;
	}

	CMapDoc		*pCurrentDoc = CMapDoc::GetActiveMapDoc();

	m_pInstancedMap->ShowWindow( true );

	CMapDoc::ActivateMapDoc( m_pInstancedMap );

	POSITION PositionCurrentView = pCurrentDoc->GetFirstViewPosition();
	POSITION PositionNewView = m_pInstancedMap->GetFirstViewPosition();

	while ( PositionCurrentView && PositionNewView )
	{
		CMapView	*pViewCurrent = dynamic_cast< CMapView * >( pCurrentDoc->GetNextView( PositionCurrentView ) );
		CMapView2D	*pViewCurrent2D = dynamic_cast< CMapView2D * >( pViewCurrent );
		CMapView3D	*pViewCurrent3D = dynamic_cast< CMapView3D * >( pViewCurrent );
		CMapView	*pViewNew = dynamic_cast< CMapView * >( m_pInstancedMap->GetNextView( PositionNewView ) );
		CMapView2D	*pViewNew2D = dynamic_cast< CMapView2D * >( pViewNew );
		CMapView3D	*pViewNew3D = dynamic_cast< CMapView3D * >( pViewNew );

		if ( ( !pViewCurrent2D || !pViewNew2D ) && ( !pViewCurrent3D || !pViewNew3D ) )
		{
			continue;
		}

		Vector			CameraVector;
		CCamera			*CurrentCamera;

		CurrentCamera = pViewCurrent->GetCamera();
		CurrentCamera->GetViewPoint( CameraVector );

		if ( pViewCurrent2D )
		{
			CameraVector = CameraVector - m_Origin;
			pViewNew2D->GetCamera()->SetViewPoint( CameraVector );
			pViewNew2D->GetCamera()->SetZoom( pViewCurrent2D->GetZoom() );
		}
		else
		{
			matrix3x4_t		Camera3x4Matrix, InstanceMatrix, InstanceInverseMatrix;
			matrix3x4_t		ResultMatrix;
			QAngle			InstanceAngles, CameraAngles;

			CameraAngles.Init( CurrentCamera->GetPitch(), CurrentCamera->GetYaw(), CurrentCamera->GetRoll() );

			InstanceAngles = m_Angles;
			InstanceAngles.x = 0;
			InstanceAngles.z = 0;
			AngleMatrix( InstanceAngles, m_Origin, InstanceMatrix );
			MatrixInvert( InstanceMatrix, InstanceInverseMatrix );
			AngleMatrix( CameraAngles, CameraVector, Camera3x4Matrix );

			MatrixMultiply( InstanceInverseMatrix, Camera3x4Matrix, ResultMatrix );
			MatrixPosition( ResultMatrix, CameraVector );

			MatrixMultiply( InstanceMatrix, Camera3x4Matrix, ResultMatrix );
			MatrixAngles( ResultMatrix, CameraAngles );

			pViewNew3D->GetCamera()->SetViewPoint( CameraVector );
			pViewNew3D->GetCamera()->SetPitch( CameraAngles.x );
			pViewNew3D->GetCamera()->SetYaw( CameraAngles.y );
//			pViewNew3D->GetCamera()->SetRoll( CameraAngles.z );			we probably don't want to set this!
		}
		pViewNew->UpdateView( MAPVIEW_OPTIONS_CHANGED );
	}
}


//-----------------------------------------------------------------------------
// Purpose: we do not want to serialize this
// Input  : &File - 
//			bRMF - 
// Output : int
//-----------------------------------------------------------------------------
int CMapInstance::SerializeRMF(std::fstream &File, BOOL bRMF)
{
	return(0);
}


//-----------------------------------------------------------------------------
// Purpose: we do not want to serialize this
// Input  : &File - 
//			bRMF - 
// Output : int
//-----------------------------------------------------------------------------
int CMapInstance::SerializeMAP(std::fstream &File, BOOL bRMF)
{
	return(0);
}


//-----------------------------------------------------------------------------
// Purpose: Canculate angles based upon the transform
// Input  : pTransBox - 
//-----------------------------------------------------------------------------
void CMapInstance::DoTransform(const VMatrix &matrix)
{
	CMapClass *pParent = GetParent();
	pParent->GetOrigin( m_Origin );

//	BaseClass::DoTransform(matrix);

	matrix3x4_t fCurrentMatrix,fMatrixNew;
	AngleMatrix(m_Angles, fCurrentMatrix);
	ConcatTransforms(matrix.As3x4(), fCurrentMatrix, fMatrixNew);
	MatrixAngles(fMatrixNew, m_Angles);

	CMapEntity *pEntity = dynamic_cast< CMapEntity * >( m_pParent );
	if (pEntity != NULL)
	{
		char szValue[ 80 ];
		sprintf( szValue, "%g %g %g", m_Angles[ 0 ], m_Angles[ 1 ], m_Angles[ 2 ] );
		pEntity->NotifyChildKeyChanged( this, "angles", szValue );
	}

	InstanceMoved();
}


//-----------------------------------------------------------------------------
// Purpose: Notifies that this object's parent entity has had a key value change.
// Input  : szKey - The key that changed.
//			szValue - The new value of the key.
//-----------------------------------------------------------------------------
void CMapInstance::OnParentKeyChanged(const char* szKey, const char* szValue)
{
	if (!stricmp(szKey, "angles"))
	{
		sscanf(szValue, "%f %f %f", &m_Angles[PITCH], &m_Angles[YAW], &m_Angles[ROLL]);
		PostUpdate(Notify_Changed);
	}
}


//-----------------------------------------------------------------------------
// Purpose: indicates that we should not render last
// Output : returns false.
//-----------------------------------------------------------------------------
bool CMapInstance::ShouldRenderLast(void)
{
	return( false );
}


//-----------------------------------------------------------------------------
// Purpose: This will render the map instance into the 2d view as well as a
//			bounding box.
// Input  : pRender - the 2d render
//-----------------------------------------------------------------------------
void CMapInstance::Render2D(CRender2D *pRender)
{
	CMapView2D *pView = ( CMapView2D * )pRender->GetView();

	if ( m_pInstancedMap )
	{
		pView->RenderInstance( this, m_pInstancedMap->GetMapWorld(), m_Origin, m_Angles );
	}

	if ( m_pManifestMap )
	{
		return;
	}

	Vector vecMins;
	Vector vecMaxs;
	GetRender2DBox(vecMins, vecMaxs);

	Vector2D pt,pt2;
	pRender->TransformPoint(pt, vecMins);
	pRender->TransformPoint(pt2, vecMaxs);

	if (!IsSelected())
	{
	    pRender->SetDrawColor( r, g, b );
		pRender->SetHandleColor( r, g, b );
	}
	else
	{
	    pRender->SetDrawColor( GetRValue(Options.colors.clrSelection), GetGValue(Options.colors.clrSelection), GetBValue(Options.colors.clrSelection) );
		pRender->SetHandleColor( GetRValue(Options.colors.clrSelection), GetGValue(Options.colors.clrSelection), GetBValue(Options.colors.clrSelection) );
	}

	// Draw the bounding box.
		
	pRender->DrawBox( vecMins, vecMaxs );

	//
	// Draw center handle.
	//

	if ( pRender->IsActiveView() )
	{
		int sizex = abs(pt.x - pt2.x)+1;
		int sizey = abs(pt.y - pt2.y)+1;

		// dont draw handle if object is too small
		if ( sizex > 6 && sizey > 6 )
		{
			pRender->SetHandleStyle( HANDLE_RADIUS, CRender::HANDLE_CROSS );
			pRender->DrawHandle( (vecMins+vecMaxs)/2 );
		}
	}
}


//-----------------------------------------------------------------------------
// Called by entity code to render sprites
//-----------------------------------------------------------------------------
void CMapInstance::RenderLogicalAt(CRender2D *pRender, const Vector2D &vecMins, const Vector2D &vecMaxs )
{
}


//-----------------------------------------------------------------------------
// Purpose: Returns if this instance is editable.  A pure instance is not editable.
//			If it is part of a manifest, it must be the primary map of the manifest
//			in order for it to be editable.
//-----------------------------------------------------------------------------
bool CMapInstance::IsEditable( void )
{
	if ( m_pManifestMap )
	{
		return m_pManifestMap->IsEditable();
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: this function checks to see if the instance is visible.
// Output : returns true if the instance is visible.
//-----------------------------------------------------------------------------
bool CMapInstance::IsInstanceVisible( void )
{
	if ( IsInstance() )
	{
		if ( CMapDoc::GetActiveMapDoc()  && CMapDoc::GetActiveMapDoc()->GetShowInstance() == INSTANCES_HIDE )
		{
			return false;
		}
	}
	else
	{
		if ( GetManifestMap()  && GetManifestMap()->m_bVisible == false )
		{
			return false;
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: this function will recalculate its bounds because map has changed
//-----------------------------------------------------------------------------
void CMapInstance::UpdateInstanceMap( void )
{
	CalcBounds();
}
