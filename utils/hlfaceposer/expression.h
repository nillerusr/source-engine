//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef EXPRESSION_H
#define EXPRESSION_H
#ifdef _WIN32
#pragma once
#endif

#include "utlvector.h"
#include "mxBitmapTools.h"
#include "hlfaceposer.h"

#define GLOBAL_STUDIO_FLEX_CONTROL_COUNT ( MAXSTUDIOFLEXCTRL * 4 )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CExpUndoInfo
{
public:
	float				setting[ GLOBAL_STUDIO_FLEX_CONTROL_COUNT ];
	float				weight[ GLOBAL_STUDIO_FLEX_CONTROL_COUNT ];

	float				redosetting[ GLOBAL_STUDIO_FLEX_CONTROL_COUNT ];
	float				redoweight[ GLOBAL_STUDIO_FLEX_CONTROL_COUNT ];

	int					counter;
};

class CExpression;
class CExpClass;

typedef unsigned int CRC32_t;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CExpression
{
public:
						CExpression	( void );
						~CExpression ( void );

						CExpression( const CExpression& from );

	void				SetModified( bool mod );
	bool				GetModified( void );
						
	void				ResetUndo( void );

	bool				CanUndo( void );
	bool				CanRedo( void );

	int					UndoLevels( void );
	int					UndoCurrent( void );

	const char 			*GetBitmapFilename( int modelindex );
	const char			*GetBitmapCheckSum();
	CRC32_t				GetBitmapCRC();
	void				CreateNewBitmap( int modelindex );

	void				PushUndoInformation( void );
	void				PushRedoInformation( void );

	void				Undo( void );
	void				Redo( void );

	void				SetSelected( bool selected );
	bool				GetSelected( void );

	float				*GetSettings( void );
	float				*GetWeights( void );

	bool				GetDirty( void );
	void				SetDirty( bool dirty );

	void				Revert( void );

	CExpClass			*GetExpressionClass( void );
	void				SetExpressionClass( char const *classname );

	// name of expression
	char				name[32];			
	int					index;
	char				description[128];

	mxbitmapdata_t		m_Bitmap[ MAX_FP_MODELS ];

	bool				m_bModified;

	// Undo information
	CUtlVector< CExpUndoInfo * >		undo;
	int								m_nUndoCurrent;

	bool				m_bSelected;

	bool				m_bDirty;

private:
	// settings of fields
	float				setting[ GLOBAL_STUDIO_FLEX_CONTROL_COUNT ];		
	float				weight[ GLOBAL_STUDIO_FLEX_CONTROL_COUNT ];		

	char				expressionclass[ 128 ];

	void WipeRedoInformation(  void );
};

#endif // EXPRESSION_H
