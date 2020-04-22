//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "hlfaceposer.h"
#include <mxtk/mx.h>
#include "expressions.h"
#include "expclass.h"
#include "hlfaceposer.h"
#include "StudioModel.h"
#include "filesystem.h"
#include "FlexPanel.h"
#include "ControlPanel.h"
#include "mxExpressionTray.h"
#include "UtlBuffer.h"
#include "filesystem.h"
#include "ExpressionTool.h"
#include "faceposer_models.h"
#include "mdlviewer.h"
#include "phonemeconverter.h"
#include "ProgressDialog.h"
#include "tier1/fmtstr.h"
#include "tier1/utlstring.h"
#include "tier1/utlvector.h"


#undef ALIGN4
#undef ALIGN16
#define ALIGN4( a ) a = (byte *)((int)((byte *)a + 3) & ~ 3)
#define ALIGN16( a ) a = (byte *)((int)((byte *)a + 15) & ~ 15)

char const *GetGlobalFlexControllerName( int index );
int GetGlobalFlexControllerCount( void );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *classname - 
//-----------------------------------------------------------------------------
CExpClass::CExpClass( const char *classname ) 
{ 
	Q_strncpy( m_szClassName, classname, sizeof( m_szClassName ) ); 
	Q_FileBase( m_szClassName, m_szBaseName, sizeof( m_szBaseName ) );
	m_szFileName[ 0 ] = 0;
	m_bDirty = false;
	m_nSelectedExpression = -1;
	m_bIsPhonemeClass = Q_strstr( classname, "phonemes" ) ? true : false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CExpClass::~CExpClass( void )
{
	m_Expressions.Purge();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *exp - 
//-----------------------------------------------------------------------------
int	CExpClass::FindExpressionIndex( CExpression *exp )
{
	for ( int i = 0 ; i < GetNumExpressions(); i++ )
	{
		CExpression *e = GetExpression( i );
		if ( e == exp )
			return i;
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CExpClass::Save( void )
{
	CStudioHdr *hdr = models->GetActiveStudioModel()->GetStudioHdr();
	if ( !hdr )
	{
		return;
	}

	const char *filename = GetFileName();
	if ( !filename || !filename[ 0 ] )
		return;

	Con_Printf( "Saving changes to %s to file %s\n", GetName(), GetFileName() );

	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );

	int i, j;

	int numflexmaps = 0;
	int flexmap[128]; // maps file local controlls into global controls

	CExpression *expr = NULL;
	// find all used controllers
	int fc = GetGlobalFlexControllerCount();
	for ( j = 0; j < fc; ++j )
	{
		for (i = 0; i < GetNumExpressions(); i++)
		{
			expr = GetExpression( i );
			Assert( expr );

			float *settings = expr->GetSettings();
			float *weights = expr->GetWeights();

			if ( settings[j] != 0 || 
				 weights[j] != 0 )
			{
				flexmap[ numflexmaps++ ] = j;
				break;
			}
		}
	}

	buf.Printf( "$keys" );
	for (j = 0; j < numflexmaps; j++)
	{
		buf.Printf( " %s", GetGlobalFlexControllerName( flexmap[j] ) );
	}
	buf.Printf( "\n" );

	buf.Printf( "$hasweighting\n" );

	for (i = 0; i < GetNumExpressions(); i++)
	{
		expr = GetExpression( i );

		buf.Printf( "\"%s\" ", expr->name );

		// isalpha returns non zero for ents > 256
		if (expr->index <= 'z') 
		{
			buf.Printf( "\"%c\" ", expr->index );
		}
		else
		{
			buf.Printf( "\"0x%04x\" ", expr->index );
		}

		float *settings = expr->GetSettings();
		float *weights = expr->GetWeights();
		Assert( settings );
		Assert( weights );

		for (j = 0; j < numflexmaps; j++)
		{
			buf.Printf( "%.3f %.3f ", settings[flexmap[j]], weights[flexmap[j]] );
		}

		if ( Q_strstr( expr->name, "Right Side Smile" ) )
		{
			Con_Printf( "wrote %s with checksum %s\n",
				expr->name, expr->GetBitmapCheckSum() );
		}

		buf.Printf( "\"%s\"\n", expr->description );
	}

	char relative[ 512 ];
	filesystem->FullPathToRelativePath( filename, relative, sizeof( relative ) );
	
	MakeFileWriteable( relative );
	FileHandle_t fh = filesystem->Open( relative, "wt" );
	if ( !fh )
	{
		Con_ErrorPrintf( "Unable to write to %s (read-only?)\n", relative );
		return;
	}
	else
	{
		filesystem->Write( buf.Base(), buf.TellPut(), fh );
		filesystem->Close(fh);
	}

	SetDirty( false );

	for (i = 0; i < GetNumExpressions(); i++)
	{
		expr = GetExpression( i );
		if ( expr )
		{
			expr->ResetUndo();
			expr->SetDirty( false );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CExpClass::Export( void )
{
	char vfefilename[ 512 ];
	Q_StripExtension( GetFileName(), vfefilename, sizeof( vfefilename ) );
	Q_DefaultExtension( vfefilename, ".vfe", sizeof( vfefilename ) );

	Con_Printf( "Exporting %s to %s\n", GetName(), vfefilename );

	int i, j;

	int numflexmaps = 0;
	int flexmap[128]; // maps file local controlls into global controls
	CExpression *expr = NULL;

	// find all used controllers
	int fc_count = GetGlobalFlexControllerCount();

	for (j = 0; j < fc_count; j++)
	{
		int k = j;

		for (i = 0; i < GetNumExpressions(); i++)
		{
			expr = GetExpression( i );
			Assert( expr );

			float *settings = expr->GetSettings();
			float *weights = expr->GetWeights();
			Assert( settings );
			Assert( weights );

			if ( settings[k] != 0 || weights[k] != 0 )
			{
				flexmap[numflexmaps++] = k;
				break;
			}
		}
	}

	byte *pData = (byte *)calloc( 1024 * 1024, 1 );
	byte *pDataStart = pData;

	flexsettinghdr_t *fhdr = (flexsettinghdr_t *)pData;
	
	fhdr->id = ('V' << 16) + ('F' << 8) + ('E');
	fhdr->version = 0;
	V_strncpy( fhdr->name, vfefilename, sizeof( fhdr->name ) );

	// allocate room for header
	pData += sizeof( flexsettinghdr_t );
	ALIGN4( pData );

	// store flex settings
	flexsetting_t *pSetting = (flexsetting_t *)pData;
	fhdr->numflexsettings = GetNumExpressions();
	fhdr->flexsettingindex = pData - pDataStart;
	pData += sizeof( flexsetting_t ) * fhdr->numflexsettings;
	ALIGN4( pData );
	for (i = 0; i < fhdr->numflexsettings; i++)
	{
		expr = GetExpression( i );
		Assert( expr );

		pSetting[i].index = expr->index;
		pSetting[i].settingindex = pData - (byte *)(&pSetting[i]);

		flexweight_t *pFlexWeights = (flexweight_t *)pData;

		float *settings = expr->GetSettings();
		float *weights = expr->GetWeights();
		Assert( settings );
		Assert( weights );

		for (j = 0; j < numflexmaps; j++)
		{
			if (settings[flexmap[j]] != 0 || weights[flexmap[j]] != 0)
			{
				pSetting[i].numsettings++;
				pFlexWeights->key = j;
				pFlexWeights->weight = settings[flexmap[j]];
				pFlexWeights->influence = weights[flexmap[j]];
				pFlexWeights++;
			}
			pData = (byte *)pFlexWeights;
			ALIGN4( pData );
		}
	}

	// store indexed table
	int numindexes = 1;
	for (i = 0; i < fhdr->numflexsettings; i++)
	{
		if (pSetting[i].index >= numindexes)
			numindexes = pSetting[i].index + 1;
	}

	int *pIndex = (int *)pData;
	fhdr->numindexes = numindexes;
	fhdr->indexindex = pData - pDataStart;
	pData += sizeof( int ) * numindexes;
	ALIGN4( pData );
	for (i = 0; i < numindexes; i++)
	{
		pIndex[i] = -1;
	}
	for (i = 0; i < fhdr->numflexsettings; i++)
	{
		pIndex[pSetting[i].index] = i;
	}

	// store flex setting names
	for (i = 0; i < fhdr->numflexsettings; i++)
	{
		expr = GetExpression( i );
		pSetting[i].nameindex = pData - (byte *)(&pSetting[i]);
		strcpy( (char *)pData,  expr->name );
		pData += strlen( expr->name ) + 1;
	}
	ALIGN4( pData );

	// store key names
	char **pKeynames = (char **)pData;
	fhdr->numkeys = numflexmaps;
	fhdr->keynameindex = pData - pDataStart;
	pData += sizeof( char *) * numflexmaps;
	
	for (i = 0; i < numflexmaps; i++)
	{
		pKeynames[i] = (char *)(pData - pDataStart);
		strcpy( (char *)pData,  GetGlobalFlexControllerName( flexmap[i] ) );
		pData += strlen( GetGlobalFlexControllerName( flexmap[i] ) ) + 1;
	}
	ALIGN4( pData );

	// allocate room for remapping
	int *keymapping = (int *)pData;
	fhdr->keymappingindex = pData - pDataStart;
	pData += sizeof( int ) * numflexmaps;
	for (i = 0; i < numflexmaps; i++)
	{
		keymapping[i] = -1;
	}
	ALIGN4( pData );

	fhdr->length = pData - pDataStart;

	char relative[ 512 ];
	filesystem->FullPathToRelativePath( vfefilename, relative, sizeof( relative ) );
	
	MakeFileWriteable( relative );
	FileHandle_t fh = filesystem->Open( relative, "wb" );
	if ( !fh )
	{
		Con_ErrorPrintf( "Unable to write to %s (read-only?)\n", relative );
		return;
	}
	else
	{
		filesystem->Write( pDataStart, fhdr->length, fh );
		filesystem->Close(fh);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *CExpClass::GetBaseName( void ) const
{
	return m_szBaseName;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *CExpClass::GetName( void ) const
{
	return m_szClassName;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *CExpClass::GetFileName( void ) const
{
	return m_szFileName;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *filename - 
//-----------------------------------------------------------------------------
void CExpClass::SetFileName( const char *filename )
{
	strcpy( m_szFileName, filename );
}

bool IsUsingPerPlayerExpressions();

void CExpClass::ReloadBitmaps( void )
{
	bool bUsingPerPlayerOverrides = IsUsingPerPlayerExpressions();

	int c = models->Count();
	for ( int model = 0; model < MAX_FP_MODELS; model++ )
	{
		// Only reload bitmaps for current model index
		if ( bUsingPerPlayerOverrides && model != models->GetActiveModelIndex() )
			continue;

		models->ForceActiveModelIndex( model );

		for ( int i = 0 ; i < GetNumExpressions(); i++ )
		{
			CExpression *e = GetExpression( i );
			if ( !e )
				continue;

			if ( e->m_Bitmap[ model ].valid )
			{
				DeleteObject( e->m_Bitmap[ model ].image );
				e->m_Bitmap[ model ].valid = false;
			}

			if ( model >= c )
				continue;

			if ( !LoadBitmapFromFile( e->GetBitmapFilename( model ), e->m_Bitmap[ model ] ) )
			{
				e->CreateNewBitmap( model );
			}
		}
	}

	models->UnForceActiveModelIndex();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//			*description - 
//			*flexsettings - 
//			selectnewitem - 
// Output : CExpression
//-----------------------------------------------------------------------------
CExpression *CExpClass::AddExpression( const char *name, const char *description, float *flexsettings, float *flexweights, bool selectnewitem, bool bDirtyClass )
{
	CStudioHdr *hdr = models->GetActiveStudioModel()->GetStudioHdr();
	if ( !hdr )
		return NULL;

	CExpression *exp = FindExpression( name );
	if ( exp )
	{
		Con_ErrorPrintf( "Can't create, an expression with the name '%s' already exists.\n", name );
		return NULL;
	}

	// Add to end of list
	int idx = m_Expressions.AddToTail();

	exp = &m_Expressions[ idx ];

	float *settings = exp->GetSettings();
	float *weights = exp->GetWeights();
	Assert( settings );
	Assert( weights );

	exp->SetExpressionClass( GetName() );
	strcpy( exp->name, name );
	strcpy( exp->description, description );
	memcpy( settings, flexsettings, GLOBAL_STUDIO_FLEX_CONTROL_COUNT * sizeof( float ) );
	memcpy( weights, flexweights, GLOBAL_STUDIO_FLEX_CONTROL_COUNT * sizeof( float ) );
	exp->index = '_';

	if ( IsPhonemeClass() )
	{
		exp->index = TextToPhoneme( name );
	}

	exp->m_Bitmap[ models->GetActiveModelIndex() ].valid = false;
	if ( !LoadBitmapFromFile( exp->GetBitmapFilename( models->GetActiveModelIndex() ), exp->m_Bitmap[ models->GetActiveModelIndex() ] ) )
	{
		exp->CreateNewBitmap( models->GetActiveModelIndex() );
	}
	
	if ( selectnewitem )
	{
		SelectExpression( idx );
	}

	if ( bDirtyClass )
	{
		SetDirty( true );
	}

	return exp;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
// Output : CExpression
//-----------------------------------------------------------------------------
CExpression *CExpClass::FindExpression( const char *name )
{
	for ( int i = 0 ; i < m_Expressions.Size(); i++ )
	{
		CExpression *exp = &m_Expressions[ i ];
		if ( !stricmp( exp->name, name ) )
		{
			return exp;
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
void CExpClass::DeleteExpression( const char *name )
{

	for ( int i = 0 ; i < m_Expressions.Size(); i++ )
	{
		CExpression *exp = &m_Expressions[ i ];
		if ( !stricmp( exp->name, name ) )
		{
			SetDirty( true );

			m_Expressions.Remove( i );
			return;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CExpClass::GetNumExpressions( void )
{
	return m_Expressions.Size();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : num - 
// Output : CExpression
//-----------------------------------------------------------------------------
CExpression *CExpClass::GetExpression( int num )
{
	if ( num < 0 || num >= m_Expressions.Size() )
	{
		return NULL;
	}

	CExpression *exp = &m_Expressions[ num ];
	return exp;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CExpClass::GetDirty( void )
{
	return m_bDirty;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dirty - 
//-----------------------------------------------------------------------------
void CExpClass::SetDirty( bool dirty )
{
	m_bDirty = dirty;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CExpClass::GetIndex( void )
{
	for ( int i = 0; i < expressions->GetNumClasses(); i++ )
	{
		CExpClass *cl = expressions->GetClass( i );
		if ( cl == this )
			return i;
	}
	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : num - 
//-----------------------------------------------------------------------------
void CExpClass::SelectExpression( int num, bool deselect )
{
	m_nSelectedExpression = num;

	g_pFlexPanel->setExpression( num );
	g_pExpressionTrayTool->Select( num, deselect );
	g_pExpressionTrayTool->redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CExpClass::GetSelectedExpression( void )
{
	return m_nSelectedExpression;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CExpClass::DeselectExpression( void )
{
	m_nSelectedExpression = -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : exp1 - 
//			exp2 - 
//-----------------------------------------------------------------------------
void CExpClass::SwapExpressionOrder( int exp1, int exp2 )
{
	CExpression temp1 = m_Expressions[ exp1 ];
	CExpression temp2 = m_Expressions[ exp2 ];

	m_Expressions.Remove( exp1 );
	m_Expressions.InsertBefore( exp1, temp2 );
	m_Expressions.Remove( exp2 );
	m_Expressions.InsertBefore( exp2, temp1 );
}

void CExpClass::BuildValidChecksums( CUtlRBTree< CRC32_t > &tree )
{
	for ( int i = 0; i < m_Expressions.Size(); i++ )
	{
		CExpression *exp = &m_Expressions[ i ];
		if ( !exp )
			continue;

		CRC32_t crc = exp->GetBitmapCRC();
		tree.Insert( crc );
	}
}

//-----------------------------------------------------------------------------
// Purpose: After a class is loaded, check the class directory and delete any bmp files that aren't
//  still referenced
//-----------------------------------------------------------------------------
void CExpClass::CheckBitmapConsistency( void )
{
	char path[ 512 ];

	Q_snprintf( path, sizeof( path ), "expressions/%s/%s/*.bmp", models->GetActiveModelName(), GetBaseName() );
	Q_FixSlashes( path );
	Q_strlower( path );

	g_pProgressDialog->Start( CFmtStr( "%s / %s - Reconcile Expression Thumbnails", models->GetActiveModelName(), GetBaseName() ), "", true );

	CUtlVector< CUtlString > workList;

	FileFindHandle_t hFindFile;
	char const *fn = filesystem->FindFirstEx( path, "MOD", &hFindFile );
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
			char kill[ 512 ];
			Q_snprintf( kill, sizeof( kill ), "expressions/%s/%s/%s", models->GetActiveModelName(), GetBaseName(), fn );
			Q_FixSlashes( kill );
			Q_strlower( kill );

			// Delete it
			Con_ErrorPrintf( "Removing unused bitmap file '%s'\n", kill );

			filesystem->RemoveFile( kill, "MOD" );
		}

		if ( g_pProgressDialog->IsCancelled() )
		{
			Msg( "Cancelled\n" );
			break;
		}
	}

	g_pProgressDialog->Finish();
}

//-----------------------------------------------------------------------------
// Purpose: Does this class have expression indices based on phoneme lookups
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CExpClass::IsPhonemeClass( void ) const
{
	return m_bIsPhonemeClass;
}