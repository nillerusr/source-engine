//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "hlfaceposer.h"
#include "expressions.h"
#include <mxtk/mx.h>
#include "ControlPanel.h"
#include "StudioModel.h"
#include "expclass.h"
#include "mxExpressionTab.h"
#include "mxExpressionTray.h"
#include "filesystem.h"
#include "faceposer_models.h"
#include "utldict.h"
#include "scriplib.h"
#include "checksum_crc.h"

bool Sys_Error(const char *pMsg, ...);
extern char g_appTitle[];

static CUtlVector< CUtlSymbol > g_GlobalFlexControllers;
static CUtlDict< int, int >		g_GlobalFlexControllerLookup; 

void ChecksumFlexControllers( bool bSpew, char const *name, CRC32_t &crc, const float *settings, const float *weights )
{
	CRC32_Init( &crc );

	// Walk them alphabetically so that load order doesn't matter
	for ( int i = g_GlobalFlexControllerLookup.First() ; 
		i != g_GlobalFlexControllerLookup.InvalidIndex(); 
		i = g_GlobalFlexControllerLookup.Next( i ) )
	{
		int controllerIndex = g_GlobalFlexControllerLookup[ i ];
		char const *pszName = g_GlobalFlexControllerLookup.GetElementName( i );
		
		// Only count active controllers in checksum
		float s = settings[ controllerIndex ];
		float w = weights[ controllerIndex ];

		if ( s == 0.0f && w == 0.0f )
		{
			 continue;
		}

		CRC32_ProcessBuffer( &crc, (void *)pszName, Q_strlen( pszName ) );
		CRC32_ProcessBuffer( &crc, (void *)&s, sizeof( s ) );
		CRC32_ProcessBuffer( &crc, (void *)&w, sizeof( w ) );

		if ( bSpew )
		{
			Msg( "[%d] %s == %f %f\n", controllerIndex, pszName, s, w );
		}
	}

	CRC32_Final( &crc );

	if ( bSpew )
	{
		char hex[ 17 ];
		Q_binarytohex( (const byte *)&crc, sizeof( crc ), hex, sizeof( hex ) );
		Msg( "%s checksum = %sf\n", name, hex );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : char const
//-----------------------------------------------------------------------------
char const *GetGlobalFlexControllerName( int index )
{
	return g_GlobalFlexControllers[ index ].String();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int GetGlobalFlexControllerCount( void )
{
	return g_GlobalFlexControllers.Count();
}
//-----------------------------------------------------------------------------
// Purpose: Accumulates throughout runtime session, oh well
// Input  : *szName - 
// Output : int
//-----------------------------------------------------------------------------
int AddGlobalFlexController( StudioModel *model, const char *szName )
{
	int idx = g_GlobalFlexControllerLookup.Find( szName );
	if ( idx != g_GlobalFlexControllerLookup.InvalidIndex() )
	{
		return g_GlobalFlexControllerLookup[ idx ];
	}

	CUtlSymbol sym;
	sym = szName;
	idx = g_GlobalFlexControllers.AddToTail( sym );
	g_GlobalFlexControllerLookup.Insert( szName, idx );
	// Con_Printf( "Added global flex controller %i %s from %s\n", idx, szName, model->GetStudioHdr()->name );
	return idx;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *model - 
//-----------------------------------------------------------------------------
void SetupModelFlexcontrollerLinks( StudioModel *model )
{
	if ( !model )
		return;

	CStudioHdr *hdr = model->GetStudioHdr();
	if ( !hdr )
		return;

	if ( hdr->numflexcontrollers() <= 0 )
		return;

	// Already set up!!!
	if ( hdr->pFlexcontroller( LocalFlexController_t(0) )->localToGlobal != -1 )
		return;

	for (LocalFlexController_t i = LocalFlexController_t(0); i < hdr->numflexcontrollers(); i++)
	{
		int j = AddGlobalFlexController( model, hdr->pFlexcontroller( i )->pszName() );
		hdr->pFlexcontroller( i )->localToGlobal = j;
		model->SetFlexController( i, 0.0f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CExpressionManager : public IExpressionManager
{
public:
							CExpressionManager( void );
							~CExpressionManager( void );

	void					Reset( void );

	void					ActivateExpressionClass( CExpClass *cl );

	// File I/O
	void					LoadClass( const char *filename );
	void					CreateNewClass( const char *filename );
	bool					CloseClass( CExpClass *cl );

	CExpClass				*AddCExpClass( const char *classname, const char *filename );
	int						GetNumClasses( void );

	CExpression				*GetCopyBuffer( void );

	bool					CanClose( void );

	CExpClass				*GetActiveClass( void );
	CExpClass				*GetClass( int num );
	CExpClass				*FindClass( const char *classname, bool bMatchBaseNameOnly );

private:
	// Methods
	const char				*GetClassnameFromFilename( const char *filename );

// UI
	void					PopulateClassCB( CExpClass *cl );

	void					RemoveCExpClass( CExpClass *cl );

private:
	// Data
	CExpClass				*m_pActiveClass;
	CUtlVector < CExpClass * > m_Classes;

	CExpression				m_CopyBuffer;
};

// Expose interface
static CExpressionManager g_ExpressionManager;
IExpressionManager *expressions = &g_ExpressionManager;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CExpressionManager::CExpressionManager( void )
{
	m_pActiveClass = NULL;
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CExpressionManager::~CExpressionManager( void )
{
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CExpressionManager::Reset( void )
{
	while ( m_Classes.Size() > 0 )
	{
		CExpClass *p = m_Classes[ 0 ];
		m_Classes.Remove( 0 );
		delete p;
	}

	m_pActiveClass = NULL;

	memset( &m_CopyBuffer, 0, sizeof( m_CopyBuffer ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CExpClass	*CExpressionManager::GetActiveClass( void )
{
	return m_pActiveClass;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : num - 
// Output : CExpClass
//-----------------------------------------------------------------------------
CExpClass *CExpressionManager::GetClass( int num )
{
	return m_Classes[ num ];
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *classname - 
//			*filename - 
// Output : CExpClass *
//-----------------------------------------------------------------------------
CExpClass * CExpressionManager::AddCExpClass( const char *classname, const char *filename )
{
	Assert( !FindClass( classname, false ) );

	CExpClass *pclass = new CExpClass( classname );
	if ( !pclass )
		return NULL;
	
	m_Classes.AddToTail( pclass );

	pclass->SetFileName( filename );

	return pclass;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *cl - 
//-----------------------------------------------------------------------------
void CExpressionManager::RemoveCExpClass( CExpClass *cl )
{
	for ( int i = 0; i < m_Classes.Size(); i++ )
	{
		CExpClass *p = m_Classes[ i ];
		if ( p == cl )
		{
			m_Classes.Remove( i );
			delete p;
			break;
		}
	}

	if ( m_Classes.Size() >= 1 )
	{
		ActivateExpressionClass( m_Classes[ 0 ] );
	}
	else
	{
		ActivateExpressionClass( NULL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *cl - 
//-----------------------------------------------------------------------------
void CExpressionManager::ActivateExpressionClass( CExpClass *cl )
{
	m_pActiveClass = cl;
	int select = 0;
	for ( int i = 0; i < GetNumClasses(); i++ )
	{
		CExpClass *c = GetClass( i );
		if ( cl == c )
		{
			select = i;
			break;
		}
	}
	
	g_pExpressionClass->select( select );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CExpressionManager::GetNumClasses( void )
{
	return m_Classes.Size();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *classname - 
// Output : CExpClass
//-----------------------------------------------------------------------------
CExpClass *CExpressionManager::FindClass( const char *classname, bool bMatchBaseNameOnly )
{
	char search[ 256 ];
	if ( bMatchBaseNameOnly )
	{
		Q_FileBase( classname, search, sizeof( search ) );
	}
	else
	{
		Q_strncpy( search, classname, sizeof( search ) );
	}

	Q_FixSlashes( search );
	Q_strlower( search );

	for ( int i = 0; i < m_Classes.Size(); i++ )
	{
		CExpClass *cl = m_Classes[ i ];

		if ( !Q_stricmp( search, bMatchBaseNameOnly ? cl->GetBaseName() : cl->GetName() ) )
		{
			return cl;
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *filename - 
// Output : const char
//-----------------------------------------------------------------------------
const char *CExpressionManager::GetClassnameFromFilename( const char *filename )
{
	char cleanname[ 256 ];
	static char classname[ 256 ];
	classname[ 0 ] = 0;

	Assert( filename && filename[ 0 ] );

	// Strip the .txt
	Q_StripExtension( filename, cleanname, sizeof( cleanname ) );

	char *p = Q_stristr( cleanname, "expressions" );
	if ( p )
	{
		Q_strncpy( classname, p + Q_strlen( "expressions" ) + 1, sizeof( classname ) );
	}
	else
	{
		Assert( 0 );
		Q_strncpy( classname, cleanname, sizeof( classname ) );
	}

	Q_FixSlashes( classname );
	Q_strlower( classname );
	return classname;
};

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CExpression
//-----------------------------------------------------------------------------
CExpression *CExpressionManager::GetCopyBuffer( void )
{
	return &m_CopyBuffer;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CExpressionManager::CanClose( void )
{
	for ( int i = 0; i < m_Classes.Size(); i++ )
	{
		CExpClass *pclass = m_Classes[ i ];
		if ( pclass->GetDirty() )
		{
			return false;
		}
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *filename - 
//-----------------------------------------------------------------------------
void CExpressionManager::LoadClass( const char *inpath )
{
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

	if ( inpath[ 0 ] == '/' || inpath[ 0 ] == '\\' )
		++inpath;

	char filename[ 512 ];
	Q_strncpy( filename, inpath, sizeof( filename ) );

	CStudioHdr *hdr = models->GetActiveStudioModel()->GetStudioHdr();
	if ( !hdr )
	{
		Con_ErrorPrintf( "Can't load expressions from %s, must load a .mdl file first!\n",
			filename );
		return;
	}

	Con_Printf( "Loading expressions from %s\n", filename );

	const char *classname = GetClassnameFromFilename( filename );
	
	// Already loaded, don't do anything
	if ( FindClass( classname, false ) )
		return;

	// Import actual data
	LoadScriptFile( filename, SCRIPT_USE_RELATIVE_PATH );

	CExpClass *active = AddCExpClass( classname, filename );
	if ( !active )
		return;

	ActivateExpressionClass( active );

	int numflexmaps = 0;
	int flexmap[128]; // maps file local controls into global controls
	LocalFlexController_t localflexmap[128]; // maps file local controls into local controls
	bool bHasWeighting = false;
	bool bNormalized = false;

	EnableStickySnapshotMode( );

	while (1)
	{
		GetToken (true);
		if (endofscript)
			break;
		if (stricmp( token, "$keys" ) == 0)
		{
			numflexmaps = 0;
			while (TokenAvailable())
			{
				flexmap[numflexmaps] = -1;
				localflexmap[numflexmaps] = LocalFlexController_t(-1);

				GetToken( false );
				bool bFound = false;
				for (LocalFlexController_t i = LocalFlexController_t(0); i < hdr->numflexcontrollers(); i++)
				{
					if (stricmp( hdr->pFlexcontroller(i)->pszName(), token ) == 0)
					{
						localflexmap[numflexmaps] = i;
						flexmap[numflexmaps] = AddGlobalFlexController( models->GetActiveStudioModel(),
							hdr->pFlexcontroller(i)->pszName() );
						bFound = true;
						break;
					}
				}
				if ( !bFound )
				{
					flexmap[ numflexmaps ] = AddGlobalFlexController( models->GetActiveStudioModel(), token );
				}
				numflexmaps++;
			}
		}
		else if ( !stricmp( token, "$hasweighting" ) )
		{
			bHasWeighting = true;
		}
		else if ( !stricmp( token, "$normalized" ) )
		{
			bNormalized = true;
		}
		else
		{
			float setting[ GLOBAL_STUDIO_FLEX_CONTROL_COUNT ];
			float weight[ GLOBAL_STUDIO_FLEX_CONTROL_COUNT ];
			char name[ 256 ];
			char desc[ 256 ];
			int index;

			memset( setting, 0, sizeof( setting ) );
			memset( weight, 0, sizeof( weight ) );

			strcpy( name, token );

			// phoneme index
			GetToken( false );
			if (token[1] == 'x')
			{
				sscanf( &token[2], "%x", &index );
			}
			else
			{
				index = (int)token[0];
			}

			// key values
			for (int i = 0; i < numflexmaps; i++)
			{
				if (flexmap[i] > -1)
				{
					GetToken( false );
					setting[flexmap[i]] = atof( token );
					if (bHasWeighting)
					{
						GetToken( false );
						weight[flexmap[i]] = atof( token );
					}
					else
					{
						weight[flexmap[i]] = 1.0;
					}

					if ( bNormalized && localflexmap[ i ] > -1 )
					{
						mstudioflexcontroller_t *pFlex = hdr->pFlexcontroller( localflexmap[i] );
						if ( pFlex->min != pFlex->max )
						{
							setting[flexmap[i]] = Lerp( setting[flexmap[i]], pFlex->min, pFlex->max );  
						}
					}
				}
				else
				{
					GetToken( false );
					if (bHasWeighting)
					{
						GetToken( false );
					}
				}
			}

			// description
			GetToken( false );
			strcpy( desc, token );

			CExpression *exp = active->AddExpression( name, desc, setting, weight, false, false );
			if ( active->IsPhonemeClass() && exp )
			{
				if ( exp->index != index )
				{
					Con_Printf( "CExpressionManager::LoadClass (%s):  phoneme index for %s in .txt file is wrong (expecting %i got %i), ignoring...\n",
						classname, name, exp->index, index );
				}
			}
		}
	}

	active->CheckBitmapConsistency();

	DisableStickySnapshotMode( );

	PopulateClassCB( active );

	active->DeselectExpression();

	Assert( !active->GetDirty() );
	active->SetDirty( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *filename - 
//-----------------------------------------------------------------------------
void CExpressionManager::CreateNewClass( const char *filename )
{
	CStudioHdr *hdr = models->GetActiveStudioModel()->GetStudioHdr();
	if ( !hdr )
	{
		Con_ErrorPrintf( "Can't create new expression file %s, must load a .mdl file first!\n", filename );
		return;
	}

	// Tell the use that the filename was loaded, expressions are empty for now
	const char *classname = GetClassnameFromFilename( filename );
	
	// Already loaded, don't do anything
	if ( FindClass( classname, false ) )
		return;

	Con_Printf( "Creating %s\n", filename );

	CExpClass *active = AddCExpClass( classname, filename );
	if ( !active )
		return;

	ActivateExpressionClass( active );

	// Select the newly created class
	PopulateClassCB( active );

	// Select first expression
	active->SelectExpression( 0 );

	// Nothing has changed so far
	active->SetDirty( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *cl - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CExpressionManager::CloseClass( CExpClass *cl )
{
	if ( !cl )
		return true;

	if ( cl->GetDirty() )
	{
		int retval = mxMessageBox( NULL, va( "Save changes to class '%s'?", cl->GetName() ), g_appTitle, MX_MB_YESNOCANCEL );
		if ( retval == 2 )
		{
			return false;
		}
		if ( retval == 0 )
		{
			Con_Printf( "Saving changes to %s : %s\n", cl->GetName(), cl->GetFileName() );
			cl->Save();
		}
	}

	// The memory can be freed here, so be more careful
	char temp[ 256 ];
	V_strcpy_safe( temp, cl->GetName() );

	RemoveCExpClass( cl );

	Con_Printf( "Closed expression class %s\n", temp );

	CExpClass *active = GetActiveClass();
	if ( !active )
	{
		PopulateClassCB( NULL );
		g_pExpressionTrayTool->redraw();
		return true;
	}

	// Select the first remaining class
	PopulateClassCB( active );

	// Select first expression
	active->DeselectExpression();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : classnum - 
//-----------------------------------------------------------------------------
void CExpressionManager::PopulateClassCB( CExpClass *current )
{
	g_pExpressionClass->removeAll();
	int select = 0;
	for ( int i = 0; i < GetNumClasses(); i++ )
	{
		CExpClass *cl = GetClass( i );
		if ( !cl )
			continue;

		g_pExpressionClass->add( cl->GetName() );
		
		if ( cl == current )
		{
			select = i;
		}
	}
	
	g_pExpressionClass->select( select );
}