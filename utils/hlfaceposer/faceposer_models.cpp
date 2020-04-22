//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "hlfaceposer.h"
#include "StudioModel.h"
#include "faceposer_models.h"
#include "filesystem.h"
#include "ifaceposerworkspace.h"
#include <mxtk/mx.h>
#include "mdlviewer.h"
#include "mxexpressiontray.h"
#include "ControlPanel.h"
#include "checksum_crc.h"
#include "ViewerSettings.h"
#include "matsyswin.h"
#include "KeyValues.h"
#include "utlbuffer.h" 
#include "expression.h"
#include "ProgressDialog.h"
#include "tier1/UtlString.h"
#include "tier1/FmtStr.h"
#include "tier1/KeyValues.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void SetupModelFlexcontrollerLinks( StudioModel *model );

IFaceposerModels::CFacePoserModel::CFacePoserModel( char const *modelfile, StudioModel *model )
{
	m_pModel = model;
	m_szActorName[ 0 ] = 0;
	m_szShortName[ 0 ] = 0;
	strcpy( m_szModelFileName, modelfile );
	Q_FixSlashes( m_szModelFileName );

	CStudioHdr *hdr = model->GetStudioHdr();
	if ( hdr )
	{	
		Q_StripExtension( hdr->pszName(), m_szShortName, sizeof( m_szShortName ) );
	}

	m_bVisibileIn3DView = false;
	m_bFirstBitmapLoad = true;

	LoadBitmaps();
}

IFaceposerModels::CFacePoserModel::~CFacePoserModel()
{
	FreeBitmaps();
}

void IFaceposerModels::CFacePoserModel::LoadBitmaps()
{
	CStudioHdr *hdr = m_pModel ? m_pModel->GetStudioHdr() : NULL;
	if ( hdr )
	{
		for ( int i = 0 ;i < hdr->GetNumSeq(); i++ )
		{
			mxbitmapdata_t *bm = new mxbitmapdata_t();

			AnimBitmap *entry = new AnimBitmap();
			entry->needsload = true;
			entry->bitmap = bm;

			// Need to load bitmap from disk image via crc, etc.
			//Assert( 0 );

			m_AnimationBitmaps.AddToTail( entry );
		}
	}
}

CRC32_t IFaceposerModels::CFacePoserModel::GetBitmapCRC( int sequence )
{
	CStudioHdr *hdr = m_pModel ? m_pModel->GetStudioHdr() : NULL;
	if ( !hdr )
		return (CRC32_t)-1;

	if ( sequence < 0 || sequence >= hdr->GetNumSeq() )
		return (CRC32_t)-1;

	mstudioseqdesc_t &seqdesc = hdr->pSeqdesc( sequence );

	CRC32_t crc;
	CRC32_Init( &crc );

	// For sequences, we'll checsum a bit of data

	CRC32_ProcessBuffer( &crc, (void *)seqdesc.pszLabel(), Q_strlen( seqdesc.pszLabel() ) );
	CRC32_ProcessBuffer( &crc, (void *)seqdesc.pszActivityName(), Q_strlen( seqdesc.pszActivityName() ) );
	CRC32_ProcessBuffer( &crc, (void *)&seqdesc.flags, sizeof( seqdesc.flags ) );
	//CRC32_ProcessBuffer( &crc, (void *)&seqdesc.numevents, sizeof( seqdesc.numevents ) );
	CRC32_ProcessBuffer( &crc, (void *)&seqdesc.numblends, sizeof( seqdesc.numblends ) );
	CRC32_ProcessBuffer( &crc, (void *)seqdesc.groupsize, sizeof( seqdesc.groupsize ) );

	KeyValues *seqKeyValues = new KeyValues("");
	if ( seqKeyValues->LoadFromBuffer( m_pModel->GetFileName( ), m_pModel->GetKeyValueText( sequence ) ) )
	{
		// Yuck, but I need it in a contiguous block of memory... oh well...
		CUtlBuffer buf;
		seqKeyValues->RecursiveSaveToFile( buf, 0 );
		CRC32_ProcessBuffer( &crc, ( void * )buf.Base(), buf.TellPut() );
	}

	seqKeyValues->deleteThis();

	CRC32_Final( &crc );

	return crc;
}

const char *IFaceposerModels::CFacePoserModel::GetBitmapChecksum( int sequence )
{
	CRC32_t crc = GetBitmapCRC( sequence );

	// Create string name out of binary data
	static char filename[ 512 ];

	char hex[ 16 ];
	Q_binarytohex( (byte *)&crc, sizeof( crc ), hex, sizeof( hex ) );

	Q_snprintf( filename, sizeof( filename ), "%s", hex );
	return filename;
}

const char *IFaceposerModels::CFacePoserModel::GetBitmapFilename( int sequence )
{
	char *in, *out;
	static char filename[ 256 ];
	filename[ 0 ] = 0;

	char modelName[512], modelNameTemp[512];
	Q_strncpy( modelNameTemp, GetShortModelName(), sizeof( modelNameTemp ) );

	in = modelNameTemp;
	out = modelName;

	while ( *in )
	{
		if ( V_isalnum( *in ) ||
			*in == '_' || 
			*in == '\\' || 
			*in == '/' ||
			*in == '.' ||
			*in == ':' )
		{
			*out++ = *in;
		}
		in++;
	}
	*out = 0;


	Q_snprintf( filename, sizeof( filename ), "expressions/%s/animation/%s.bmp", modelName, GetBitmapChecksum( sequence ) );

	Q_FixSlashes( filename );
	strlwr( filename );

	CreatePath( filename );
	
	return filename;
}

void IFaceposerModels::CFacePoserModel::FreeBitmaps()
{
	while ( m_AnimationBitmaps.Count() > 0 )
	{
		AnimBitmap *bm = m_AnimationBitmaps[ 0 ];
		delete bm->bitmap;
		delete bm;
		m_AnimationBitmaps.Remove( 0 );
	}
}

void IFaceposerModels::CFacePoserModel::LoadBitmapForSequence( mxbitmapdata_t *bitmap, int sequence )
{
	// See if it exists
	char filename[ 512 ];
	Q_strncpy( filename, GetBitmapFilename( sequence ), sizeof( filename ) );

	if ( !LoadBitmapFromFile( filename, *bitmap ) )
	{
		CreateNewBitmap( filename, sequence, 256, false, NULL, bitmap );
	}
}

static float FindPoseCycle( StudioModel *model, int sequence )
{
	float cycle = 0.0f;
	if ( !model->GetStudioHdr() )
		return cycle;

	KeyValues *seqKeyValues = new KeyValues("");
	if ( seqKeyValues->LoadFromBuffer( model->GetFileName( ), model->GetKeyValueText( sequence ) ) )
	{
		// Do we have a build point section?
		KeyValues *pkvAllFaceposer = seqKeyValues->FindKey("faceposer");
		if ( pkvAllFaceposer )
		{
			int thumbnail_frame = pkvAllFaceposer->GetInt( "thumbnail_frame", 0 );
			if ( thumbnail_frame )
			{
				// Convert frame to cycle if we have valid data
				int maxFrame = model->GetNumFrames( sequence ) - 1;

				if ( maxFrame > 0 )
				{
					cycle = thumbnail_frame / (float)maxFrame;
				}
			}
		}
	}

	seqKeyValues->deleteThis();

	return cycle;
}


void EnableStickySnapshotMode( void )
{
	g_pMatSysWindow->EnableStickySnapshotMode( );
}

void DisableStickySnapshotMode( void )
{
	g_pMatSysWindow->DisableStickySnapshotMode( );
}


void IFaceposerModels::CreateNewBitmap( int modelindex, char const *pchBitmapFilename, int sequence, int nSnapShotSize, bool bZoomInOnFace, CExpression *pExpression, mxbitmapdata_t *bitmap )
{
	CFacePoserModel *m = m_Models[ modelindex ];
	if ( m )
	{
		m->CreateNewBitmap( pchBitmapFilename, sequence, nSnapShotSize, bZoomInOnFace, pExpression, bitmap );
	}
}

void IFaceposerModels::CFacePoserModel::CreateNewBitmap( char const *pchBitmapFilename, int sequence, int nSnapShotSize, bool bZoomInOnFace, CExpression *pExpression, mxbitmapdata_t *bitmap )
{
	MatSysWindow *pWnd = g_pMatSysWindow;
	if ( !pWnd ) 
		return;

	StudioModel *model = m_pModel;
	if ( !model )
		return;

	CStudioHdr *hdr = model->GetStudioHdr();
	if ( !hdr )
		return;
	if ( sequence < 0 || sequence >= hdr->GetNumSeq() )
		return;

	mstudioseqdesc_t &seqdesc = hdr->pSeqdesc( sequence );

	Con_ColorPrintf( FILE_COLOR, "Creating bitmap %s for sequence '%s'\n", pchBitmapFilename, seqdesc.pszLabel() );

	model->ClearOverlaysSequences();
	int iLayer = model->GetNewAnimationLayer();
	model->SetOverlaySequence( iLayer, sequence, 1.0 );
	model->SetOverlayRate( iLayer, FindPoseCycle( model, sequence ), 0.0 );

	for (int i = 0; i < hdr->GetNumPoseParameters(); i++)
	{
		model->SetPoseParameter( i, 0.0 );
	}

	float flexValues[ GLOBAL_STUDIO_FLEX_CONTROL_COUNT ] = { 0 };

	if ( pExpression )
	{
		float *settings = pExpression->GetSettings();
		float *weights = pExpression->GetWeights();

		// Save existing settings from model
		for ( LocalFlexController_t i = LocalFlexController_t(0); i < hdr->numflexcontrollers(); ++i )
		{
			int j = hdr->pFlexcontroller( i )->localToGlobal;
			if ( j == -1 )
				continue;
			flexValues[ i ] = model->GetFlexController( i );
			// Set Value from passed in settings
			model->SetFlexController( i, settings[ j ] * weights[ j ] );
		}
	}

	model->ClearLookTargets( );

	QAngle oldrot, oldLight;
	Vector oldtrans;
	
	VectorCopy( model->m_angles, oldrot );
	VectorCopy( model->m_origin, oldtrans );
	VectorCopy( g_viewerSettings.lightrot, oldLight );

	model->m_angles.Init();
	model->m_origin.Init();
	g_viewerSettings.lightrot.Init();

	g_viewerSettings.lightrot.y = -180;

	bool bSaveGround = g_viewerSettings.showGround;
	g_viewerSettings.showGround = false;

	if ( bZoomInOnFace )
	{
		Vector size;
		VectorSubtract( hdr->hull_max(), hdr->hull_min(), size );

		float eyeheight = hdr->hull_min().z + 0.9 * size.z;
		//	float width = ( size.x + size.y ) / 2.0f;

		model->m_origin.x = size.z * .6f;

		if ( hdr->GetNumAttachments() > 0 )
		{
			for (int i = 0; i < hdr->GetNumAttachments(); i++)
			{
				const mstudioattachment_t &attachment = hdr->pAttachment( i );
				int iBone = hdr->GetAttachmentBone( i );

				if ( Q_stricmp( attachment.pszName(), "eyes" ) )
					continue;

				mstudiobone_t *bone = hdr->pBone( iBone );
				if ( !bone )
					continue;

				matrix3x4_t boneToPose;
				MatrixInvert( bone->poseToBone, boneToPose );

				matrix3x4_t attachmentPoseToLocal;
				ConcatTransforms( boneToPose, attachment.local, attachmentPoseToLocal );

				Vector localSpaceEyePosition;
				VectorITransform( vec3_origin, attachmentPoseToLocal, localSpaceEyePosition );

				// Not sure why this must be negative?
				eyeheight = -localSpaceEyePosition.z + hdr->hull_min().z;
				break;
			}
		}

		KeyValues *seqKeyValues = new KeyValues("");
		if ( seqKeyValues->LoadFromBuffer( model->GetFileName( ), model->GetKeyValueText( sequence ) ) )
		{
			// Do we have a build point section?
			KeyValues *pkvAllFaceposer = seqKeyValues->FindKey("faceposer");
			if ( pkvAllFaceposer )
			{
				float flEyeheight = pkvAllFaceposer->GetFloat( "eye_height", -9999.0f );
				if ( flEyeheight != -9999.0f )
				{
					eyeheight = flEyeheight;
				}
			}
		}

		model->m_origin.z += eyeheight;
	}
	else
	{
		Vector mins, maxs;
		model->ExtractBbox(mins, maxs);
		Vector size;
		VectorSubtract( maxs, mins, size );

		float maxdim = size.x;
		if ( size.y > maxdim )
			maxdim = size.y;
		if ( size.z > maxdim )
			maxdim = size.z;

		float midpoint = mins.z + 0.5 * size.z;

		model->m_origin.x = 3 * maxdim;
		model->m_origin.z += midpoint;
	}

	g_pMatSysWindow->PushSnapshotMode( nSnapShotSize );

	// Snapshots are taken of the back buffer; 
	// we need to render to the back buffer but not move it to the front
	pWnd->SuppressBufferSwap( true );
	pWnd->redraw();
	pWnd->SuppressBufferSwap( false );

	// make it square, assumes w > h
	char fullpath[ 512 ];
	Q_snprintf( fullpath, sizeof( fullpath ), "%s%s", GetGameDirectory(), pchBitmapFilename );
	pWnd->TakeSnapshotRect( fullpath, 0, 0, nSnapShotSize, nSnapShotSize );

	g_pMatSysWindow->PopSnapshotMode( );

	VectorCopy( oldrot, model->m_angles );
	VectorCopy( oldtrans, model->m_origin );
	VectorCopy( oldLight, g_viewerSettings.lightrot );

	g_viewerSettings.showGround = bSaveGround;

	if ( pExpression )
	{
		// Save existing settings from model
		for ( LocalFlexController_t i = LocalFlexController_t(0); i < hdr->numflexcontrollers(); ++i )
		{
			int j = hdr->pFlexcontroller( i )->localToGlobal;
			if ( j == -1 )
				continue;

			model->SetFlexController( i, flexValues[ i ] );
		}
	}

	model->ClearOverlaysSequences();
	
	if ( bitmap->valid )
	{
		DeleteObject( bitmap->image );
		bitmap->image = 0;
		bitmap->valid = false;
	}

	LoadBitmapFromFile( pchBitmapFilename, *bitmap );
}

mxbitmapdata_t *IFaceposerModels::CFacePoserModel::GetBitmapForSequence( int sequence )
{
	static mxbitmapdata_t nullbitmap;
	if ( sequence < 0 || sequence >= m_AnimationBitmaps.Count() )
		return &nullbitmap;

	/*
	if ( m_bFirstBitmapLoad )
	{
		m_bFirstBitmapLoad = false;
		ReconcileAnimationBitmaps();
	}
	*/

	AnimBitmap *slot = m_AnimationBitmaps[ sequence ];
	if ( slot->needsload )
	{
		slot->needsload = false;
		LoadBitmapForSequence( slot->bitmap, sequence );
	}

	return m_AnimationBitmaps[ sequence ]->bitmap;
}

void IFaceposerModels::CFacePoserModel::BuildValidChecksums( CUtlRBTree< CRC32_t > &tree )
{
	StudioModel *model = m_pModel;
	if ( !model )
		return;

	CStudioHdr *hdr = model->GetStudioHdr();
	if ( !hdr )
		return;	

	for ( int i = 0; i < hdr->GetNumSeq(); i++ )
	{
		CRC32_t crc = GetBitmapCRC( i );
		tree.Insert( crc );
	}
}

void IFaceposerModels::CFacePoserModel::ReconcileAnimationBitmaps()
{
	// iterate files in directory and see if each checksum is valid and if not delete the .bmp
	char path[ 512 ];
	Q_snprintf( path, sizeof( path ), "expressions/%s/animation/*.bmp", GetShortModelName() );

	FileFindHandle_t hFindFile;

	char const *fn = filesystem->FindFirstEx( path, "MOD", &hFindFile );

	g_pProgressDialog->Start( CFmtStr( "%s - Reconcile Animation Thumbnails", GetShortModelName() ), "", true );

	CUtlVector< CUtlString > workList;

	if ( fn )
	{
		while ( fn )
		{
			// Don't do anything with directories
			if ( !filesystem->FindIsDirectory( hFindFile ) )
			{
				CUtlString s = fn;
				workList.AddToTail( s );
			}

			fn = filesystem->FindNext( hFindFile );
		}

		filesystem->FindClose( hFindFile );
	}

	CUtlRBTree< CRC32_t > tree( 0, 0, DefLessFunc( CRC32_t ) );
	BuildValidChecksums( tree );

	for ( int i = 0 ; i < workList.Count(); ++i )
	{
		char testname[ 256 ];
		Q_StripExtension( workList[ i ].String(), testname, sizeof( testname ) );

		g_pProgressDialog->UpdateText( "%s", testname );
		g_pProgressDialog->Update( (float)i / (float)workList.Count() );

		CRC32_t check;
		Q_hextobinary( testname, Q_strlen( testname ), (byte *)&check, sizeof( check ) );

		if ( tree.Find( check ) == tree.InvalidIndex() )
		{
			Q_snprintf( testname, sizeof( testname ), "expressions/%s/animation/%s", GetShortModelName(), fn );

			char fullpath[ 512 ];
			filesystem->RelativePathToFullPath( testname, "MOD", fullpath, sizeof( fullpath ) );
			// Delete it
			Con_ErrorPrintf( "Removing unused bitmap file %s\n", 
				fullpath );

			_unlink( fullpath );
		}

		if ( g_pProgressDialog->IsCancelled() )
		{
			Msg( "Cancelled\n" );
			break;
		}
	}

	g_pProgressDialog->Finish();
}

void IFaceposerModels::CFacePoserModel::RecreateAllAnimationBitmaps()
{
	StudioModel *model = m_pModel;
	if ( !model )
		return;

	CStudioHdr *hdr = model->GetStudioHdr();
	if ( !hdr )
		return;	

	g_pProgressDialog->Start( CFmtStr( "%s - Animation Thumbnails", GetShortModelName() ), "", true );

	for ( int i = 0; i < hdr->GetNumSeq(); ++i )
	{
		const mstudioseqdesc_t &seq = hdr->pSeqdesc( i );

		g_pProgressDialog->UpdateText( "%s", seq.pszLabel() );
		g_pProgressDialog->Update( (float)i / (float)hdr->GetNumSeq() );

		RecreateAnimationBitmap( i, false );

		if ( g_pProgressDialog->IsCancelled() )
		{
			Msg( "Cancelling\n" );
			break;
		}
	}

	g_pProgressDialog->Finish();

	ReconcileAnimationBitmaps();
}

void IFaceposerModels::CFacePoserModel::RecreateAnimationBitmap( int sequence, bool reconcile )
{
	if ( sequence < 0 || sequence >= m_AnimationBitmaps.Count() )
	{
		Assert( 0 );
		return;
	}
	AnimBitmap *slot = m_AnimationBitmaps[ sequence ];
	slot->needsload = true;
	if ( slot->bitmap->valid )
	{
		DeleteObject( slot->bitmap->image );
		slot->bitmap->image = 0;
		slot->bitmap->valid = false;
	}

	char filename[ 512 ];
	Q_snprintf( filename, sizeof( filename ), "%s", GetBitmapFilename( sequence ) );

	if ( filesystem->FileExists( filename ) )
	{
		char fullpath[ 512 ];
		filesystem->RelativePathToFullPath( filename, "MOD", fullpath, sizeof( fullpath ) );
		_unlink( fullpath );
	}

	// Force recreation
	GetBitmapForSequence( sequence );

	if ( reconcile )
	{
		ReconcileAnimationBitmaps( );
	}
}

void IFaceposerModels::CFacePoserModel::Release( void )
{
	m_pModel->FreeModel( true );
}

void IFaceposerModels::CFacePoserModel::Restore( void )
{
	StudioModel *save = g_pStudioModel;

	g_pStudioModel = m_pModel;

	if (m_pModel->LoadModel( m_pModel->GetFileName() ) )
	{
		m_pModel->PostLoadModel( m_pModel->GetFileName() );
		m_pModel->SetSequence( m_pModel->LookupSequence( "idle_subtle" ) );
	}
	
	g_pStudioModel = save;

	SetupModelFlexcontrollerLinks( m_pModel );
}


IFaceposerModels::IFaceposerModels()
{
	m_nLastRenderFrame = -1;
	m_nForceModelIndex = -1;
}

IFaceposerModels::~IFaceposerModels()
{
	while ( m_Models.Count() > 0 )
	{
		delete m_Models[ 0 ];
		m_Models.Remove( 0 );
	}
}

IFaceposerModels::CFacePoserModel *IFaceposerModels::GetEntry( int index )
{
	if ( index < 0 || index >= Count() )
		return NULL;

	CFacePoserModel *m = m_Models[ index ];
	if ( !m )
		return NULL;
	return m;
}

int IFaceposerModels::Count( void ) const
{
	return m_Models.Count();
}

char const *IFaceposerModels::GetModelName( int index )
{
	CFacePoserModel *entry = GetEntry( index );
	if ( !entry )
		return "";

	return entry->GetShortModelName();
}

char const *IFaceposerModels::GetModelFileName( int index )
{
	CFacePoserModel *entry = GetEntry( index );
	if ( !entry )
		return "";

	return entry->GetModelFileName();
}

void IFaceposerModels::ForceActiveModelIndex( int index )
{
	m_nForceModelIndex = index;
}

void IFaceposerModels::UnForceActiveModelIndex()
{
	m_nForceModelIndex = -1;
}

int IFaceposerModels::GetActiveModelIndex( void ) const
{
	if ( !g_MDLViewer )
		return 0;

	if ( m_nForceModelIndex != -1 )
		return m_nForceModelIndex;

	return g_MDLViewer->GetActiveModelTab();
}

char const *IFaceposerModels::GetActiveModelName( void )
{
	if ( !g_MDLViewer )
		return NULL;

	return GetModelName( GetActiveModelIndex() );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : StudioModel
//-----------------------------------------------------------------------------
StudioModel *IFaceposerModels::GetActiveStudioModel( void )
{
	StudioModel *mdl = GetStudioModel( GetActiveModelIndex() );
	if ( !mdl )
		return g_pStudioModel;
	return mdl;
}

int IFaceposerModels::FindModelByFilename( char const *filename )
{
	int c = Count();
	for ( int i = 0; i < c; i++ )
	{
		CFacePoserModel *m = GetEntry( i );
		if ( !m )
			continue;

		if ( !stricmp( m->GetModelFileName(), filename ) )
			return i;
	}

	return -1;
}

void SetupModelFlexcontrollerLinks( StudioModel *model );

int IFaceposerModels::LoadModel( char const *filename )
{
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

	int idx = FindModelByFilename( filename );
	if ( idx == -1 && Count() < MAX_FP_MODELS )
	{
		StudioModel *model = new StudioModel();

		StudioModel *save = g_pStudioModel;
		g_pStudioModel = model;
		if ( !model->LoadModel( filename ) )
		{
			delete model;
			g_pStudioModel = save;
			return 0; // ?? ERROR
		}
		g_pStudioModel = save;

		model->SetSequence( model->LookupSequence( "idle_subtle" ) );
		int idx = model->GetSequence();
		model->SetSequence( idx );

		SetupModelFlexcontrollerLinks( model );

		if (!LoadViewerSettings( filename, model ))
		{
			InitViewerSettings( "faceposer" );
		}
		model->ClearOverlaysSequences();


		CFacePoserModel *newEntry = new CFacePoserModel( filename, model );
		
		idx = m_Models.AddToTail( newEntry );

		g_MDLViewer->InitModelTab();
		
		g_MDLViewer->SetActiveModelTab( idx );

		//g_pControlPanel->CenterOnFace();
	}
	return idx;
}

void IFaceposerModels::FreeModel( int index  )
{
	CFacePoserModel *entry = GetEntry( index );
	if ( !entry )
		return;

	StudioModel *m = entry->GetModel();

	SaveViewerSettings( m->GetFileName(), m );

	m->FreeModel( false );
	delete m;

	delete entry;
	m_Models.Remove( index );

	g_MDLViewer->InitModelTab();
}

void IFaceposerModels::CloseAllModels( void )
{
	int c = Count();
	for ( int i = c - 1; i >= 0; i-- )
	{
		FreeModel( i );
	}
}

StudioModel *IFaceposerModels::GetStudioModel( int index )
{
	CFacePoserModel *m = GetEntry( index );
	if ( !m )
		return NULL;

	if ( !m->GetModel() )
		return NULL;

	return m->GetModel();
}

CStudioHdr *IFaceposerModels::GetStudioHeader( int index )
{
	StudioModel *m = GetStudioModel( index );
	if ( !m )
		return NULL;

	CStudioHdr *hdr = m->GetStudioHdr();
	if ( !hdr )
		return NULL;
	return hdr;
}

int IFaceposerModels::GetModelIndexForActor( char const *actorname )
{
	int c = Count();
	for ( int i = 0; i < c; i++ )
	{
		CFacePoserModel *m = GetEntry( i );
		if ( !m )
			continue;

		if ( !stricmp( m->GetActorName(), actorname ) )
			return i;
	}

	return 0;
}

StudioModel *IFaceposerModels::GetModelForActor( char const *actorname )
{
	int c = Count();
	for ( int i = 0; i < c; i++ )
	{
		CFacePoserModel *m = GetEntry( i );
		if ( !m )
			continue;

		if ( !stricmp( m->GetActorName(), actorname ) )
			return m->GetModel();
	}

	return NULL;
}

char const *IFaceposerModels::GetActorNameForModel( int modelindex )
{
	CFacePoserModel *m = GetEntry( modelindex );
	if ( !m )
		return "";
	return m->GetActorName();
}

void IFaceposerModels::SetActorNameForModel( int modelindex, char const *actorname )
{
	CFacePoserModel *m = GetEntry( modelindex );
	if ( !m )
		return;

	m->SetActorName( actorname );
}

void IFaceposerModels::SaveModelList( void )
{
	workspacefiles->StartStoringFiles( IWorkspaceFiles::MODELDATA );
	int c = Count();
	for ( int i = 0; i < c; i++ )
	{
		CFacePoserModel *m = GetEntry( i );
		if ( !m )
			continue;

		workspacefiles->StoreFile( IWorkspaceFiles::MODELDATA, m->GetModelFileName() );
	}
	workspacefiles->FinishStoringFiles( IWorkspaceFiles::MODELDATA );
}

void IFaceposerModels::LoadModelList( void )
{
	int files = workspacefiles->GetNumStoredFiles( IWorkspaceFiles::MODELDATA );
	for ( int i = 0; i < files; i++ )
	{
		char const *filename = workspacefiles->GetStoredFile( IWorkspaceFiles::MODELDATA, i );
		LoadModel( filename );
	}
}

void IFaceposerModels::ReleaseModels( void )
{
	int c = Count();
	for ( int i = 0; i < c; i++ )
	{
		CFacePoserModel *m = GetEntry( i );
		if ( !m )
			continue;

		m->Release();
	}
}

void IFaceposerModels::RestoreModels( void )
{
	int c = Count();
	for ( int i = 0; i < c; i++ )
	{
		CFacePoserModel *m = GetEntry( i );
		if ( !m )
			continue;

		m->Restore();
	}
}


/*
void IFaceposerModels::RefreshModels( void )
{
	int c = Count();
	for ( int i = 0; i < c; i++ )
	{
		CFacePoserModel *m = GetEntry( i );
		if ( !m )
			continue;

		m->Refresh();
	}
}
*/

int IFaceposerModels::CountVisibleModels( void )
{
	int num = 0;
	int c = Count();
	for ( int i = 0; i < c; i++ )
	{
		CFacePoserModel *m = GetEntry( i );
		if ( !m )
			continue;

		if ( m->GetVisibleIn3DView() )
		{
			num++;
		}
	}

	return num;
}

void IFaceposerModels::ShowModelIn3DView( int modelindex, bool show )
{
	CFacePoserModel *m = GetEntry( modelindex );
	if ( !m )
		return;

	m->SetVisibleIn3DView( show );
}

bool IFaceposerModels::IsModelShownIn3DView( int modelindex )
{
	CFacePoserModel *m = GetEntry( modelindex );
	if ( !m )
		return false;

	return m->GetVisibleIn3DView();
}

int IFaceposerModels::GetIndexForStudioModel( StudioModel *model )
{
	int c = Count();
	for ( int i = 0; i < c; i++ )
	{
		CFacePoserModel *m = GetEntry( i );
		if ( !m )
			continue;

		if ( m->GetModel() == model )
			return i;
	}
	return -1;
}

void IFaceposerModels::CheckResetFlexes( void )
{
	int current_render_frame = g_MDLViewer->GetCurrentFrame();
	if ( current_render_frame == m_nLastRenderFrame )
		return;

	m_nLastRenderFrame = current_render_frame;

	// the phoneme editor just adds to the face, so reset the controllers 
	int c = Count();
	for ( int i = 0; i < c; i++ )
	{
		CFacePoserModel *m = GetEntry( i );
		if ( !m )
			continue;

		StudioModel *model = m->GetModel();
		if ( !model )
			continue;

		CStudioHdr *hdr = model->GetStudioHdr();
		if ( !hdr )
			continue;

		for ( LocalFlexController_t i = LocalFlexController_t(0); i < hdr->numflexcontrollers(); i++ )
		{
			model->SetFlexController( i, 0.0f );
		}
	}
}

void IFaceposerModels::ClearOverlaysSequences( void )
{
	int c = Count();
	for ( int i = 0; i < c; i++ )
	{
		CFacePoserModel *m = GetEntry( i );
		if ( !m )
			continue;

		StudioModel *model = m->GetModel();
		if ( !model )
			continue;

		model->ClearOverlaysSequences();
	}
}

mxbitmapdata_t *IFaceposerModels::GetBitmapForSequence( int modelindex, int sequence )
{
	static mxbitmapdata_t nullbitmap;
	CFacePoserModel *m = GetEntry( modelindex );
	if ( !m )
		return &nullbitmap;

	return m->GetBitmapForSequence( sequence );
}

void IFaceposerModels::RecreateAllAnimationBitmaps( int modelindex )
{
	CFacePoserModel *m = GetEntry( modelindex );
	if ( !m )
		return;

	m->RecreateAllAnimationBitmaps();

}

void IFaceposerModels::RecreateAnimationBitmap( int modelindex, int sequence )
{
	CFacePoserModel *m = GetEntry( modelindex );
	if ( !m )
		return;

	m->RecreateAnimationBitmap( sequence, true );
}

int IFaceposerModels::CountActiveSources()
{
	int count = 0;
	
	int c = Count();
	for ( int i = 0; i < c; i++ )
	{
		CFacePoserModel *m = GetEntry( i );
		if ( !m )
			continue;

		StudioModel *model = m->GetModel();
		if ( !model )
			continue;

		count += model->m_mouth.GetNumVoiceSources();
	}

	return count;
}

void IFaceposerModels::ClearModelTargets( bool force /*=false*/ )
{
	int c = Count();
	for ( int i = 0; i < c; i++ )
	{
		CFacePoserModel *m = GetEntry( i );
		if ( !m )
			continue;

		StudioModel *mdl = m->GetModel();
		if ( !mdl )
			continue;

		mdl->ClearLookTargets();
	}
}

void IFaceposerModels::SetSolveHeadTurn( int solve )
{
	int c = Count();
	for ( int i = 0; i < c; i++ )
	{
		CFacePoserModel *m = GetEntry( i );
		if ( !m )
			continue;

		StudioModel *mdl = m->GetModel();
		if ( !mdl )
			continue;

		mdl->SetSolveHeadTurn( solve );
	}
}


static IFaceposerModels g_ModelManager;
IFaceposerModels *models = &g_ModelManager;
