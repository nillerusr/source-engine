//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef EXPCLASS_H
#define EXPCLASS_H
#ifdef _WIN32
#pragma once
#endif

#include "utlvector.h"

class CExpression;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CExpClass
{
public:

						CExpClass( const char *classname );
	virtual 			~CExpClass( void );

	void				Save( void );
	void				Export( void );

	void				CheckBitmapConsistency( void );
	void				ReloadBitmaps( void );

	const char			*GetName() const;
	const char			*GetBaseName() const;
	const char			*GetFileName() const;
	void				SetFileName( const char *filename );

	CExpression			*AddExpression( const char *name, const char *description, float *flexsettings, float *flexweights, bool selectnewitem, bool bDirtyClass );
	CExpression			*FindExpression( const char *name );
	int					FindExpressionIndex( CExpression *exp );
	void				DeleteExpression( const char *name );
	
	int					GetNumExpressions( void );
	CExpression			*GetExpression( int num );

	bool				GetDirty( void );
	void				SetDirty( bool dirty );

	void				SelectExpression( int num, bool deselect = true );
	int					GetSelectedExpression( void );
	void				DeselectExpression( void );

	void				SwapExpressionOrder( int exp1, int exp2 );

	// Get index of this class in the global class list
	int					GetIndex( void );

	bool				IsPhonemeClass( void ) const;

private:

	void				BuildValidChecksums( CUtlRBTree< CRC32_t > &tree );

	char				m_szBaseName[ 128 ]; // name w/out any subdirectory names
	char				m_szClassName[ 128 ];
	char				m_szFileName[ 128 ];
	bool				m_bDirty;
	int					m_nSelectedExpression;
	CUtlVector < CExpression > m_Expressions;

	bool				m_bIsPhonemeClass;
};
#endif // EXPCLASS_H
