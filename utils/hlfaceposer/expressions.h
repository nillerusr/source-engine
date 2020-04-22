//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#if !defined( EXPRESSIONS_H )
#define EXPRESSIONS_H
#ifdef _WIN32
#pragma once
#endif

#include "studio.h"
#include "expression.h"

class FlexPanel;
class ControlPanel;
class MatSysWindow;
class CExpClass;
class ExpressionTool;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class IExpressionManager
{
public:
	virtual void				Reset( void ) = 0;

	// File i/o
	virtual void				LoadClass( const char *filename ) = 0;
	virtual void				CreateNewClass( const char *filename ) = 0;
	virtual bool				CloseClass( CExpClass *cl ) = 0;
	virtual void				ActivateExpressionClass( CExpClass *cl ) = 0;

	virtual CExpClass			*AddCExpClass( const char *classname, const char *filename ) = 0;
	virtual int					GetNumClasses( void ) = 0;

	virtual CExpression			*GetCopyBuffer( void ) = 0;

	virtual bool				CanClose( void ) = 0;

	virtual CExpClass			*GetActiveClass( void ) = 0;
	virtual CExpClass			*GetClass( int num ) = 0;
	virtual CExpClass			*FindClass( const char *classname, bool bMatchBaseNameOnly ) = 0;

};

extern IExpressionManager *expressions;

#endif // EXPRESSIONS_H