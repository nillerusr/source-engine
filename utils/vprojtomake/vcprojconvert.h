//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#ifndef VCPROJCONVERT_H
#define VCPROJCONVERT_H
#ifdef _WIN32
#pragma once
#endif

#include "stdafx.h"
#include "utlvector.h"
#include "utlsymbol.h"
#ifdef _WIN32
#include "msxml2.h"
#elif _LINUX
#include "xercesc/dom/DOMDocument.hpp"
#define IXMLDOMDocument DOMDocument
#define IXMLDOMElement DOMElement
XERCES_CPP_NAMESPACE_USE

#else
#error "Unsupported Platform"
#endif

//-----------------------------------------------------------------------------
// Purpose:  constructor
//-----------------------------------------------------------------------------
class CVCProjConvert
{
public:
	CVCProjConvert();
	~CVCProjConvert();

	bool LoadProject( const char *project );
	int GetNumConfigurations();
	CUtlSymbol & GetName() { return m_Name; }
	CUtlSymbol & GetBaseDir() { return m_BaseDir; }

	class CConfiguration
	{
	public:
		CConfiguration() {}
		~CConfiguration() {}

		typedef enum
		{
			FILE_SOURCE,
			FILE_HEADER,
			FILE_LIBRARY, 
			FILE_TYPE_UNKNOWN_E
		} FileType_e;

		class CFileEntry
		{
		public:
			CFileEntry( CUtlSymbol name, FileType_e type ) { m_Name = name; m_Type = type; }
			~CFileEntry() {}

			const char *GetName() { return m_Name.String(); }
			FileType_e GetType() { return m_Type; }
			bool operator==( const CFileEntry other ) const { return m_Name == other.m_Name; }

		private:
			FileType_e m_Type;
			CUtlSymbol m_Name;
		};

		void InsertFile( CFileEntry file ) { m_Files.AddToTail( file ); }
		void RemoveFile( CUtlSymbol file ) { m_Files.FindAndRemove( CFileEntry( file, FILE_TYPE_UNKNOWN_E ) ); } // file type doesn't matter on remove
		void SetName( CUtlSymbol name ) { m_Name = name; }

		int GetNumFileNames() { return m_Files.Count(); }
		const char * GetFileName(int i) { return m_Files[i].GetName(); }
		FileType_e GetFileType(int i) { return m_Files[i].GetType(); }
		CUtlSymbol & GetName() { return m_Name; }

		void ResetDefines() { m_Defines.RemoveAll(); }
		void AddDefine( CUtlSymbol define ) { m_Defines.AddToTail( define ); }
		int GetNumDefines() { return m_Defines.Count(); }
		const char *GetDefine( int i ) { return m_Defines[i].String(); } 

		void ResetIncludes() { m_Includes.RemoveAll(); }
		void AddInclude( CUtlSymbol include ) { m_Includes.AddToTail( include ); }
		int GetNumIncludes() { return m_Includes.Count(); }
		const char *GetInclude( int i ) { return m_Includes[i].String(); } 

	private:
		CUtlSymbol m_Name;
		CUtlVector<CUtlSymbol> m_Defines;
		CUtlVector<CUtlSymbol> m_Includes;
		CUtlVector<CFileEntry> m_Files;
	};

	CConfiguration & GetConfiguration( int i );
	int FindConfiguration( CUtlSymbol name );

private:
	bool ExtractFiles( IXMLDOMDocument *pDoc );
	bool ExtractConfigurations( IXMLDOMDocument *pDoc );
	bool ExtractProjectName( IXMLDOMDocument *pDoc );
	bool ExtractIncludes( IXMLDOMElement *pDoc, CConfiguration & config );
	bool IterateFileConfigurations( IXMLDOMElement *pFile, CUtlSymbol fileName );

	// helper funcs
	CUtlSymbol GetXMLNodeName( IXMLDOMElement *p );
	CUtlSymbol GetXMLAttribValue( IXMLDOMElement *p, const char *attribName );
	CConfiguration::FileType_e GetFileType( const char *fileName );
	void FindFileCaseInsensitive( char *file, int fileNameSize );

	// data
	CUtlVector<CConfiguration> m_Configurations;
	CUtlSymbol m_Name;
	CUtlSymbol m_BaseDir;
	bool m_bProjectLoaded;
};

#endif // VCPROJCONVERT_H
