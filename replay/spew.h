//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef SPEW_H
#define SPEW_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

class ISpewer
{
public:
	virtual ~ISpewer() {}

	virtual void PrintBlockStart() const = 0;
	virtual void PrintBlockEnd() const = 0;
	virtual void PrintEmptyLine() const = 0;
	virtual void PrintEventStartMsg( const char *pMsg ) const = 0;
	virtual void PrintEventResult( bool bSuccess ) const = 0;
	virtual void PrintEventError( const char *pError ) const = 0;
	virtual void PrintTestHeader( const char *pHeader ) const = 0;
	virtual void PrintMsg( const char *pMsg ) const = 0;
	virtual void PrintValue( const char *pWhat, const char *pValue ) const = 0;
};

//----------------------------------------------------------------------------------------

extern ISpewer *g_pDefaultSpewer;
extern ISpewer *g_pBlockSpewer;
extern ISpewer *g_pSimpleSpewer;
extern ISpewer *g_pNullSpewer;

//----------------------------------------------------------------------------------------

class CSpewScope
{
public:
	CSpewScope( ISpewer *pSpewer )
	{
		m_pOldSpewer = g_pDefaultSpewer;
		g_pDefaultSpewer = pSpewer;
	}

	~CSpewScope()
	{
		g_pDefaultSpewer = m_pOldSpewer;
	}

private:
	ISpewer	*m_pOldSpewer;
};

//----------------------------------------------------------------------------------------

class CBaseSpewer : public ISpewer
{
public:
	CBaseSpewer( ISpewer *pSpewer = g_pDefaultSpewer );

	//
	// ISpewer implementation for shorthand.
	//
	virtual void PrintBlockStart() const { m_pSpewer->PrintBlockStart(); }
	virtual void PrintBlockEnd() const { m_pSpewer->PrintBlockEnd(); }
	virtual void PrintEmptyLine() const { m_pSpewer->PrintEmptyLine(); }
	virtual void PrintEventStartMsg( const char *pMsg ) const { m_pSpewer->PrintEventStartMsg( pMsg ); }
	virtual void PrintEventResult( bool bSuccess ) const { m_pSpewer->PrintEventResult( bSuccess ); }
	virtual void PrintEventError( const char *pError ) const { m_pSpewer->PrintEventError( pError ); }
	virtual void PrintTestHeader( const char *pHeader ) const { m_pSpewer->PrintTestHeader( pHeader ); }
	virtual void PrintMsg( const char *pMsg ) const { m_pSpewer->PrintMsg( pMsg ); }
	virtual void PrintValue( const char *pWhat, const char *pValue ) const { m_pSpewer->PrintValue( pWhat, pValue ); }

private:
	ISpewer *m_pSpewer;
};

//----------------------------------------------------------------------------------------

#endif	// SPEW_H
