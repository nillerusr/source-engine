//======= Copyright © 1996-2007, Valve Corporation, All rights reserved. ======
//
// Purpose:
//
//=============================================================================

#ifndef VALVEPYTHON_H
#define VALVEPYTHON_H


#if defined( _WIN32 )
#pragma once
#endif


// Python includes
#include <Python.h>


// Valve includes
#include "interface.h"
#include "tier1/utlvector.h"
#include "tier1/utlstring.h"

//-----------------------------------------------------------------------------
// Finds the GetAppFactory function in the specified python module and returns
// the app factory handle or NULL on error
//-----------------------------------------------------------------------------
CreateInterfaceFn ValvePythonAppFactory( const char *pGetAppFactory = "GetAppFactory" );


//=============================================================================
//
// Should be called by each module's init function
//
//=============================================================================
bool ValvePythonInit( CreateInterfaceFn pInFactory = NULL );


//=============================================================================
//
// Python command factory class
//
//=============================================================================
class CValvePythonCommand
{
public:
	// Constructor
	CValvePythonCommand( const char *pName, PyCFunction pMeth, int nFlags, const char *pDoc )
	: m_name( pName )
	, m_pMeth( pMeth )
	, m_nFlags( nFlags )
	, m_doc( pDoc )
	{
		m_pNextFactory = s_pFirstFactory;
		s_pFirstFactory = this;
	}

	static void Register( char *pModuleName );

protected:

	void Register( CUtlVector< PyMethodDef > &pyMethodDefs );

private:
	// The next factory
	CValvePythonCommand *m_pNextFactory;
	static CValvePythonCommand *s_pFirstFactory;
	// Has to stay in same place in memory for duration of python
	static CUtlVector< PyMethodDef > s_pyMethodDefs;

	CUtlString m_name;
	PyCFunction m_pMeth;
	const int m_nFlags;
	CUtlString m_doc;
};


//-----------------------------------------------------------------------------
// Macro to install a valve python command
//
// Use it like this:
//
// PYTHON_COMMAND( foo, METH_VARARGS, "The documentation for foo" )
//-----------------------------------------------------------------------------
#define PYTHON_COMMAND( _name, _flags, _doc ) \
	extern "C" static PyObject *_name##_pythonFunc( PyObject *pSelf, PyObject *pArgs ); \
	static CValvePythonCommand _name##_command( #_name, _name##_pythonFunc, _flags, _doc ); \
	extern "C" static PyObject *_name##_pythonFunc( PyObject *pSelf, PyObject *pArgs )

//-----------------------------------------------------------------------------
// Macro to install a valve python command which uses keywords in the interface
//
// Use it like this:
//
// PYTHON_COMMAND( foo, METH_VARARGS, "The documentation for foo" )
//-----------------------------------------------------------------------------
#define PYTHON_COMMAND_KEYWORDS( _name, _flags, _doc ) \
	extern "C" static PyObject *_name##_pythonFunc( PyObject *pSelf, PyObject *pArgs, PyObject *pKeywords ); \
	static CValvePythonCommand _name##_command( #_name, reinterpret_cast< PyCFunction >( _name##_pythonFunc ), _flags, _doc ); \
	extern "C" static PyObject *_name##_pythonFunc( PyObject *pSelf, PyObject *pArgs, PyObject *pKeywords )


//=============================================================================
//
// Python sub module factory class
//
//=============================================================================
class CValvePythonSubModule
{
public:
	typedef void ( *PythonInitFunc_t )( void );

	// Constructor
	CValvePythonSubModule( const char *pName, PythonInitFunc_t pInitFunc )
	: m_name( "_" )
	, m_pInitFunc( pInitFunc )
	{
		m_name += pName;
		m_pNextFactory = s_pFirstFactory;
		s_pFirstFactory = this;
	}

	static void Register( PyObject *pPackage );

private:
	// The next factory
	CValvePythonSubModule *m_pNextFactory;
	static CValvePythonSubModule *s_pFirstFactory;

	CUtlString m_name;
	PythonInitFunc_t m_pInitFunc;
};


//-----------------------------------------------------------------------------
// Macro to install a valve python command
//
// Use it like this:
//
// PYTHON_COMMAND( foo, METH_VARARGS, "The documentation for foo" )
//-----------------------------------------------------------------------------
#define PYTHON_SUBMODULE( _name ) \
	extern "C" void init_##_name( void ); \
	static CValvePythonSubModule _name##_submodule( #_name, init_##_name );


// Support for implemented extended python commands which can take multiple position arguments and specified keyword arguments
//
typedef enum _pytypes
{
	PY_FIELD_VOID = 0,			// No type or value
	PY_FIELD_BOOLEAN,			// boolean, implemented as an int, I may use this as a hint for compression
	PY_FIELD_CHARACTER,			// a byte
	PY_FIELD_UTLSTRING,			// CUtlString
	PY_FIELD_SHORT,				// 2 byte integer
	PY_FIELD_INTEGER,			// Any integer or enum
	PY_FIELD_FLOAT,				// Any floating point value
	PY_FIELD_VECTOR,			// Any vector, QAngle, or AngularImpulse
	PY_FIELD_QUATERNION,		// A quaternion
	PY_FIELD_VMATRIX,			// a Vmatrix (output coords are NOT worldspace)
	PY_FIELD_MATRIX3X4,			// matrix3x4_t

	PY_FIELD_TYPECOUNT,			// MUST BE LAST
} pytype_t;

#define _PY_PARAMETER(var_name,type,parameter_name,help)		{ type, #var_name, offsetof(classNameTypedef, var_name), -1, parameter_name, help, NULL, sizeof( ((classNameTypedef *)0)->var_name ) }
#define _PY_ARGUMENT(var_name,type,argument_index,help)			{ type, #var_name, offsetof(classNameTypedef, var_name), argument_index, "", help, NULL, sizeof( ((classNameTypedef *)0)->var_name ) }

#define DEFINE_PY_PARAMETER(var_name,type,parameter_name,help)				_PY_PARAMETER(var_name, type, parameter_name, help )
#define DEFINE_PY_ARGUMENT(var_name,type,argument_index,help)				_PY_ARGUMENT(var_name, type, argument_index, help )

struct pydatamap_t;
struct pytypedescription_t;

#define SIZE_OF_ARRAY(p)	_ARRAYSIZE(p)

#define DECLARE_PY_PARAMETER_DESC() \
	static pydatamap_t m_ParameterMap; \
	static pydatamap_t *GetBaseParameterMap(); \
	template <typename T> friend void ParameterMapAccess(T *, pydatamap_t **p); \
	template <typename T> friend pydatamap_t *ParameterMapInit(T *); \
	virtual pydatamap_t *GetParameterMap( void );

#define BEGIN_PY_PARAMETERS( className ) \
	pydatamap_t className::m_ParameterMap = { 0, 0, #className, NULL }; \
	pydatamap_t *className::GetParameterMap( void ) { return &m_ParameterMap; } \
	pydatamap_t *className::GetBaseParameterMap() { pydatamap_t *pResult; ParameterMapAccess((BaseClass *)NULL, &pResult); return pResult; } \
	BEGIN_PY_PARAMETERS_GUTS( className )

#define BEGIN_PY_PARAMETERS_NO_BASE( className ) \
	pydatamap_t className::m_ParameterMap = { 0, 0, #className, NULL }; \
	pydatamap_t *className::GetParameterMap( void ) { return &m_ParameterMap; } \
	pydatamap_t *className::GetBaseParameterMap() { return NULL; } \
	BEGIN_PY_PARAMETERS_GUTS( className )

#define BEGIN_PY_PARAMETERS_GUTS( className ) \
	template <typename T> pydatamap_t *ParameterMapInit(T *); \
	template <> pydatamap_t *ParameterMapInit<className>( className * ); \
	namespace className##_ParameterMapInit \
	{ \
		pydatamap_t *g_DataMapHolder = ParameterMapInit( (className *)NULL ); /* This can/will be used for some clean up duties later */ \
	} \
	\
	template <> pydatamap_t *ParameterMapInit<className>( className * ) \
	{ \
		typedef className classNameTypedef; \
		static CParameterGeneratedNameHolder nameHolder(#className); \
		className::m_ParameterMap.baseMap = className::GetBaseParameterMap(); \
		static pytypedescription_t dataDesc[] = \
		{ \
		{ PY_FIELD_VOID,0,0,0,0,0,0 }, /* so you can define "empty" tables */

#define END_PY_PARAMETERS() \
		}; \
		\
		if ( sizeof( dataDesc ) > sizeof( dataDesc[0] ) ) \
		{ \
			classNameTypedef::m_ParameterMap.dataNumFields = SIZE_OF_ARRAY( dataDesc ) - 1; \
			classNameTypedef::m_ParameterMap.dataDesc 	  = &dataDesc[1]; \
		} \
		else \
		{ \
			classNameTypedef::m_ParameterMap.dataNumFields = 1; \
			classNameTypedef::m_ParameterMap.dataDesc 	  = dataDesc; \
		} \
		return &classNameTypedef::m_ParameterMap; \
	}

#define IMPLEMENT_NULL_PY_PARAMETERS( derivedClass ) \
	BEGIN_PY_PARAMETERS_GUTS( derivedClass ) \
	END_PY_PARAMETERS()

struct pytypedescription_t
{
	pytype_t			type;
	const char			*var_name;
	int					offset;
	// index of the argument in the script file function call
	int					argument_index;
	// the name of the parameter in script files
	const char			*parameter_name;	
	const char			*help;
	// For embedding additional datatables inside this one
	pydatamap_t			*td;  // NOT HOOKED UP YET!!!

	// Stores the actual member variable size in bytes
	int					fieldSizeInBytes;
};

struct pydatamap_t
{
	pytypedescription_t	*dataDesc;
	int					dataNumFields;
	char const			*dataClassName;
	pydatamap_t			*baseMap;
};

template <typename T> 
inline void ParameterMapAccess(T *ignored, pydatamap_t **p)
{
	*p = &T::m_ParameterMap;
}


//-----------------------------------------------------------------------------

class CParameterGeneratedNameHolder
{
public:
	CParameterGeneratedNameHolder( const char *pszBase )
		: m_pszBase(pszBase)
	{
		m_nLenBase = strlen( m_pszBase );
	}

	~CParameterGeneratedNameHolder()
	{
		for ( int i = 0; i < m_Names.Count(); i++ )
		{
			delete m_Names[i];
		}
	}

	const char *GenerateName( const char *pszIdentifier )
	{
		char *pBuf = new char[m_nLenBase + strlen(pszIdentifier) + 1];
		strcpy( pBuf, m_pszBase );
		strcat( pBuf, pszIdentifier );
		m_Names.AddToTail( pBuf );
		return pBuf;
	}

private:
	const char *m_pszBase;
	size_t m_nLenBase;
	CUtlVector<char *> m_Names;
};

#define PYTHON_COMMAND_EXTENDED( _className ) \
	static _className g_##_className##Instance;\
	extern "C" static PyObject *g_##_className##Dispatch( PyObject *pSelf, PyObject *pArgs, PyObject *pKeywords ) \
{ \
	return g_##_className##Instance.DispatchRaw( pSelf, pArgs, pKeywords ); \
} \
	static CValvePythonCommand _className##_command( g_##_className##Instance.GetName(), reinterpret_cast< PyCFunction >( g_##_className##Dispatch ), g_##_className##Instance.GetFlags(), g_##_className##Instance.GetDoc() ); \

class CBasePythonCommand
{
	DECLARE_PY_PARAMETER_DESC();

public:
	CBasePythonCommand( const char *pchCommandName, int nFlags, char const *pchHelpText, pydatamap_t *pDataMap );

	PyObject *DispatchRaw( PyObject *pSelf, PyObject *pArgs, PyObject *pKeywords );

	char const *GetName() const;
	char const *GetDoc() const;
	int GetFlags() const;
	
	virtual PyObject *Dispatch( CUtlVector< CUtlString > &args, CUtlVector< int > &typedArgs ) = 0;
	virtual void SetDefaults() = 0;

private:

	void InitParameters();
	void ProcessArguments( PyObject *pArgs, PyObject *pKeywords, CUtlVector< CUtlString > &args, CUtlVector< int > &typedArgs );
	pytypedescription_t *FindParameter( char const *pchName );
	pytypedescription_t *FindArgument( int index );
	void ExtractParameter( char const *pchName, pytypedescription_t *pTypeDescription, PyObject *pValue );
	template <class T>
	inline bool IsParameterTypeValid( pytype_t, T * ) const;

protected:

	CUtlString BuildAutoGeneratedField( char const *pchParameterName, char const *pchPrefix, int *pCounter );

	template < class T >
	inline bool GetParameter( char const *pchParameterName, T *value );

protected:

	char const					*m_pchCommandName;
	int							m_nFlags;
	char const					*m_pchBaseHelpText;
	CUtlString					m_HelpText;
	pydatamap_t					*m_pDataMap;
};

#endif // VALVEPYTHON_H