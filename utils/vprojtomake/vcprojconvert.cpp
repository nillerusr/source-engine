//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#ifdef _LINUX
#include <ctime> // needed by xercesc
#endif

#include "stdafx.h"
#include "tier0/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#include <comutil.h> // _variant_t
#include <atlbase.h> // CComPtr
#elif _LINUX
#include <unistd.h>
#include <dirent.h> // scandir()
#define _stat stat


#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/util/XMLString.hpp>
#include <xercesc/dom/DOM.hpp>
#include <xercesc/sax/HandlerBase.hpp>
#include <xercesc/parsers/XercesDOMParser.hpp>

#include "valve_minmax_off.h"
#if defined(XERCES_NEW_IOSTREAMS)
#include <iostream>
#else
#include <iostream.h>
#endif

#include "valve_minmax_on.h"

#define IXMLDOMNode DOMNode
#define IXMLDOMNodeList DOMNodeList

#define _alloca alloca

XERCES_CPP_NAMESPACE_USE

class XStr
{
public :
    XStr(const char* const toTranscode)
    {
        // Call the private transcoding method
        fUnicodeForm = XMLString::transcode(toTranscode);
    }

    ~XStr()
    {
        XMLString::release(&fUnicodeForm);
    }


    // -----------------------------------------------------------------------
    //  Getter methods
    // -----------------------------------------------------------------------
    const XMLCh* unicodeForm() const
    {
        return fUnicodeForm;
    }

private :
    XMLCh*   fUnicodeForm;
};

#define  _bstr_t(str) XStr(str).unicodeForm()


#else
#error "Unsupported platform"
#endif

#include "vcprojconvert.h"
#include "utlvector.h"

//-----------------------------------------------------------------------------
// Purpose:  constructor
//-----------------------------------------------------------------------------
CVCProjConvert::CVCProjConvert()
{
#ifdef _WIN32
	::CoInitialize(NULL); 
#elif _LINUX
	try {
            XMLPlatformUtils::Initialize();
        }
        catch (const XMLException& toCatch) {
            char* message = XMLString::transcode(toCatch.getMessage());
            Error( "Error during initialization! : %s\n", message);
            XMLString::release(&message);
        }
#endif
	m_bProjectLoaded = false;
}

//-----------------------------------------------------------------------------
// Purpose:  destructor
//-----------------------------------------------------------------------------
CVCProjConvert::~CVCProjConvert()
{
#ifdef _WIN32
	::CoUninitialize(); 
#elif _LINUX
	// nothing to shutdown
#endif
}

//-----------------------------------------------------------------------------
// Purpose: load up a project and parse it
//-----------------------------------------------------------------------------
bool CVCProjConvert::LoadProject( const char *project )
{
#ifdef _WIN32
	HRESULT hr;
	IXMLDOMDocument *pXMLDoc=NULL;

	hr = ::CoCreateInstance(CLSID_DOMDocument, 
							NULL, 
							CLSCTX_INPROC_SERVER, 
							IID_IXMLDOMDocument, 
							(void**)&pXMLDoc);

	if (FAILED(hr))
	{
		Msg ("Cannot instantiate msxml2.dll\n");
		Msg ("Please download the MSXML run-time (url below)\n");
		Msg ("http://msdn.microsoft.com/downloads/default.asp?url=/downloads/sample.asp?url=/msdn-files/027/001/766/msdncompositedoc.xml\n");
		return false;
	}

	VARIANT_BOOL vtbool;
	_variant_t bstrProject(project);

	pXMLDoc->put_async( VARIANT_BOOL(FALSE) );
	hr = pXMLDoc->load(bstrProject,&vtbool);
	if (FAILED(hr) || vtbool==VARIANT_FALSE)
	{
		Msg ("Could not open %s.\n", bstrProject);
		pXMLDoc->Release();
		return false;
	} 
#elif _LINUX
	XercesDOMParser* parser = new XercesDOMParser();
        parser->setValidationScheme(XercesDOMParser::Val_Always);    // optional.
        parser->setDoNamespaces(true);    // optional

        ErrorHandler* errHandler = (ErrorHandler*) new HandlerBase();
        parser->setErrorHandler(errHandler);

        try {
            parser->parse(project);
        }
        catch (const XMLException& toCatch) {
            char* message = XMLString::transcode(toCatch.getMessage());
            Error( "Exception message is: %s\n", message );    
            XMLString::release(&message);
            return;
        }
        catch (const DOMException& toCatch) {
            char* message = XMLString::transcode(toCatch.msg);
            Error( "Exception message is: %s\n", message );    
            XMLString::release(&message);
            return;
        }
        catch (...) {
            Error( "Unexpected Exception \n" );
            return;
        }
	
	DOMDocument *pXMLDoc = parser->getDocument();
#endif

	ExtractProjectName( pXMLDoc );
	if ( !m_Name.IsValid() )
	{
		Msg( "Failed to extract project name\n" );
		return false;
	}
	char baseDir[ MAX_PATH ];
	Q_ExtractFilePath( project, baseDir, sizeof(baseDir) );
	Q_StripTrailingSlash( baseDir );
	m_BaseDir = baseDir;

	ExtractConfigurations( pXMLDoc );
	if ( m_Configurations.Count() == 0 )
	{
		Msg( "Failed to find any configurations to load\n" );
		return false;
	}

	ExtractFiles( pXMLDoc );

#ifdef _WIN32
	pXMLDoc->Release();
#elif _LINUX
	delete pXMLDoc;
	delete errHandler;
#endif

	m_bProjectLoaded = true;
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: returns the number of different configurations loaded
//-----------------------------------------------------------------------------
int CVCProjConvert::GetNumConfigurations()
{
	Assert( m_bProjectLoaded );
	return m_Configurations.Count();

}

//-----------------------------------------------------------------------------
// Purpose: returns the index of a config with this name, -1 on err
//-----------------------------------------------------------------------------
int CVCProjConvert::FindConfiguration( CUtlSymbol name )
{
	if ( !name.IsValid() )
	{
		return -1;
	}
	
	for ( int i = 0; i < m_Configurations.Count(); i++ )
	{
		if ( m_Configurations[i].GetName() == name )
		{
			return i;
		}
	}
	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: extracts the value of the xml attrib "attribName"
//-----------------------------------------------------------------------------
CUtlSymbol CVCProjConvert::GetXMLAttribValue( IXMLDOMElement *p, const char *attribName )
{
	if (!p) 
	{
		return CUtlSymbol();
	}

#ifdef _WIN32
	VARIANT vtValue;
	p->getAttribute( _bstr_t(attribName), &vtValue);
	if ( vtValue.vt == VT_NULL )
	{
		return CUtlSymbol(); // element not found
	}

	Assert( vtValue.vt == VT_BSTR );
	CUtlSymbol name( static_cast<char *>( _bstr_t( vtValue.bstrVal ) ) );
	::SysFreeString(vtValue.bstrVal);
#elif _LINUX
	const XMLCh *xAttrib = XMLString::transcode( attribName );
	const XMLCh *value = p->getAttribute( xAttrib );
	if ( value == NULL )
	{
		return CUtlSymbol(); // element not found
        }
	char *transValue = XMLString::transcode(value);
	CUtlSymbol name( transValue );
	XMLString::release( &xAttrib );
	XMLString::release( &transValue );
#endif
	return name;

}

//-----------------------------------------------------------------------------
// Purpose: returns the name of this node
//-----------------------------------------------------------------------------
CUtlSymbol CVCProjConvert::GetXMLNodeName( IXMLDOMElement *p )
{
	CUtlSymbol name;
	if (!p) 
	{
		return name;
	}

#ifdef _WIN32
	BSTR bstrName;
	p->get_nodeName( &bstrName );
	_bstr_t bstr(bstrName);
	name = static_cast<char *>(bstr);
	return name;
#elif _LINUX
	Assert( 0 );
	Error( "Function CVCProjConvert::GetXMLNodeName not implemented\n" );
	return name;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: returns the config object at this index
//-----------------------------------------------------------------------------
CVCProjConvert::CConfiguration & CVCProjConvert::GetConfiguration( int i )
{
	Assert( m_bProjectLoaded );
	Assert( m_Configurations.IsValidIndex(i) );
	return m_Configurations[i];
}

//-----------------------------------------------------------------------------
// Purpose: extracts the project name from the loaded vcproj
//-----------------------------------------------------------------------------
bool CVCProjConvert::ExtractProjectName( IXMLDOMDocument *pDoc )
{
#ifdef _WIN32
	CComPtr<IXMLDOMNodeList> pProj;
	pDoc->getElementsByTagName( _bstr_t("VisualStudioProject"), &pProj);
	if (pProj)
	{
		long len = 0;
		pProj->get_length(&len);
		Assert( len == 1 );
		if ( len == 1 )
		{
			CComPtr<IXMLDOMNode> pNode;
			pProj->get_item( 0, &pNode );
			if (pNode)
			{
				CComQIPtr<IXMLDOMElement> pElem( pNode );
				m_Name = GetXMLAttribValue( pElem, "Name");
			}
		}
	}
#elif _LINUX
	DOMNodeList *nodes = pDoc->getElementsByTagName( _bstr_t("VisualStudioProject") );
	if ( nodes )
	{
		int len = nodes->getLength();
		if ( len == 1 )
		{
			DOMNode *node = nodes->item(0);
			if ( node )
			{
				m_Name = GetXMLAttribValue( node, "Name" );
			}
		}
	}
#endif
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: extracts the list of configuration names from the vcproj
//-----------------------------------------------------------------------------
bool CVCProjConvert::ExtractConfigurations( IXMLDOMDocument *pDoc )
{
	m_Configurations.RemoveAll();

	if (!pDoc)
	{
		return false;
	}

#ifdef _WIN32
	CComPtr<IXMLDOMNodeList> pConfigs;
	pDoc->getElementsByTagName( _bstr_t("Configuration"), &pConfigs);
    if (pConfigs)
	{
		long len = 0;
		pConfigs->get_length(&len);
		for ( int i=0; i<len; i++ )
		{
			CComPtr<IXMLDOMNode> pNode;
			pConfigs->get_item( i, &pNode );
			if (pNode)
			{
				CComQIPtr<IXMLDOMElement> pElem( pNode );
				CUtlSymbol configName = GetXMLAttribValue( pElem, "Name" );
				if ( configName.IsValid() )
				{
					int newIndex = m_Configurations.AddToTail();
					CConfiguration & config = m_Configurations[newIndex];
					config.SetName( configName );
					ExtractIncludes( pElem, config );
				}
			}
		}
	}
#elif _LINUX
	 DOMNodeList *nodes = pDoc->getElementsByTagName( _bstr_t("Configuration"));
    	if ( nodes )
	{
		int len = nodes->getLength();
		for ( int i=0; i<len; i++ )
		{
			DOMNode *node = nodes->item(i);
			if (node)
			{
				CUtlSymbol configName = GetXMLAttribValue( node, "Name" );
				if ( configName.IsValid() )
				{
					int newIndex = m_Configurations.AddToTail();
					CConfiguration & config = m_Configurations[newIndex];
					config.SetName( configName );
					ExtractIncludes( node, config );
				}
			}
		}
	}
#endif
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: extracts the list of defines and includes used for this config
//-----------------------------------------------------------------------------
bool CVCProjConvert::ExtractIncludes( IXMLDOMElement *pDoc, CConfiguration & config )
{
	config.ResetDefines();
	config.ResetIncludes();
	
	if (!pDoc)
	{
		return false;
	}

#ifdef _WIN32
	CComPtr<IXMLDOMNodeList> pTools;
	pDoc->getElementsByTagName( _bstr_t("Tool"), &pTools);
	if (pTools)
	{
		long len = 0;
		pTools->get_length(&len);
		for ( int i=0; i<len; i++ )
		{
			CComPtr<IXMLDOMNode> pNode;
			pTools->get_item( i, &pNode );
			if (pNode)
			{
				CComQIPtr<IXMLDOMElement> pElem( pNode );
				CUtlSymbol toolName = GetXMLAttribValue( pElem, "Name" );
				if ( toolName == "VCCLCompilerTool" )
				{
					CUtlSymbol defines = GetXMLAttribValue( pElem, "PreprocessorDefinitions" );
					char *str = (char *)_alloca( Q_strlen( defines.String() ) + 1 );
					Assert( str );
					Q_strcpy( str, defines.String() );
					// now tokenize the string on the ";" char
					char *delim = strchr( str, ';' );
					char *curpos = str;
					while ( delim )
					{
						*delim = 0;
						delim++;
						if ( Q_stricmp( curpos, "WIN32" ) && Q_stricmp( curpos, "_WIN32" )  &&  
							 Q_stricmp( curpos, "_WINDOWS") && Q_stricmp( curpos, "WINDOWS")) // don't add WIN32 defines
						{
							config.AddDefine( curpos );
						}
						curpos = delim;
						delim = strchr( delim, ';' );
					}
					if ( Q_stricmp( curpos, "WIN32" ) && Q_stricmp( curpos, "_WIN32" )  &&  
						 Q_stricmp( curpos, "_WINDOWS") && Q_stricmp( curpos, "WINDOWS")) // don't add WIN32 defines
					{
						config.AddDefine( curpos );
					}

					CUtlSymbol includes = GetXMLAttribValue( pElem, "AdditionalIncludeDirectories" );
					char *str2 = (char *)_alloca( Q_strlen( includes.String() ) + 1 );
					Assert( str2 );
					Q_strcpy( str2, includes.String() );
					// now tokenize the string on the ";" char
					delim = strchr( str2, ',' );
					curpos = str2;
					while ( delim )
					{
						*delim = 0;
						delim++;
						config.AddInclude( curpos );
						curpos = delim;
						delim = strchr( delim, ',' );
					}
					config.AddInclude( curpos );
				}
			}
		}
	}
#elif _LINUX
	DOMNodeList *nodes= pDoc->getElementsByTagName( _bstr_t("Tool"));
	if (nodes)
	{
		int len = nodes->getLength();
		for ( int i=0; i<len; i++ )
		{
			DOMNode *node = nodes->item(i);
			if (node)
			{
				CUtlSymbol toolName = GetXMLAttribValue( node, "Name" );
				if ( toolName == "VCCLCompilerTool" )
				{
					CUtlSymbol defines = GetXMLAttribValue( node, "PreprocessorDefinitions" );
					char *str = (char *)_alloca( Q_strlen( defines.String() ) + 1 );
					Assert( str );
					Q_strcpy( str, defines.String() );
					// now tokenize the string on the ";" char
					char *delim = strchr( str, ';' );
					char *curpos = str;
					while ( delim )
					{
						*delim = 0;
						delim++;
						if ( Q_stricmp( curpos, "WIN32" ) && Q_stricmp( curpos, "_WIN32" )  &&  
							 Q_stricmp( curpos, "_WINDOWS") && Q_stricmp( curpos, "WINDOWS")) // don't add WIN32 defines
						{
							config.AddDefine( curpos );
						}
						curpos = delim;
						delim = strchr( delim, ';' );
					}
					if ( Q_stricmp( curpos, "WIN32" ) && Q_stricmp( curpos, "_WIN32" )  &&  
						 Q_stricmp( curpos, "_WINDOWS") && Q_stricmp( curpos, "WINDOWS")) // don't add WIN32 defines
					{
						config.AddDefine( curpos );
					}

					CUtlSymbol includes = GetXMLAttribValue( node, "AdditionalIncludeDirectories" );
					char *str2 = (char *)_alloca( Q_strlen( includes.String() ) + 1 );
					Assert( str2 );
					Q_strcpy( str2, includes.String() );
					// now tokenize the string on the ";" char
					char token = ',';
					delim = strchr( str2, token );
					if ( !delim )
					{
						token = ';';
						delim = strchr( str2, token );
					}
					curpos = str2;
					while ( delim )
					{
						*delim = 0;
						delim++;
						Q_FixSlashes( curpos );
						char fullPath[ MAX_PATH ];
						Q_snprintf( fullPath, sizeof(fullPath), "%s/%s", m_BaseDir.String(), curpos );
						Q_StripTrailingSlash( fullPath );
						config.AddInclude( fullPath );
						curpos = delim;
						delim = strchr( delim, token );
					}
					Q_FixSlashes( curpos );
					Q_strlower( curpos );
					char fullPath[ MAX_PATH ];
					Q_snprintf( fullPath, sizeof(fullPath), "%s/%s", m_BaseDir.String(), curpos );
					Q_StripTrailingSlash( fullPath );
					config.AddInclude( fullPath );
				}
			}
		}
	}

#endif
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: walks a particular files config entry and removes an files not valid for this config
//-----------------------------------------------------------------------------
bool CVCProjConvert::IterateFileConfigurations( IXMLDOMElement *pFile, CUtlSymbol fileName )
{
#ifdef _WIN32
	CComPtr<IXMLDOMNodeList> pConfigs;
	pFile->getElementsByTagName( _bstr_t("FileConfiguration"), &pConfigs);
    if (pConfigs)
	{
		long len = 0;
		pConfigs->get_length(&len);
		for ( int i=0; i<len; i++ )
		{
			CComPtr<IXMLDOMNode> pNode;
			pConfigs->get_item( i, &pNode);
			if (pNode)
			{
				CComQIPtr<IXMLDOMElement> pElem( pNode );
				CUtlSymbol configName = GetXMLAttribValue( pElem, "Name");
				CUtlSymbol excluded = GetXMLAttribValue( pElem ,"ExcludedFromBuild");
				if ( configName.IsValid() && excluded.IsValid() )
				{
					int index = FindConfiguration( configName );
					if ( index > 0 && excluded == "TRUE" )
					{
						m_Configurations[index].RemoveFile( fileName );
					}
				}
			}

		}//for
	}//if
#elif _LINUX
	DOMNodeList *nodes = pFile->getElementsByTagName( _bstr_t("FileConfiguration"));
	if (nodes)
	{
		int len = nodes->getLength();
		for ( int i=0; i<len; i++ )
		{
			DOMNode *node = nodes->item(i);
			if (node)
			{
				CUtlSymbol configName = GetXMLAttribValue( node, "Name");
				CUtlSymbol excluded = GetXMLAttribValue( node ,"ExcludedFromBuild");
				if ( configName.IsValid() && excluded.IsValid() )
				{
					int index = FindConfiguration( configName );
					if ( index >= 0 && excluded == "TRUE" )
					{
						m_Configurations[index].RemoveFile( fileName );
					}
				}
			}

		}//for
	}//if
#endif

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: walks the file elements in the vcproj and inserts them into configs
//-----------------------------------------------------------------------------
bool CVCProjConvert::ExtractFiles( IXMLDOMDocument *pDoc  )
{
	if (!pDoc)
	{
		return false;
	}
	Assert( m_Configurations.Count() ); // some configs must be loaded first

#ifdef _WIN32
	CComPtr<IXMLDOMNodeList> pFiles;
	pDoc->getElementsByTagName( _bstr_t("File"), &pFiles);
	if (pFiles)
	{
		long len = 0;
		pFiles->get_length(&len);
		for ( int i=0; i<len; i++ )
		{
			CComPtr<IXMLDOMNode> pNode;
			pFiles->get_item( i, &pNode);
			if (pNode)
			{
				CComQIPtr<IXMLDOMElement> pElem( pNode );
				CUtlSymbol fileName = GetXMLAttribValue(pElem,"RelativePath");
				if ( fileName.IsValid() )
				{
					CConfiguration::FileType_e type = GetFileType( fileName.String() );
					CConfiguration::CFileEntry fileEntry( fileName.String(), type );
					for ( int i = 0; i < m_Configurations.Count(); i++ ) // add the file to all configs
					{
						CConfiguration & config = m_Configurations[i];
						config.InsertFile( fileEntry );
					}
					IterateFileConfigurations( pElem, fileName ); // now remove the excluded ones
				}
			}
		}//for
	}
#elif _LINUX
	DOMNodeList *nodes = pDoc->getElementsByTagName( _bstr_t("File") );
	if (nodes)
	{
		int len = nodes->getLength();
		for ( int i=0; i<len; i++ )
		{
			DOMNode *node = nodes->item(i);
			if (node)
			{
				CUtlSymbol fileName = GetXMLAttribValue(node,"RelativePath");
				if ( fileName.IsValid() )
				{
					char fixedFileName[ MAX_PATH ];
					Q_strncpy( fixedFileName, fileName.String(), sizeof(fixedFileName) );
					if ( fixedFileName[0] == '.' && fixedFileName[1] == '\\' )
					{
						Q_memmove( fixedFileName, fixedFileName+2, sizeof(fixedFileName)-2 );
					}

					Q_FixSlashes( fixedFileName );
					FindFileCaseInsensitive( fixedFileName, sizeof(fixedFileName) );
					CConfiguration::FileType_e type = GetFileType( fileName.String() );
					CConfiguration::CFileEntry fileEntry( fixedFileName, type );
					for ( int i = 0; i < m_Configurations.Count(); i++ ) // add the file to all configs
					{
						CConfiguration & config = m_Configurations[i];
						config.InsertFile( fileEntry );
					}
					IterateFileConfigurations( node, fixedFileName ); // now remove the excluded ones
				}
			}
		}//for
	}
#endif
	return true;
}

#ifdef _LINUX
static char fileName[MAX_PATH];
int CheckName(const struct dirent *dir)
{
        return !strcasecmp( dir->d_name, fileName );
}

const char *findFileInDirCaseInsensitive(const char *file)
{
        const char *dirSep = strrchr(file,'/');
        if( !dirSep )
        {
                dirSep=strrchr(file,'\\');
                if( !dirSep )
                {
                        return NULL;
                }
        }

        char *dirName = static_cast<char *>( alloca( ( dirSep - file ) +1 ) );
        if( !dirName )
                return NULL;

		Q_strncpy( dirName, file, dirSep - file );
        dirName[ dirSep - file ] = '\0';

        struct dirent **namelist;
        int n;

		Q_strncpy( fileName, dirSep + 1, MAX_PATH );


        n = scandir( dirName , &namelist, CheckName, alphasort );

        if( n > 0 )
        {
                while( n > 1 )
                {
                        free( namelist[n] ); // free the malloc'd strings
                        n--;
                }

                Q_snprintf( fileName, sizeof( fileName ), "%s/%s", dirName, namelist[0]->d_name );
                return fileName;
        }
        else
        {
                // last ditch attempt, just return the lower case version!
                Q_strncpy( fileName, file, sizeof(fileName) );
                Q_strlower( fileName );
                return fileName;
        }
}
#endif

void CVCProjConvert::FindFileCaseInsensitive( char *fileName, int fileNameSize )
{
	char filePath[ MAX_PATH ];

	Q_snprintf( filePath, sizeof(filePath), "%s/%s", m_BaseDir.String(), fileName ); 

	struct _stat buf;
	if ( _stat( filePath, &buf ) == 0)
	{
		return; // found the filename directly
	}

#ifdef _LINUX
	const char *realName = findFileInDirCaseInsensitive( filePath );
	if ( realName )
	{
		Q_strncpy( fileName, realName+strlen(m_BaseDir.String())+1, fileNameSize );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: extracts the generic type of a file being loaded
//-----------------------------------------------------------------------------
CVCProjConvert::CConfiguration::FileType_e CVCProjConvert::GetFileType( const char *fileName )
{
	CConfiguration::FileType_e type = CConfiguration::FILE_TYPE_UNKNOWN_E;
	char ext[10];
	Q_ExtractFileExtension( fileName, ext, sizeof(ext) );
	if ( !Q_stricmp( ext, "lib" ) )
	{
		type = CConfiguration::FILE_LIBRARY;
	}
	else if ( !Q_stricmp( ext, "h" ) )
	{
		type = CConfiguration::FILE_HEADER;
	}
	else if ( !Q_stricmp( ext, "hh" ) )
	{
		type = CConfiguration::FILE_HEADER;
	}
	else if ( !Q_stricmp( ext, "hpp" ) )
	{
		type = CConfiguration::FILE_HEADER;
	}
	else if ( !Q_stricmp( ext, "cpp" ) )
	{
		type = CConfiguration::FILE_SOURCE;
	}
	else if ( !Q_stricmp( ext, "c" ) )
	{
		type = CConfiguration::FILE_SOURCE;
	}
	else if ( !Q_stricmp( ext, "cc" ) )
	{
		type = CConfiguration::FILE_SOURCE;
	}

	return type;
}
