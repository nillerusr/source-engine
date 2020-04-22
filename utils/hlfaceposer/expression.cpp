//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "hlfaceposer.h"
#include <mxtk/mx.h>
#include "expressions.h"
#include "StudioModel.h"
#include "filesystem.h"
#include "viewersettings.h"
#include "matsyswin.h"
#include "checksum_crc.h"
#include "expclass.h"
#include "ControlPanel.h"
#include "faceposer_models.h"
#include "mdlviewer.h"

static int g_counter = 0;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CExpression::CExpression( void )
{
	name[ 0 ] = 0;
	index = 0;
	description[ 0 ] = 0;
	memset( setting, 0, sizeof( setting ) );

	for ( int i = 0; i < MAX_FP_MODELS; i++ )
	{
		m_Bitmap[ i ].valid = false;
	}

	m_nUndoCurrent = 0;
	m_bModified = false;

	m_bSelected = false;

	m_bDirty = false;

	expressionclass[ 0 ] = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Copy constructor
// Input  : from - 
//-----------------------------------------------------------------------------
CExpression::CExpression( const CExpression& from )
{
	int i;

	strcpy( name, from.name );
	index = from.index;
	strcpy( description, from.description );
	
	for ( i = 0; i < MAX_FP_MODELS; i++ )
	{
		m_Bitmap[ i ] = from.m_Bitmap[ i ];
	}

	m_bModified = from.m_bModified;

	for ( i = 0 ; i < from.undo.Size(); i++ )
	{
		CExpUndoInfo *newUndo = new CExpUndoInfo();
		*newUndo = *from.undo[ i ];
		undo.AddToTail( newUndo );
	}

	m_nUndoCurrent = from.m_nUndoCurrent;

	m_bSelected = from.m_bSelected;

	m_bDirty = from.m_bDirty;

	strcpy( expressionclass, from.expressionclass );

	memcpy( setting, from.setting, GLOBAL_STUDIO_FLEX_CONTROL_COUNT * sizeof( float ) );
	memcpy( weight, from.weight, GLOBAL_STUDIO_FLEX_CONTROL_COUNT * sizeof( float ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CExpression::~CExpression( void )
{
	ResetUndo();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CExpression::GetDirty( void )
{
	return m_bDirty;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dirty - 
//-----------------------------------------------------------------------------
void CExpression::SetDirty( bool dirty )
{
	m_bDirty = dirty;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float *CExpression::GetSettings( void )
{
	return setting;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float *CExpression::GetWeights( void )
{
	return weight;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mod - 
//-----------------------------------------------------------------------------
void CExpression::SetModified( bool mod )
{
	m_bModified = mod;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CExpression::GetModified( void )
{
	return m_bModified;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : selected - 
//-----------------------------------------------------------------------------
void CExpression::SetSelected( bool selected )
{
	m_bSelected = selected;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CExpression::GetSelected( void )
{
	return m_bSelected;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CExpression::ResetUndo( void )
{
	CExpUndoInfo *u;
	for ( int i = 0; i < undo.Size(); i++ )
	{
		u = undo[ i ];
		delete u;
	}

	undo.RemoveAll();
	m_nUndoCurrent = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CExpression::CanRedo( void )
{
	if ( !undo.Size() )
		return false;

	if ( m_nUndoCurrent == 0 )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CExpression::CanUndo( void )
{
	if ( !undo.Size() )
		return false;

	if ( m_nUndoCurrent >= undo.Size() )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CExpression::UndoLevels( void )
{
	return undo.Size();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CExpression::UndoCurrent( void )
{
	return m_nUndoCurrent;
}

void ChecksumFlexControllers( bool bSpew, char const *name, CRC32_t &crc, const float *settings, const float *weights );

CRC32_t	CExpression::GetBitmapCRC()
{
	CRC32_t crc;

	float *s = setting;
	float *w = weight;

	// Note, we'll use the pristine values if this has changed
	if ( undo.Size() >= 1 )
	{
		s = undo[ undo.Size() - 1 ]->setting;
		w = undo[ undo.Size() - 1 ]->weight;
	}

	// This walks the global controllers sorted by name and only includes values with a setting or value which is != 0.0f

	ChecksumFlexControllers( false, name, crc, s, w );

	return crc;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *CExpression::GetBitmapCheckSum()
{
	CRC32_t crc = GetBitmapCRC();	

	// Create string name out of binary data
	static char hex[ 9 ];
	Q_binarytohex( (byte *)&crc, sizeof( crc ), hex, sizeof( hex ) );
	return hex;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *CExpression::GetBitmapFilename( int modelindex )
{
	static char filename[ 256 ] = { 0 };
	
	char const *classname = "error";
	CExpClass *cl = GetExpressionClass();
	if ( cl )
	{
		classname = cl->GetBaseName();
	}
	
	char modelName[512], modelNameTemp[512];
	Q_strncpy( modelNameTemp, models->GetModelName( modelindex ), sizeof( modelNameTemp ) );

	char const *in = modelNameTemp;
	char *out = modelName;

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


	sprintf( filename, "expressions/%s/%s/%s.bmp", modelName, classname, GetBitmapCheckSum() );

	Q_FixSlashes( filename );
	strlwr( filename );

	CreatePath( filename );
	
	return filename;
}

void CExpression::CreateNewBitmap( int modelindex )
{
	MatSysWindow *pWnd = g_pMatSysWindow;
	if ( !pWnd ) 
		return;

	StudioModel *model = models->GetStudioModel( modelindex );
	if ( !model )
		return;

	CStudioHdr *hdr = models->GetStudioHeader( modelindex );
	if ( !hdr )
		return;

	char filename[ 256 ];
	V_strcpy_safe( filename, GetBitmapFilename( modelindex ) );
	if ( !Q_strstr( filename, ".bmp" ) )
		return;

	models->CreateNewBitmap( modelindex, filename, 0, 128, true, this, &m_Bitmap[ modelindex ] );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *exp - 
//-----------------------------------------------------------------------------
void CExpression::PushUndoInformation( void )
{
	SetModified( true );

	// A real change to the data wipes out the redo counters
	WipeRedoInformation();

	CExpUndoInfo *newundo = new CExpUndoInfo;

	memcpy( newundo->setting, setting, GLOBAL_STUDIO_FLEX_CONTROL_COUNT * sizeof( float ) );
	memset( newundo->redosetting, 0, GLOBAL_STUDIO_FLEX_CONTROL_COUNT * sizeof( float ) );
	memcpy( newundo->weight, weight, GLOBAL_STUDIO_FLEX_CONTROL_COUNT * sizeof( float ) );
	memset( newundo->redoweight, 0, GLOBAL_STUDIO_FLEX_CONTROL_COUNT * sizeof( float ) );

	newundo->counter = g_counter++;

	undo.AddToHead( newundo );

	Assert( m_nUndoCurrent == 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *exp - 
//-----------------------------------------------------------------------------
void CExpression::PushRedoInformation( void )
{
	Assert( undo.Size() >= 1 );

	CExpUndoInfo *redo = undo[ 0 ];
	memcpy( redo->redosetting, setting, GLOBAL_STUDIO_FLEX_CONTROL_COUNT * sizeof( float ) );
	memcpy( redo->redoweight, weight, GLOBAL_STUDIO_FLEX_CONTROL_COUNT * sizeof( float ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *exp - 
//-----------------------------------------------------------------------------
void CExpression::Undo( void )
{
	if ( !CanUndo() )
		return;

	Assert( m_nUndoCurrent < undo.Size() );

	CExpUndoInfo *u = undo[ m_nUndoCurrent++ ];
	Assert( u );
	
	memcpy( setting, u->setting, GLOBAL_STUDIO_FLEX_CONTROL_COUNT * sizeof( float ) );
	memcpy( weight, u->weight, GLOBAL_STUDIO_FLEX_CONTROL_COUNT * sizeof( float ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *exp - 
//-----------------------------------------------------------------------------
void CExpression::Redo( void )
{
	if ( !CanRedo() )
		return;

	Assert( m_nUndoCurrent >= 1 );
	Assert( m_nUndoCurrent <= undo.Size() );

	CExpUndoInfo *u = undo[ --m_nUndoCurrent ];
	Assert( u );
	
	memcpy( setting, u->redosetting, GLOBAL_STUDIO_FLEX_CONTROL_COUNT * sizeof( float ) );
	memcpy( weight, u->redoweight, GLOBAL_STUDIO_FLEX_CONTROL_COUNT * sizeof( float ) );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *exp - 
//-----------------------------------------------------------------------------
void CExpression::WipeRedoInformation( void )
{
	// Wipe out all stuff newer then m_nUndoCurrent
	int level = 0;
	while ( level < m_nUndoCurrent )
	{
		CExpUndoInfo *u = undo[ 0 ];
		undo.Remove( 0 );
		Assert( u );
		delete u;
		level++;
	}

	m_nUndoCurrent = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Revert to last saved state
//-----------------------------------------------------------------------------
void CExpression::Revert( void )
{
	SetDirty( false );

	if ( undo.Size() <= 0 )
		return;

	// Go back to original data
	CExpUndoInfo *u = undo[ undo.Size() - 1 ];
	Assert( u );

	memcpy( setting, u->setting, GLOBAL_STUDIO_FLEX_CONTROL_COUNT * sizeof( float ) );
	memcpy( weight, u->weight, GLOBAL_STUDIO_FLEX_CONTROL_COUNT * sizeof( float ) );

	ResetUndo();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CExpClass
//-----------------------------------------------------------------------------
CExpClass *CExpression::GetExpressionClass( void )
{
	CExpClass *cl = expressions->FindClass( expressionclass, false );
	if ( !cl )
	{
		Assert( cl );
	}
	return cl;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *classname - 
//-----------------------------------------------------------------------------
void CExpression::SetExpressionClass( char const *classname )
{
	strcpy( expressionclass, classname );
}

