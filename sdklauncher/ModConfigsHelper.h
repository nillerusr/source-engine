//========= Copyright Valve Corporation, All rights reserved. ============//
/**
 * Utility class for providing some basic capabilities for enumerating and specifying MOD
 * directories.
 *
 * \version 1.0
 *
 * \date 07-18-2006
 *
 * \author mdurand
 *
 * \todo 
 *
 * \bug 
 *
 */
#ifndef MODCONFIGSHELPER_H
#define MODCONFIGSHELPER_H
#ifdef _WIN32
#pragma once
#endif

#include "utlvector.h"
#include "sdklauncher_main.h"

class ModConfigsHelper
{
public:
	/**
	 * Default constructor which automatically finds and sets the base directory for all MODs 
	 * and populates a vector of MOD directory names.
	 */
	ModConfigsHelper();

	/**
	 * Default destructor. 
	 */
	~ModConfigsHelper();

	/**
	 * Getter method that provides the parent directory for all MODs.
	 * \return parent directory for all MODs
	 */
	const char *getSourceModBaseDir();

	/**
	 * Getter method that provides a vector of the names of each MOD found.
	 * \return vector of the names of each MOD found
	 */
	const CUtlVector<char *> &getModDirsVector();

protected:
	/**
	 * Determines and sets the base directory for all MODs
	 */
	void setSourceModBaseDir();

	/**
	 * Searches the parent MOD directory for child MODs and puts their names in the member vector
	 */
	void EnumerateModDirs();

private:
	// Member that holds the base directory of MODs
	char m_sourceModBaseDir[MAX_PATH];

	// Member that holds the child MOD names
	CUtlVector<char *> m_ModDirs;
};

#endif // MODCONFIGSHELPER_H