//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
// This module manages a stack of "script sources".
//
//==================================================================================================

#ifndef SCRIPTSOURCE_H
#define SCRIPTSOURCE_H
#ifdef _WIN32
#pragma once
#endif

#define MAX_SYSPRINTMSG		4096
#define MAX_SYSTOKENCHARS	4096

class CScriptSource
{
public:
	CScriptSource()
	{
		Set( "", NULL, 0, false );
	}

	CScriptSource( const char *pScriptName, const char *pScriptData, int nScriptLine, bool bFreeScriptAtPop )
	{
		Set( pScriptName, pScriptData, nScriptLine, bFreeScriptAtPop );
	}

	void Set( const char *pScriptName, const char *pScriptData, int nScriptLine, bool bFreeScriptAtPop )
	{
		m_ScriptName = pScriptName;
		m_pScriptData = pScriptData;
		m_nScriptLine = nScriptLine;
		m_bFreeScriptAtPop = bFreeScriptAtPop;
	}

	const char *GetName() const			{ return m_ScriptName.Get(); }
	const char *GetData() const			{ return m_pScriptData; }
	int GetLine() const					{ return m_nScriptLine; }
	bool IsFreeScriptAtPop() const		{ return m_bFreeScriptAtPop; }

private:
	CUtlString	m_ScriptName;
	const char	*m_pScriptData;
	int			m_nScriptLine;
	bool		m_bFreeScriptAtPop;
};

class CScript
{
public:
	CScript();

	void			PushScript( const char *pFilename );
	void 			PushScript( const char *pScriptName, const char *ppScriptData, int nScriptLine = 1, bool bFreeScriptAtPop = false );
	void 			PushCurrentScript();
	void 			PopScript();
	CScriptSource	GetCurrentScript();
	void			RestoreScript( const CScriptSource &scriptSource );
	void			EnsureScriptStackEmpty();
	void			SpewScriptStack();

	const char		*GetName() const		{ return m_ScriptName.Get(); }
	const char		*GetData() const 		{ return m_pScriptData; }
	int				GetLine() const			{ return m_nScriptLine; }

	const char		*GetToken( bool bAllowLineBreaks );
	const char		*PeekNextToken( bool bAllowLineBreaks );
	void			SkipRestOfLine();
	void			SkipBracedSection(); 
	void			SkipToValidToken(); 

	bool			ParsePropertyValue( const char *pBaseString, char *pOutBuff, int outBuffSize );

private:
	const char		*SkipWhitespace( const char *data, bool *pHasNewLines, int *pNumLines );
	const char		*SkipToValidToken( const char *data, bool *pHasNewLines, int *pNumLines );
	void			SkipBracedSection( const char **dataptr, int *numlines );
	void			SkipRestOfLine( const char **dataptr, int *numlines );
	const char		*PeekNextToken( const char *dataptr, bool bAllowLineBreaks );
	const char		*GetToken( const char **dataptr, bool allowLineBreaks, int *pNumLines );

	CUtlStack< CScriptSource >	m_ScriptStack;

	int							m_nScriptLine;
	int							*m_pScriptLine;
	const char					*m_pScriptData;
	CUtlString					m_ScriptName;
	bool						m_bFreeScriptAtPop;

	char						m_Token[MAX_SYSTOKENCHARS];
	char						m_PeekToken[MAX_SYSTOKENCHARS];
};

#endif // SCRIPTSOURCE_H
