//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Handles parsing and routing of shell commands to their handlers.
//
// $NoKeywords: $
//=============================================================================//

#ifndef SHELL_H
#define SHELL_H
#pragma once


class CMapDoc;
struct ShellDispatchTable_t;


class CShell
{
	public:

		CShell(void);
		~CShell(void);

		void SetDocument(CMapDoc *pDoc);
		bool RunCommand(const char *pszCommand);

	private:

		//
		// Shell command handlers.
		//
		bool BeginSession(const char *pszCommand, const char *pszArguments);
		bool CheckMapVersion(const char *pszCommand, const char *pszArguments);
		bool EndSession(const char *pszCommand, const char *pszArguments);
		bool EntityCreate(const char *pszCommand, const char *pszArguments);
		bool EntityDelete(const char *pszCommand, const char *pszArguments);
		bool EntitySetKeyValue(const char *pszCommand, const char *pszArguments);
		bool EntityRotateIncremental(const char *pszCommand, const char *pszArguments);
		bool NodeCreate(const char *pszCommand, const char *pszArguments);
		bool NodeDelete(const char *pszCommand, const char *pszArguments);
		bool NodeLinkCreate(const char *pszCommand, const char *pszArguments);
		bool NodeLinkDelete(const char *pszCommand, const char *pszArguments);
		bool ReleaseVideoMemory(const char *pszCommand, const char *pszArguments);
		bool GrabVideoMemory(const char *pszCommand, const char *pszArguments);

		//
		// Utility functions.
		//
		bool DoVersionCheck(const char *pszArguments);

		static ShellDispatchTable_t m_DispatchTable[];

		CMapDoc *m_pDoc;
};

#endif // SHELL_H
