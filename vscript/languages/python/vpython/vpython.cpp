//========== Copyright © 2008, Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//=============================================================================

#ifdef PLATFORM
#define __PLATFORM PLATFORM
#undef PLATFORM
#endif 

#ifdef _DEBUG

#undef _DEBUG				// don't want a debug python header!
#include "Python.h"			// include python before any standard headers
#define _DEBUG

#else

#include "Python.h"

#endif // _DEBUG

#ifdef __PLATFORM
#undef PLATFORM
#define PLATFORM __PLATFORM
#undef __PLATFORM
#endif

#include <stdio.h>
#include <io.h>
#include <conio.h>
#include <direct.h>

#define _HAS_EXCEPTIONS 0
#include <string>

#include "platform.h"

#include "tier1/utlmap.h"

#include "datamap.h"
#include "tier1/functors.h"
#include "tier1/utlvector.h"
#include "tier1/utlhash.h"
#include "tier1/utlbuffer.h"
#include "tier1/fmtstr.h"
#include "tier1/convar.h"
#include "mathlib/vector.h"
#include "vstdlib/random.h"

#include "vscript/ivscript.h"

// #include <crtdbg.h>		// DEBUG: memory overwrites
#include "memdbgon.h"



/*
Implementation of the IVSCRIPT interface for Python 2.5
-------------------------------------------------------

The IVSCRIPT interface bridges the Python virtual machine (python25.dll), and a
set of server-side systems and objects, such as CBaseEntity.  Although the ivscript
interface is not changed for python, the python implementation works quite differently 
from the Squirrel implementation from a programming standpoint.

Squirrel ivscript associates instance data with an entity's 
code file, and each entity has its own copy of the code:
-----------------------------------------------------
The current implementation of ivscript that interfaces with the Squirrel virtual machine
assumes that every map that spawns may cause one or more script files to load, compile and 
execute.  In addition, every entity spawned within a map may also cause one or more
script files to load, compile and execute.  An entity's script code and data is fully isolated from other entities.  
Within an entity's code file, script variables such as 'self' and 'player'are auto-
defined, and they refer to the entity that invokes the code, the player etc.

This scheme creates an execution model with per-entity scope, and a global scope common
to all entities.  It also means that every entity keeps a unique, compiled copy of its code in memory. 
When a map is re/loaded, all code is flushed, and the Squirrel interpreter shuts down and restarts.  No code or data persists
beyond a level load or restart, and all code associated with a new map must be newly loaded and compiled.


Python ivscript associates instance data with the entity itself,
and all entities share the same compiled code:
-------------------------------------------------------
Python has a global scope, which contains a single compiled instance of each loaded code module.
When a code module is loaded, it is compiled and executed exactly once.  (Note that 'execution' of a 
loaded module creates module-global variables, and creates functions and classes).
Any variables defined within a module are global to functions/classes defined in the module, but not to other modules.  
In other words, code defined within the module shares only the module's data scope.  However, code can call
into another module, and must simply refer to any 'foreign' module data through explicit use of the module name.  
Within a module, a class definition also creates a scope, as do method definitions with classes.

Since all module-defined data is shared by all code within the module, we must associate instance
data with the entity instance, instead of the module (as is done in the Squirrel implementation).
This means that to preserve data associated with an entity, we pass the entity pointer as 'self' to all
module functions that need to access instance data as the first parameter. In python, we have very
simple ways to get and set data on an instance, including defining new data at runtime.

Unlike Squirrel ivscript, the Python interpreter is not shut down when a new map is loaded or
when the game is restarted.  This means that more compiled code is shared between maps 
as new modules are imported.  (NOTE: It also means that by the end of a game,
all unique script code modules could be simultaneously loaded in memory.  Compiled code is 
relatively small for full-featured modules- perhaps 10-20k. 20 modules would be a lot of 
script code for a game - so perhaps 200-400k total.  To manage large amounts of in-memory code,
we may need a mechanism for manually discarding
modules from memory that we know to be unique to a particulary map or entity instance.)


Hammer entity interface:
-----------------------
Rule 1: A python module should contain functionality corresponding to a particular type of entity.  For instance,
you might have a CameraMan.py module, a SpaceMarine.py module and a Medic.py module, each assigned as the ActivateScript
for a camera, spacemarine and medic entity types you've placed in hammer.

The 'ActivateScript' field of an entity may contain only one python module name, such as 'SpaceMarine.py'.
This module will be loaded and run the first time an entity is spawned with this module listed as
its ActivateScript.  This module may contain definitions of various Valve callbacks. Each callback
will be passed the entity instance as the first parameter, such as:

	ScriptThinkFunction( entity )  - called every tenth of a second during entity think
	OnDeath( entity ) - called when entity dies
	etc.

	These are identical to the regular vscript callbacks (see wiki), except that under the python
	system, they all take 'entity' as the first parameter.

*/
static int GetNewProxyId( void );
static void SetProxyBinding( int proxyId, ScriptFunctionBinding_t *pBinding );
static PyCFunction GetProxyFunction( int proxyId );
static ScriptFunctionBinding_t *GetProxyBinding( int proxyId );
static void InitProxyTable( void );
inline PyObject *CreatePyVector( Vector *pVector );
inline bool IsPyVector( PyObject *pobj );
inline bool VMInitFinalized( void );

//------------------------------------------------------------------------------
// python interpreter singleton
//------------------------------------------------------------------------------
static void *g_pVm = NULL;	

// object tags, largely used for memory validity checking on pointer casts
#define TYPETAG_INSTANCE	71717171
#define TYPETAG_SCOPE		81818181
#define TYPETAG_VECTOR		91919191

// NOTE: increase MAX_VALVE_FUNCTIONS_EXPORTED and the DPX(n) & SPX(n) entries at the end of this file if more than 260
// functions/methods are exported from valve dlls to vscript.
#define MAX_VALVE_FUNCTIONS_EXPORTED 260

// NOTE: increase this if we export more than N classes to python.
#define MAX_VALVE_CLASSES_EXPORTED 260


//------------------------------------------------------------------------------
// Module structs
//------------------------------------------------------------------------------

// a single python method definition followed by a null sentinel
typedef struct 
{
	PyMethodDef defs[2];
} pymethoddef_t;		

// valve server object instance data
typedef struct
{
	void *pInstance;				// instance of the valve object
	ScriptClassDesc_t *pClassDesc;	// binding descriptors for methods
	PyObject *pPyName;				// name
} InstanceContext_t;

// python object with data for valve server object instances
typedef struct 
{
	PyObject_HEAD;							
	
	/* instance data */
	PyObject *pDict;			// mapped to __dict__ of python instance object
	int typeTag;
	InstanceContext_t instanceContext;
	
} scriptClassInstance_t;


typedef struct 
{
		PyCFunction pfn;
		ScriptFunctionBinding_t *pBinding;	// binding to valve function
} proxybinding_t;

// global array mapping from proxy function id to function binding data
static proxybinding_t g_proxies[MAX_VALVE_FUNCTIONS_EXPORTED];


//------------------------------------------------------------------------------
// inline debug helpers
//------------------------------------------------------------------------------

// define this to always sanity check the object's ref counts
// NOTE: failing this can imply a reference count bug (i.e. too many frees)
// or possible memory corruption bug.
#define DEBUG_PY 1


inline void AssertIsPyObject( HSCRIPT hscript, int minRefCount = 0 )
{
#ifdef DEBUG_PY
	Assert( ((PyObject*)hscript == Py_None) || ((PyObject *)hscript)->ob_refcnt < 5000 && ((PyObject *)hscript)->ob_refcnt >= minRefCount);
#endif // DEBUG_PY
}

// object must be a python object and an instance object
inline void AssertIsInstance( HSCRIPT hscript )
{
#ifdef DEBUG_PY
	AssertIsPyObject( hscript, 0 );

	Assert ( ((scriptClassInstance_t *)hscript)->typeTag == TYPETAG_INSTANCE );
#endif // DEBUG_PY
}


//-------------------------------------------------------------------------
// Vector class
//-------------------------------------------------------------------------

// python vector instance with data for valve object instance
typedef struct 
{
	PyObject_HEAD;							
	
	/* instance data goes here*/
	int typeTag;
	Vector *pVector;
} PyVectorInstance_t;

inline void AssertIsVector( HSCRIPT hscope )
{
	Assert( ((PyVectorInstance_t *)hscope)->typeTag == TYPETAG_VECTOR );
}

static int DEBUG_VECCOUNT = 0;			// count the vector allocations vs frees (test for mem leaks)
static int DEBUG_VARIANTCOUNT = 0;		// count the variant allocations vs frees (test for mem leaks)
static int DEBUG_FUNCCOUNT = 0;		// count the function handle allocs vs frees (test for mem leaks)

//-------------------------------------------------------------
// called from python directly during object destruction.
//-------------------------------------------------------------
static void VectorRelease( PyObject *pSelf )
{
	AssertIsVector( (HSCRIPT)pSelf );

	// free the game vector
	if ( ((PyVectorInstance_t *)pSelf)->pVector )
	{
		delete ((PyVectorInstance_t *)pSelf)->pVector;

		DEBUG_VECCOUNT--;

	}
	// free the python object
	pSelf->ob_type->tp_free( pSelf );
}

//-------------------------------------------------------------
// called from python directly during object construction.
// same as class __init__ function - init the member data for valve instance
// Allocates a NEW Vector.  The new Vector object will be deleted
// when the python object is deleted via VectorRelease.
//-------------------------------------------------------------
static int VectorConstructNew( PyObject *pSelf, PyObject *pArgs, PyObject *pkwds )
{
	float x = 0.0;
	float y = 0.0;
	float z = 0.0;

	if ( pArgs )
	{
		PyArg_ParseTuple( pArgs, "|fff", &x, &y, &z );
	}
	
	Vector *pVector = new Vector(x, y, z);
	
	DEBUG_VECCOUNT++;
	Assert( DEBUG_VECCOUNT < 1000 );  // if this fails, we're likely leaking new vectors each frame. 

	((PyVectorInstance_t *)pSelf)->pVector = pVector;
	((PyVectorInstance_t *)pSelf)->typeTag = TYPETAG_VECTOR;
	return 0;
}
//--------------------------------------------------------------------
// creates a new python vector object which references the given Vector.
// (i.e. does NOT create a new Vector object). The referenced vector will
// be deleted when the python object is deleted via VectorRelease.
//--------------------------------------------------------------------
void VectorBuildCopy( PyObject *pSelf, Vector *pVector)
{
	((PyVectorInstance_t *)pSelf)->pVector = pVector;
	((PyVectorInstance_t *)pSelf)->typeTag = TYPETAG_VECTOR;
}

//-------------------------------------------------------------
// called from python directly on object attribute x,y,z access
//-------------------------------------------------------------
static PyObject * VectorGet( PyObject *pSelf, PyObject *pname )
{
	AssertIsVector( (HSCRIPT)pSelf );

	if (!PyString_Check( pname ))
		return NULL;

	Vector *pVector = ((PyVectorInstance_t *)pSelf)->pVector;

	if ( !pVector )
	{
		PyErr_SetString(PyExc_ValueError, "null vector");
		return NULL;
	}

	const char *pszKey = PyString_AsString( pname );

	if ( pszKey && *pszKey && !*(pszKey + 1) )
	{
		int index = *pszKey - 'x';
		if ( index >=0 && index <= 2)
		{
			float fret = (*pVector)[index];
			return PyFloat_FromDouble( (double)fret );
		}
	}
	return PyObject_GenericGetAttr( pSelf, pname );
}

//-------------------------------------------------------------
// called from python directly on object attribute x,y,z access
//-------------------------------------------------------------
static int VectorSet( PyObject *pSelf, PyObject *pname, PyObject *pval )
{
	AssertIsVector( (HSCRIPT)pSelf );

	if (!PyString_Check( pname ))
		return -1;

	Vector *pVector = ((PyVectorInstance_t *)pSelf)->pVector;
	if ( !pVector )
	{
		PyErr_SetString(PyExc_ValueError, "null vector");
		return -1;
	}

	const char *pszKey = PyString_AsString( pname );

	if ( pszKey && *pszKey && !*(pszKey + 1) )
	{
		int index = *pszKey - 'x';
		if ( index >=0 && index <= 2)
		{
			(*pVector)[index] = (float)PyFloat_AsDouble( pval );
			return 0;
		}
	}
	return -1;
	// no dictionary on vector object! return PyObject_GenericSetAttr( pSelf, pname, pval);
}


// repr function for vector
static PyObject * VectorToString( PyObject *pSelf )
{
	AssertIsVector( (HSCRIPT)pSelf );

	Vector *pVector = ((PyVectorInstance_t *)pSelf)->pVector;
	if ( !pVector )
	{
		return PyString_FromString("<Vector : null>");
	}
	return PyString_FromString( (static_cast<const char *>(CFmtStr("<Vector: %f %f %f>", pVector->x, pVector->y, pVector->z))) );
}

// repr function for instance
static PyObject * InstanceToString( PyObject *pSelf )
{
	AssertIsPyObject( (HSCRIPT)pSelf );

	return PyString_FromFormat("<%s at %p>", pSelf->ob_type->tp_name, (void*)pSelf );

	// UNDONE:
	//		StackHandler sa(hVM);
	//		InstanceContext_t *pContext = (InstanceContext_t *)sa.GetInstanceUp(1,0);
	//		char szBuf[64];
	//
	//		if ( pContext && pContext->pInstance && pContext->pClassDesc->pHelper && pContext->pClassDesc->pHelper->ToString( pContext->pInstance, szBuf, ARRAYSIZE(szBuf) ) )
	//		{
	//			sa.Return( szBuf );
	//		}
	//		else
	//		{
	//			HPYOBJECT hInstance = sa.GetObjectHandle( 1 );
	//			sq_pushstring( hVM, CFmtStr( "(instance : 0x%p)", (void*)_rawval(hInstance) ), -1 );
	//		}
	//		return 1;

}

static PyObject * VectorToKeyValueString( PyObject *pSelf, PyObject *pArgs )
{
	AssertIsVector( (HSCRIPT)pSelf );

	Vector *pVector = ((PyVectorInstance_t *)pSelf)->pVector;
	if ( !pVector )
	{
		PyErr_SetString(PyExc_ValueError, "null vector");
		return NULL;
	}

	return PyString_FromString( (static_cast<const char *>(CFmtStr("%f %f %f", pVector->x, pVector->y, pVector->z))) );
}

static PyObject * VectorAdd( PyObject *pSelf, PyObject *pOther )
{
	AssertIsVector( (HSCRIPT)pSelf );
	
	Vector *pVector = ((PyVectorInstance_t *)pSelf)->pVector;
	Vector *pVectorOther = ((PyVectorInstance_t *)pOther)->pVector;

	if ( !pVectorOther || !pVector )
	{
		PyErr_SetString(PyExc_ValueError, "null vector");
		return NULL;
	}

	if ( !IsPyVector( pOther ) )
	{
		PyErr_SetString(PyExc_ValueError, "can't add vector to non vector type");
		return NULL;
	}

	// create new PyTypeVector object - explicitly calls VectorConstructNew
	PyObject *pretObj = CreatePyVector( NULL );
	
	*( ((PyVectorInstance_t *)pretObj)->pVector ) = *pVector + *pVectorOther;
	
	return pretObj;
}

static PyObject * VectorSubtract( PyObject *pSelf, PyObject *pOther )
{
	AssertIsVector( (HSCRIPT)pSelf );

	Vector *pVector = ((PyVectorInstance_t *)pSelf)->pVector;
	Vector *pVectorOther = ((PyVectorInstance_t *)pOther)->pVector;

	if ( !pVectorOther || !pVector )
	{
		PyErr_SetString(PyExc_ValueError, "null vector");
		return NULL;
	}

	if ( !IsPyVector( pOther ) )
	{
		PyErr_SetString(PyExc_ValueError, "can't sub non vector type from vector");
		return NULL;
	}

	// create new PyTypeVector object
	PyObject *pretObj = CreatePyVector( NULL );
	
	*( ((PyVectorInstance_t *)pretObj)->pVector ) = *pVector - *pVectorOther;
	
	return pretObj;
}

static PyObject * VectorScale( PyObject *pSelf, PyObject *pScale )
{
	PyObject *pPyVec;
	PyObject *pPyScale;

	if ( ((PyVectorInstance_t *)pSelf)->typeTag == TYPETAG_VECTOR )
	{
		pPyVec = pSelf;
		pPyScale = pScale;
	}
	else
	{
		pPyVec = pScale;
		pPyScale = pSelf;
	}

	Vector *pVector = ((PyVectorInstance_t *)pPyVec)->pVector;

	if ( !pVector )
	{
		PyErr_SetString(PyExc_ValueError, "null vector");
		return NULL;
	}

	if ( !PyFloat_Check( pPyScale ) )
	{
		PyErr_SetString(PyExc_ValueError, "can't scale vector by non float type");
		return NULL;
	}

	float scale = (float)PyFloat_AsDouble( pPyScale );

	PyObject *pretObj = CreatePyVector( NULL );

	*( ((PyVectorInstance_t *)pretObj)->pVector ) = *pVector * scale;
	
	return pretObj;
}

static int VectorCoerce(PyObject **pv, PyObject **pw)
{	

	if ( PyFloat_Check(*pw) )
		return 0;  // NOTE: we don't actually coerce - vector multiply handles a float scalar
					// and all other math functions explicitly check 2nd arg type.
	else
		return 1; // can't coerce anything else
}

static PyObject * VectorLength( PyObject *pSelf, PyObject *pArgs )
{
	AssertIsVector( (HSCRIPT)pSelf );

	Vector *pVector = ((PyVectorInstance_t *)pSelf)->pVector;

	if ( !pVector )
	{
		PyErr_SetString(PyExc_ValueError, "null vector");
		return NULL;
	}

	float flLength = pVector->Length();
	
	return PyFloat_FromDouble( (double)flLength );
}

static PyObject * VectorLengthSqr( PyObject *pSelf, PyObject *pArgs )
{

	AssertIsVector( (HSCRIPT)pSelf );

	Vector *pVector = ((PyVectorInstance_t *)pSelf)->pVector;

	if ( !pVector )
	{
		PyErr_SetString(PyExc_ValueError, "null vector");
		return NULL;
	}

	float flLength = pVector->LengthSqr();
	return PyFloat_FromDouble( (double)flLength );
}

static PyObject * VectorLength2D( PyObject *pSelf, PyObject *pArgs )
{
	AssertIsVector( (HSCRIPT)pSelf );

	Vector *pVector = ((PyVectorInstance_t *)pSelf)->pVector;

	if ( !pVector )
	{
		PyErr_SetString(PyExc_ValueError, "null vector");
		return NULL;
	}

	float flLength = pVector->Length2D();
	return PyFloat_FromDouble( (double)flLength );
}

static PyObject * VectorLength2DSqr( PyObject *pSelf, PyObject *pArgs )
{
	AssertIsVector( (HSCRIPT)pSelf );

	Vector *pVector = ((PyVectorInstance_t *)pSelf)->pVector;

	if ( !pVector )
	{
		PyErr_SetString(PyExc_ValueError, "null vector");
		return NULL;
	}

	float flLength = pVector->Length2DSqr();
	return PyFloat_FromDouble( (double)flLength );
}

static PyObject * VectorCross( PyObject *pSelf, PyObject *pOther )
{
	AssertIsVector( (HSCRIPT)pSelf );

	Vector *pVector = ((PyVectorInstance_t *)pSelf)->pVector;
	Vector *pVectorOther = ((PyVectorInstance_t *)pOther)->pVector;

	if ( !pVectorOther || !pVector )
	{
		PyErr_SetString(PyExc_ValueError, "null vector");
		return NULL;
	}

	// create new PyTypeVector object
	PyObject *pretObj = CreatePyVector( NULL );
	
	*( ((PyVectorInstance_t *)pretObj)->pVector ) = (*pVector).Cross( *pVectorOther );

	return pretObj;
}

static PyObject * VectorDot( PyObject *pSelf, PyObject *pOther )
{
	AssertIsVector( (HSCRIPT)pSelf );

	Vector *pVector = ((PyVectorInstance_t *)pSelf)->pVector;
	Vector *pVectorOther = ((PyVectorInstance_t *)pOther)->pVector;

	if ( !pVectorOther || !pVector )
	{
		PyErr_SetString(PyExc_ValueError, "null vector");
		return NULL;
	}

	float flResult = (*pVector).Dot( *pVectorOther );
	return PyFloat_FromDouble( (double)flResult );
}

static PyObject * VectorNorm( PyObject *pSelf, PyObject *pArgs )
{
	AssertIsVector( (HSCRIPT)pSelf );

	Vector *pVector = ((PyVectorInstance_t *)pSelf)->pVector;

	if ( !pVector )
	{
		PyErr_SetString(PyExc_ValueError, "null vector");
		return NULL;
	}

	float flLength = pVector->NormalizeInPlace();
	return PyFloat_FromDouble( (double)flLength );
}

// python vector class methods

static PyMethodDef g_VectorFuncs[] =
{
	{ "ToKVString",		VectorToKeyValueString, METH_NOARGS, "Get x,y,z as string of space-separated floats." },
	{ "Length",			VectorLength,		METH_NOARGS, "Get the vector's magnitude." },
	{ "LengthSqr",		VectorLengthSqr,	METH_NOARGS, "Get the vector's magnitude squared." },
	{ "Length2D",		VectorLength2D,		METH_NOARGS, "2d x,y length." },
	{ "Length2DSqr",	VectorLength2DSqr,	METH_NOARGS, "Square of x,y length." },
	{ "Dot",			VectorDot,			METH_O, "Dot product of vectors." },
	{ "Cross",			VectorCross,		METH_O, "Cross product of vectors."},
	{ "Norm",			VectorNorm,			METH_NOARGS, "Normalize vector in place." },
	{ NULL, NULL, 0, NULL }
};

static PyNumberMethods g_VectorAsNumber = {
	(binaryfunc)VectorAdd,			/* nb_add */
	(binaryfunc)VectorSubtract,		/* nb_subtract */
	(binaryfunc)VectorScale,		/* nb_multiply */
	0,					/* nb_divide */
	0,					/* nb_remainder */
	0,					/* nb_divmod */
	0,					/* nb_power */
	0,					/* nb_negative */
	0,					/* nb_positive */
	0,					/* nb_absolute */
	0,					/* nb_nonzero */
	0,					/*nb_invert*/
	0,					/*nb_lshift*/
	0,					/*nb_rshift*/
	0,					/*nb_and*/
	0,					/*nb_xor*/
	0,					/*nb_or*/
	0,					// (coercion)VectorCoerce,	/*nb_coerce*/
	0,					/*nb_int*/
	0,					/*nb_long*/
	0,					/*nb_float*/
	0,					/*nb_oct*/
	0, 					/*nb_hex*/
	0,					/*nb_inplace_add*/
	0,					/*nb_inplace_subtract*/
	0,					/*nb_inplace_multiply*/
	0,					/*nb_inplace_divide*/
	0,					/*nb_inplace_remainder*/
	0,					/*nb_inplace_power*/
	0,					/*nb_inplace_lshift*/
	0,					/*nb_inplace_rshift*/
	0,					/*nb_inplace_and*/
	0,					/*nb_inplace_xor*/
	0,					/*nb_inplace_or*/
	0,					/* nb_floor_divide */
	0,					/* nb_true_divide */
	0,					/* nb_inplace_floor_divide */
	0,					/* nb_inplace_true_divide */
};

// python vector class template
static PyTypeObject PyTypeVector = {		
	PyObject_HEAD_INIT(NULL)	/* type type */ // set up by PyType_Ready() call later.
	0,							/*ob_size*/
	"Vector",					/*tp_name*/
	sizeof(PyVectorInstance_t),/*tp_basicsize*/
	0,							/*tp_itemsize*/
	(destructor)VectorRelease,	/*tp_dealloc*/		// consider
	0,							/*tp_print*/
	0,							/*tp_getattr*/
	0,							/*tp_setattr*/
	0,							/*tp_compare*/
	(reprfunc )VectorToString,	/*tp_repr*/			// consider
	&g_VectorAsNumber,			/*tp_as_number*/
	0,							/*tp_as_sequence*/
	0,							/*tp_as_mapping*/
	0,							/*tp_hash */
	0,							/*tp_call*/
	0,							/*tp_str*/
	(getattrofunc)VectorGet,	/*tp_getattro*/ 
	(setattrofunc)VectorSet,	/*tp_setattro*/
	0,							/*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_CHECKTYPES,		/*tp_flags*/ // don't coerce, all number functions check types
	0,							/* tp_doc */
	0,							/* tp_traverse */
	0,							/* tp_clear */
	0,							/* tp_richcompare */
	0,							/* tp_weaklistoffset */
	0,							/* tp_iter */
	0,							/* tp_iternext */
	g_VectorFuncs,				/* tp_methods */
	0,							/* tp_members */
	0,							/* tp_getset */
	0,							/* tp_base */			// base class type object
	0,							/* tp_dict */
	0,							/* tp_descr_get */
	0,							/* tp_descr_set */
	0,							/* tp_dictoffset */
	(initproc)VectorConstructNew,	/* tp_init */		// consider
	PyType_GenericAlloc,		/* tp_alloc */
	PyType_GenericNew,			/* tp_new */		// consider // BUG: doesn't call tp_init?
	PyObject_Del,				/* tp_free */

};

//-----------------------------------------------------------------------------------
// create a new python vector object from the PyTypeVector type
// if pVector is null, allocs a new vector object, otherwise just references pVector
//-----------------------------------------------------------------------------------
inline PyObject *CreatePyVector( Vector *pVector )
{
	PyObject *pretObj = PyType_GenericNew( &PyTypeVector, NULL, NULL ); // calls tp_alloc(type,0) -> calls PyType_GenericAlloc -> calls PyObject_MALLOC
	if ( pVector )
	{
		VectorBuildCopy( pretObj, pVector );	// use the provided vector to build the python object
	}
	else
	{
		VectorConstructNew( pretObj, NULL, NULL );	// allocate a new vector object within the python object
	}
	
	return pretObj;
}

inline bool IsPyVector( PyObject *pobj )
{
	return ( pobj->ob_type == &PyTypeVector );
}

// -----------------------------------------------------------
// register custom vector class into the 'valve' module scope
// -----------------------------------------------------------
bool RegisterVector( PyObject *pmodule )
{
	// finalize the new python vector type
	if (PyType_Ready( &PyTypeVector ) < 0)
		return false;

	// add the new class type to the 'valve' module
	Py_INCREF( &PyTypeVector );
	
	PyModule_AddObject( pmodule, "Vector", (PyObject *)&PyTypeVector );

	return true;
}

// -------------------------------------------------------
// Scope wrapper class. Python scope represents 
// a python module's dictionary, or an instance object's dictionary.
// -------------------------------------------------------

class CPyScope
{
public:
	CPyScope( )
	{
		m_pPyModule = NULL;			// a python module object
		m_pPySelf = NULL;			// an instance object 
		m_pTempDict = PyDict_New(); // holding location for data until 'self' is set in scope
		m_typeTag = TYPETAG_SCOPE;
	}

	//-------------------------------------------------------
	// release all python objects on delete
	//-------------------------------------------------------
	~CPyScope()
	{
		Py_XDECREF( m_pTempDict );
		Py_XDECREF( m_pPyModule );
		Py_XDECREF( m_pPySelf );
	}

	void SetInstanceObject( PyObject *pSelf )
	{
		m_pPySelf = (scriptClassInstance_t *)pSelf;
		Py_XINCREF( m_pPySelf );
	}

	void SetModuleObject( PyObject *pModule )
	{
		m_pPyModule = pModule;
		Py_XINCREF( m_pPyModule );
	}

	PyObject *GetInstanceObject ()
	{
		return (PyObject *)m_pPySelf; // return borrowed ref
	}	
	
	PyObject * GetInstanceDict() 
	{ 
		if ( !m_pPySelf )
			return NULL;

		return m_pPySelf->pDict;  // return borrowed ref
	}	

	PyObject * GetModuleDict() 
	{ 
		if ( !m_pPyModule )
			return NULL;

		return PyModule_GetDict( m_pPyModule ); // return borrowed ref
	}

	PyObject *GetModule()
	{
		return m_pPyModule;
	}
	
	void TransferTempDictToInstance()
	{
		// move contents of temp dict into instance dict.
		// don't overwrite values in instance dict.
		Assert ( m_pPySelf );
		PyDict_Merge( m_pPySelf->pDict, m_pTempDict, false );
	}

	void SetTempDictValue( const char *pszKey, PyObject *pyobj )
	{
		// set python key:value in temp dict
		PyDict_SetItemString( m_pTempDict, pszKey, pyobj );
	}

	scriptClassInstance_t *m_pPySelf;		// pointer to invoking entity instance - entity data lives here
	PyObject *m_pPyModule;					// pointer to module  - entity code lives here
	PyObject *m_pTempDict;

	int m_typeTag;
};

inline void AssertIsScope( HSCRIPT hScope )
{
	// validate hscope handle as a CPyScope object
	Assert ( !hScope || ((CPyScope *)hScope)->m_typeTag == TYPETAG_SCOPE );
}

//--------------------------------------------------------------------------------
// utility callback: display a string at the console
// python sys.stdio and sys.stderr are redirected here
//--------------------------------------------------------------------------------
static PyObject *vprint(PyObject *self, PyObject *args)
{
	const char *psz;

	int ret = PyArg_ParseTuple(args, "s", &psz);
	
	if ( ret )
		//DevMsg( (const tchar *)psz);
		Msg( (const tchar *)psz );
	else
		DevMsg("vpython.cpp, vprint error: bad argument?");

	return Py_BuildValue("i", 0);
};

//--------------------------------------------------------------------------------
// Reload the named module & fixes up the internal module pointer saved in the hScope.
// params: (PyObject *pSelf, PyObject *moduleName, PyObject *hScope )
// ignores first param 'self'.
//--------------------------------------------------------------------------------
static PyObject *ReplaceClosures( PyObject *self, PyObject *args ) 
{
	PyObject *pSelf;			// this is called through ExecuteFunction, which adds the self param
	const char *pszModuleName;
	PyObject *hScope;

	int ret = PyArg_ParseTuple(args, "OsO", &pSelf, &pszModuleName, &hScope);
	
	if ( !ret )
	{
		DevMsg("vpython.cpp, ReplaceClosures argument error!");
		return Py_BuildValue("i", 0);
	}

	// get the previously imported module handle
	PyObject *pOldModule = PyImport_AddModule( pszModuleName );
	
	if ( !pOldModule )
	{
		DevMsg("vpython.cpp, ReplaceClosures error: module was never previously loaded!" );
		return Py_BuildValue("i", 0);
	}


	PyObject *pNewModule = PyImport_ReloadModule( pOldModule );
	
	if ( !pNewModule )
	{
		DevMsg("vpython.cpp, ReplaceClosures error: module failed to reload!" );
		return Py_BuildValue("i", 0);
	}

	// fixup refs to old module object in hScope
	if ( hScope )
	{
		if ( pOldModule != pNewModule )
		{
			Assert( pOldModule == ((CPyScope *)hScope)->GetModule() );
			Py_XDECREF( pOldModule ); // release the ref count that the hScope is holding on the old module

			((CPyScope *)hScope)->SetModuleObject( pNewModule );
		}
	}

	return Py_BuildValue("i", 0);
}

//---------------------------------------------------------------------------------
// given euler angles Pitch, Yaw, Roll, get forward vector
//---------------------------------------------------------------------------------
static PyObject *VectorFromAngles( PyObject *self, PyObject *args )
{
	float fpitch, fyaw, froll;

	int ret = PyArg_ParseTuple(args, "|fff", &fpitch, &fyaw, &froll );
	if ( !ret )
	{
		DevMsg("vpython.cpp: invalid params to VectorFromAngles, expected 3 floats!");
		return Py_BuildValue("i",0);
	}

	// create new PyTypeVector object - explicitly calls VectorConstructNew
	PyObject *pretObj = CreatePyVector( NULL );

	QAngle angles(fpitch, fyaw, froll);	// QAngle(pitch, yaw, roll);
	Vector forward;

	AngleVectors( angles, &forward );

	*( ((PyVectorInstance_t *)pretObj)->pVector ) = forward;	// copy

	return pretObj;
}

//---------------------------------------------------------------------------------
// given forward vector, get euler angles Pitch, Yaw, Roll.  Roll is always 0.
//---------------------------------------------------------------------------------
static PyObject *AnglesFromVector( PyObject *self, PyObject *args )
{
	PyObject *pyVector;

	int ret = PyArg_ParseTuple(args, "|O",&pyVector );
	if ( !ret )
	{
		DevMsg("vpython.cpp: invalid params to AnglesFromVector, expected 1 vector");
		return Py_BuildValue("i",0);
	}
	
	QAngle angles;
	Vector forward = *((PyVectorInstance_t *)pyVector)->pVector; // copy

	VectorAngles( forward, angles); // returns pitch (x), yaw (y), roll (z)
	
	PyObject *pretObj1 = PyFloat_FromDouble(angles[PITCH]);
	PyObject *pretObj2 = PyFloat_FromDouble(angles[YAW]);
	PyObject *pretObj3 = PyFloat_FromDouble(angles[ROLL]);

	PyObject *pretObj = PyTuple_Pack(3, pretObj1, pretObj2, pretObj3);

	Py_XDECREF(pretObj1);
	Py_XDECREF(pretObj2);
	Py_XDECREF(pretObj3);

	return pretObj;
}

// ----------------------------------------------------------------------------
// given euler angles Pitch, Yaw, Roll, return tuple of vforward, vright, vup
// ----------------------------------------------------------------------------
static PyObject *VectorsFromAngles( PyObject *self, PyObject *args )
{
	float fpitch, fyaw, froll;

	int ret = PyArg_ParseTuple(args, "|fff", &fpitch, &fyaw, &froll );
	if ( !ret )
	{
		DevMsg("vpython.cpp: invalid params to VectorFromAngles, expected 3 floats!");
		return Py_BuildValue("i",0);
	}

	// create new PyTypeVector object - explicitly calls VectorConstructNew
	PyObject *pretObj1 = CreatePyVector( NULL );
	PyObject *pretObj2 = CreatePyVector( NULL );
	PyObject *pretObj3 = CreatePyVector( NULL );


	QAngle angles(fpitch, fyaw, froll);	// QAngle(pitch, yaw, roll);
	Vector forward;
	Vector right;
	Vector up;

	AngleVectors( angles, &forward, &right, &up );

	*( ((PyVectorInstance_t *)pretObj1)->pVector ) = forward;	// copy
	*( ((PyVectorInstance_t *)pretObj2)->pVector ) = right;	// copy
	*( ((PyVectorInstance_t *)pretObj3)->pVector ) = up;	// copy

	PyObject *pretObj = PyTuple_Pack(3, pretObj1, pretObj2, pretObj3);
	
	Py_XDECREF(pretObj1);
	Py_XDECREF(pretObj2);
	Py_XDECREF(pretObj3);

	return pretObj;
}

//-----------------------------------------------------------------------------
// Interpolate a Catmull-Rom spline.
// t is a [0,1] value and interpolates a curve between p2 and p3.
// takes p1,p2,p3,p4 vectors and float t, returns output vector.
//-----------------------------------------------------------------------------
static PyObject *CatmullRomSpline( PyObject *self, PyObject *args )
{
	// UNDONE: use interpolatortypes.cpp types instead of a single interpolator here

	PyObject *pyv1,*pyv2,*pyv3,*pyv4;
	Vector p1,p2,p3,p4;
	float t;

	int ret = PyArg_ParseTuple(args, "|OOOOf", &pyv1, &pyv2, &pyv3, &pyv4, &t );
	if ( !ret )
	{
		DevMsg("vpython.cpp: invalid params to Catmull_Rom_Spline, expected 4 vectors and 1 float!");
		return Py_BuildValue("i",0);
	}

	p1 = *((PyVectorInstance_t *)pyv1)->pVector;
	p2 = *((PyVectorInstance_t *)pyv2)->pVector;
	p3 = *((PyVectorInstance_t *)pyv3)->pVector;
	p4 = *((PyVectorInstance_t *)pyv4)->pVector;

	// create new PyTypeVector object - explicitly calls VectorConstructNew
	PyObject *pretObj = CreatePyVector( NULL );

	Vector output(0,0,0);

	Catmull_Rom_Spline(p1,p2,p3,p4,t,output);

	*( ((PyVectorInstance_t *)pretObj)->pVector ) = output;	// copy

	return pretObj;
}

//-----------------------------------------------------------------------------
// Purpose: basic hermite spline.  t = 0 returns p1, t = 1 returns p2, 
//			d1 and d2 are used to entry and exit slope of curve
// Input  : p1,p2,d1,d2, t, 
//-----------------------------------------------------------------------------
static PyObject *HermiteSpline( PyObject *self, PyObject *args )
{
	// UNDONE: use interpolatortypes.cpp types instead of a single interpolator here

	PyObject *pyv1,*pyv2,*pyv3,*pyv4;
	Vector p1,p2,d1,d2;
	float t;

	int ret = PyArg_ParseTuple(args, "|OOOOf", &pyv1, &pyv2, &pyv3, &pyv4, &t );
	if ( !ret )
	{
		DevMsg("vpython.cpp: invalid params to HermiteSpline, expected 4 vectors and 1 float!");
		return Py_BuildValue("i",0);
	}

	p1 = *((PyVectorInstance_t *)pyv1)->pVector;
	p2 = *((PyVectorInstance_t *)pyv2)->pVector;
	d1 = *((PyVectorInstance_t *)pyv3)->pVector;
	d2 = *((PyVectorInstance_t *)pyv4)->pVector;

	// create new PyTypeVector object - explicitly calls VectorConstructNew
	PyObject *pretObj = CreatePyVector( NULL );

	Vector output(0,0,0);

	Hermite_Spline(p1,p2,d1,d2,t,output);

	*( ((PyVectorInstance_t *)pretObj)->pVector ) = output;	// copy

	return pretObj;
}

static PyObject *HermiteSplineFloat( PyObject *self, PyObject *args )
{
	float p1,p2,d1,d2;
	float t;

	int ret = PyArg_ParseTuple(args, "|fffff", &p1,&p2,&d1,&d2, &t );
	if ( !ret )
	{
		DevMsg("vpython.cpp: invalid params to HermiteSplineFloat, expected 5 floats!");
		return 0;
	}

	float output = Hermite_Spline(p1,p2,d1,d2,t);

	return Py_BuildValue("f", output);
}

//---------------------------------------------------------------------------------
// 
//---------------------------------------------------------------------------------
static PyObject *ExactTime( PyObject *self, PyObject *args )
{
	return PyFloat_FromDouble( Plat_FloatTime() );
}


static PyMethodDef valvemethods[] = 
{
	{"vprint",(PyCFunction) vprint, METH_VARARGS, "Display a string on the Valve console."},
	{"__ReplaceClosures",(PyCFunction) ReplaceClosures, METH_VARARGS, "Reload a module."},
		
	{"VectorFromAngles",(PyCFunction) VectorFromAngles, METH_VARARGS, "Convert from Roll, Pitch, Yaw to Vector"},
	{"VectorsFromAngles",(PyCFunction) VectorsFromAngles, METH_VARARGS, "Convert from Roll, Pitch, Yaw to forward, right, up Vectors"},
	{"AnglesFromVector",(PyCFunction) AnglesFromVector, METH_VARARGS, "Convert Vector to Roll, Pitch, Yaw angles (Yaw always 0)"},
	{"ExactTime",(PyCFunction) ExactTime, METH_VARARGS, "Get accurate sub-frame time in seconds"},
	{"CatmullRomSpline",(PyCFunction) CatmullRomSpline, METH_VARARGS, "Interpolate along a Catmull Rom Spline. Takes p1,p2,p3,p4 vectors and t float fraction."},
	{"HermiteSpline",   (PyCFunction) HermiteSpline, METH_VARARGS, "Interpolate along a Hermite Spline. Takes p1,p2,d1,d2 vectors and t float fraction."},
	{"HermiteSplineFloat",(PyCFunction) HermiteSplineFloat, METH_VARARGS, "Interpolate along a Hermite Spline. Takes f1,f2,d1,d2 floats, and t float fraction."},

	{NULL, NULL, 0, NULL} // sentinel

};

//------------------------------------------------------------------------------
// UNUSED: Purpose: remove old module from sys.modules and decrement ref to module object.
// may provide either module object, or module name
//------------------------------------------------------------------------------
void UnloadModule( PyObject *pmodule, const char *pszModuleName )
{

	return;	// BUG: the name of the last module to run becomes '__main__' - must not unload main!

	const char *pszName = NULL;

	PyObject *pSysModules = PyImport_GetModuleDict( ); // borrowed ref

	if ( pmodule )
	{
		pszName = PyModule_GetFilename( pmodule );
		pszName = PyModule_GetName( pmodule );
		
	}
	else
	{
		Assert ( pszModuleName );
		pszName = pszModuleName;
	}

	if ( !pszName )
		return;		// scope module has no name

	PyObject *pPyKey = PyString_FromString( pszName );

	if ( PyDict_Contains( pSysModules, pPyKey) )
	{
		// remove module from dict
		PyObject *pOldModule = PyDict_GetItemString( pSysModules, pszName );
		PyDict_DelItemString( pSysModules, pszName );
		Py_XDECREF( pOldModule );
	}
	Py_XDECREF( pPyKey );

}


//------------------------------------------------------------------
// SINGLETON python interpreter - do not instantiate more than one
// (this module keeps a static global pointer to the interpreter instance.)
//------------------------------------------------------------------

class CPythonVM : public IScriptVM
{
public:
	CPythonVM(  )
	  :	m_iUniqueIdSerialNumber( 0 )
#ifndef VPYTHON_TEST
	    , developer( "developer" )
#else
	    , developer( "developer", "1" )
#endif
	{
		m_bInitialized = false;		// becomes true on all subsequent attempts to kill/restart the interpreter
		m_pRootScope = NULL;
		m_pValveScope = NULL;
		m_iMethodDef = 0;
		m_iClassDef = 0;

		m_debugObjCount = 0;

	}


	//-------------------------------------------------------------
	// Init the interpreter class - NOP if interp already active
	//-------------------------------------------------------------
	bool Init()
	{
		// g_pMemAlloc->CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_CHECK_CRT_DF );
		if ( VMInitFinalized() )
			return true;

		// Python makes use of mathlib, so we have to init it:
		MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f );

		// init static proxy table
		InitProxyTable();

		// start python interpreter installed on users machine. 
		// i.e. python25.dll should be in c:\windows\system32 or on dll search path.
		
		Py_Initialize();		// this is a NOP if already initialized

		if ( !Py_IsInitialized() )
		{
			DevWarning("CPythonVM.Init(): python interperter failed to initialize!");
			return false;
		}
		

		const char *pszVersion = Py_GetVersion();
		char buffer[64];
		V_strncpy(buffer,pszVersion, 6);
		buffer[5] = '\0';
		
		if ( V_strncmp(pszVersion, buffer, 5) )
		{
			// we link with python25.lib from version 2.5.1
			DevWarning("Python25.dll version mismatch: version %s loaded. Should be 2.5.1!\n", buffer);
		}

		// modify python's module search path in sys.path to point to current working directory + <gamename>/scripts/vscripts

		int ret;

		ret = PyRun_SimpleString(
			"import sys\n"
			"import os\n"
			"a,b = os.path.split(os.getcwd())\n"
			"c,gamename = os.path.split(a)\n"
			"p = os.path.join (os.getcwd(),gamename,'scripts','vscripts')\n"
			"sys.path.append( p )\n"
			 );

		Assert ( ret == 0 ); // make sure we could set up the python sys.path to the vscripts directory
	

		// save off the vscript directory - ex: U:\projects\sob\game\sob\scripts\vscripts

		char	szScriptPathTemp[MAX_PATH];
		char	szScriptPathTemp2[MAX_PATH];
		char	szDirectoryTemp[MAX_PATH];
		char	szGamename[MAX_PATH];

		// get current working directory 

		V_GetCurrentDirectory( szDirectoryTemp, sizeof( szDirectoryTemp ) ); // U:\projects\sob\game
		
		// get game name from current working directory

		V_ExtractFilePath( szDirectoryTemp, szGamename, sizeof(szGamename) ); // U:\projects\sob
		const char *pszGamename = V_UnqualifiedFileName( szGamename );

		V_ComposeFileName( szDirectoryTemp, pszGamename, szScriptPathTemp, sizeof( szScriptPathTemp ) );
		V_ComposeFileName( szScriptPathTemp, "scripts", szScriptPathTemp2, sizeof( szScriptPathTemp2 ) );
		V_ComposeFileName( szScriptPathTemp2, "vscripts", m_szScriptPath, sizeof( m_szScriptPath ) );
		

		// create a new global scope 'valve' module  - all valve classes, functions, instances are registered in this module.
		// NOTE: 'valve' must be imported by python modules that wish to access these c functions.
		
		PyObject *pPyValve;
	
		pPyValve = PyImport_AddModule("valve");
		PyModule_AddStringConstant(pPyValve, "__file__", "<synthetic>");
		
		PyObject *pmod = Py_InitModule("valve", valvemethods); 
		Assert ( pmod == pPyValve );

		// Redirect python stdout & stderr to Valve console
		ret = PyRun_SimpleString(
			"import valve\n"
			"import sys\n"
			"class StdoutCatcher:\n"
				"\tdef write(self, str):\n"
					"\t\tvalve.vprint(str)\n"
			"class StderrCatcher:\n"
				"\tdef write(self, str):\n"
					"\t\tvalve.vprint(str)\n"
			"sys.stdout = StdoutCatcher()\n"
			"sys.stderr = StderrCatcher()\n" );

		Assert ( ret == 0 );

		// create new valve module scope
		m_pValveScope = new CPyScope( );
		m_pValveScope->SetModuleObject( pPyValve );
		
		// get the main module object
		PyObject* pPyMain = PyImport_AddModule("__main__");

		// create new root scope from main's dictionary
		m_pRootScope = new CPyScope( );
		m_pRootScope->SetModuleObject( pPyMain );


		// load and run the init.py module in the global scope
		PyObject *pPyModule;
		PyObject *pystr = PyString_FromString( "init" );
		pPyModule = PyImport_Import( pystr );
		Py_XDECREF( pystr );
		
		PyPrintError();

		if ( !pPyModule )
		{
			DevWarning("CPythonVM.Init(): unable to load main module init.py - module not on PYTHONPATH (sys.path)?");
			return false;
		}

		m_TypeMap.Init( 256 );		// must be power of 2
		m_ClassMap.Init( 256 );		// must be power of 2

		RegisterVector( m_pValveScope->GetModule() );

		PyPrintError();

		return true;

	}

	//-------------------------------------------------------------
	// Called every frame with frame time
	//-------------------------------------------------------------
	bool Frame( float simTime )
	{
		// UNDONE: garbage collect periodically?
		// UNDONE: invoke a per-frame global function? ex: to run generators
#ifdef DEBUG_PY
		if (1)
		{
			// validate every instance object we've allocated
			for (int i=0; i < m_debugObjCount; i++)
			{
				if ( m_debugObjects[i] )
					AssertIsPyObject( (HSCRIPT) (m_debugObjects[i]) );
			}

		}
#endif // DEBUG_PY
		return false;
	}

	//-------------------------------------------------------------
	// Called on level load
	//-------------------------------------------------------------
	void Shutdown()
	{
		bool bGameExit = false;	 // UNDONE: need flag passed in - only kill interpreter data on game exit, not restart or level load

		if ( bGameExit )
		{
			if(1)
				Py_Finalize(); // interpreter shutdown - reliable only ONCE per game session. 

			// shut down the vm. called on restart, or game exit

			if (1)
			{
				ReleaseScope( (HSCRIPT)m_pRootScope );
				ReleaseScope( (HSCRIPT)m_pValveScope );
				m_TypeMap.Purge();
				m_ClassMap.Purge();

			}

		}

	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	ScriptLanguage_t GetLanguage()
	{
		return SL_PYTHON;
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	virtual const char *GetLanguageName()
	{
		return "Python";
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	virtual void AddSearchPath( const char *pszSearchPath )
	{

	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	bool ConnectDebugger()
	{
		// for debugging from visual studio into python, running pydev in eclipse, 
		// first launch the pydev debug server in eclipse, then 
		// run python code containing the following line:
		//
		//      import pydevd; pydevd.settrace()

		// for debuggin in Wing IDE, place 'import wingdbstub.py' in
		// the code module you wish to debug.

		return false;
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	void DisconnectDebugger()
	{

	}

	//-------------------------------------------------------------
	// run script text in the root scope
	//-------------------------------------------------------------
	ScriptStatus_t Run( const char *pszScript, bool bWait = true )
	{
		Assert( bWait );

		PyObject* pyret = PyRun_String( pszScript, Py_file_input, m_pRootScope->GetModuleDict(), m_pRootScope->GetModuleDict() ) ;
		
		PyPrintError();

		if ( pyret == NULL )
			return SCRIPT_ERROR;
		
		Py_XDECREF(pyret);
		return SCRIPT_DONE;

	}

	//-------------------------------------------------------------
	// run the compiled script in the given scope
	//-------------------------------------------------------------
	ScriptStatus_t Run( HSCRIPT hScript, HSCRIPT hScope = NULL, bool bWait = true )
	{
		return CPythonVM::ExecuteFunction( hScript, NULL, 0, NULL, hScope, bWait );
	}

	//-------------------------------------------------------------
	// run the compiled script in the root scope
	//-------------------------------------------------------------
	ScriptStatus_t Run( HSCRIPT hScript, bool bWait )
	{
		Assert( bWait );
		return CPythonVM::Run( hScript, (HSCRIPT)NULL, bWait );
	}

	//-------------------------------------------------------------
	// python auto-compiles modules, so just return a python string containing the module name, 
	// or a python function object 
	//
	//	pyszScript - script file contents - module content - ignored if pszId is given 
	//	pszId - module name with extension - modules live in scripts/vscripts
	//
	//-------------------------------------------------------------
	HSCRIPT CompileScript( const char *pszScript, const char *pszId = NULL )
	{
		
		if ( pszId )
		{
			// pszId is the module name with extension - required for python
			Assert(pszId != NULL);

			PyObject *pName;
			
			// strip module extension
			char buffer[1024];
			V_StripExtension( pszId, buffer, sizeof(buffer) );

			pName = PyString_FromString(buffer); // new ref
		
			return (HSCRIPT)pName;				 // return the name of the module to later import (and run). compilation is automatic.
		}
		else
		{
			// code string fixup:
			/* Replace any occurances of "\r\n?" in the input string with "\n".
			This converts DOS and Mac line endings to Unix line endings.
			Also append a trailing "\n" to be compatible with
			PyParser_SimpleParseFile(). Returns a new reference. */

			if ( !pszScript || !*pszScript )
			{
				DevWarning ( "Vscript: no script text passed to CompileScript - ignoring compilation!");
				return NULL;
			}

			char *buf;

			{
				char *q;
				const char *p = pszScript;
				
				if (!p)
					return NULL;

				/* one char extra for trailing \n and one for terminating \0 */
				buf = (char *)PyMem_Malloc( strlen(pszScript) + 2);
				if (buf == NULL) {
					PyErr_SetString(PyExc_MemoryError,
						"Python Source Compile - no memory to allocate conversion buffer!");
					return NULL;
				}
				/* replace "\r\n?" by "\n" */
				for (q = buf; *p != '\0'; p++) {
					if (*p == '\r') {
						*q++ = '\n';
						if (*(p + 1) == '\n')
							p++;
					}
					else
						*q++ = *p;
				}
				*q++ = '\n';  /* add trailing \n */
				*q = '\0';
			
			}

			// create a new python code object - we own the reference which we must release later
			// NOTE: must pass full path to source file to enable debugging in external IDE (such as Wing IDE Professional).
			char szFullPath[MAX_PATH];

			if ( pszId )
			{
				V_ComposeFileName( m_szScriptPath, pszId, szFullPath, sizeof(szFullPath) );
			}
			

			PyObject *pyCodeObject = Py_CompileString(buf, (pszId) ? szFullPath : "unnamed", Py_file_input);
			// PyObject *pyCodeObject = Py_CompileString(buf, "U:\\projects\\sob\\game\\sob\\scripts\\vscripts\\mapspawn.py", Py_file_input);

			PyPrintError();

			PyMem_Free(buf);

			return (HSCRIPT)pyCodeObject;		// return a code object - NULL on compile error
		}

	}

	//-------------------------------------------------------------
	// release code object, string object (module) or instance object
	//-------------------------------------------------------------
	void ReleaseScript( HSCRIPT hScript )
	{
		Assert( hScript );
		AssertIsPyObject( hScript );
		
		if ( !PyString_Check( (PyObject *)hScript ) && !PyCode_Check( (PyObject *)hScript ) )
		{
			AssertIsInstance( hScript );
		}

		Py_XDECREF((PyObject *)hScript);
	}


	//-------------------------------------------------------------
	// create an empty scope object - use SetModule and SetInstance
	// to create actual references to dictionaries.
	//-------------------------------------------------------------
	HSCRIPT CreateScope( const char *pszScope, HSCRIPT hParent = NULL )
	{
		CPyScope *pPyScope = new CPyScope();

		if ( hParent )
		{
			DevMsg( "Warning, Python script language ignoring hParent parameter for CreateScope!" );
		}

		return (HSCRIPT)pPyScope;
	}

	//-------------------------------------------------------------
	// delete the CPyScope object
	//-------------------------------------------------------------
	void ReleaseScope( HSCRIPT hScript )
	{
		AssertIsScope( hScript );

		if ( hScript )
			delete (CPyScope *)hScript;

	}

	//-------------------------------------------------------------
	// return a python PyObject* given a named variable in the given scope's instance object.
	// python object is a new reference if bAddRef is true, otherwise borrowed reference.
	//-------------------------------------------------------------
	PyObject* LookupObject( const char *pszObject, HSCRIPT hScope = NULL, bool bAddRef = true )
	{
		if ( hScope == INVALID_HSCRIPT )
			return NULL;

		PyObject *pGlobals = InstanceDictFromScope( hScope );

		// Return the object from globals dict using key. Return NULL if the key is not present.
		PyObject *pyobj;

		pyobj = PyDict_GetItemString( pGlobals, pszObject); // returns borrowed reference - we don't own this obj unless we increment the reference.
		
		PyPrintError();

		if ( pyobj == NULL)
			// key not in dict
			return NULL;

		if ( bAddRef )
			Py_XINCREF(pyobj);

		return pyobj;
	}

	//-------------------------------------------------------------
	// return a python function object - lookup function in MODULE scope dict - 
	// NOTE: this will only work correctly AFTER CompileScript and 
	// Run or ExecuteFunction is called on the script which holds the function.
	// NOTE: caller must call ReleaseFunction when finished with function!
	//-------------------------------------------------------------
	HSCRIPT LookupFunction( const char *pszFunction, HSCRIPT hScope = NULL )
	{
		// UNDONE: CBaseEntity calls this for EVERY think cycle - 
		// UNDONE: rewrite CBaseEntity to remember the think function handle!

		if ( hScope == INVALID_HSCRIPT )
			return NULL;

		PyObject *pGlobals;

		if ( hScope )
		{
			pGlobals = ((CPyScope *)hScope)->GetModuleDict();
		}
		else
		{
			pGlobals = m_pValveScope->GetModuleDict();	// lookup function in valve scope
		}

		if ( !pGlobals )
		{
			DevWarning("Vscript, vpython.cpp: LookupFunction - must first compile and run the script before you can lookup a function!");
			return NULL;
		}

		PyObject *pFunc = PyDict_GetItemString(pGlobals, pszFunction); // borrowed reference
		
		// set up function object with scope

		PyPrintError();

		if (pFunc != NULL && PyCallable_Check(pFunc) )
		{
			Py_INCREF( pFunc );				// inc ref so obj can be released later
			DEBUG_FUNCCOUNT++;
			Assert(DEBUG_FUNCCOUNT < 1000);  // if this fails, server is likely not freeing function handles (leaking each frame)
			return (HSCRIPT )pFunc;
		}
		else
		{
			return NULL;
		}
	}

	//-------------------------------------------------------------
	// decrement our reference to the function handle
	//-------------------------------------------------------------
	void ReleaseFunction( HSCRIPT hScript )
	{
		AssertIsPyObject( hScript );
		Py_XDECREF((PyObject *)hScript);
		DEBUG_FUNCCOUNT--;
		//ReleaseScriptObject( hScript );
	}

	//-------------------------------------------------------------
	// given a handle to a python function, a compiled code object,
	// or a module name, execute the object.  If given a module name,
	// also sets the module object in the hScope for subsequent
	// execution of functions in module scope. Caller must free
	// variant args & any newly returned variant.
	//-------------------------------------------------------------
	ScriptStatus_t ExecuteFunction( HSCRIPT hFunction, ScriptVariant_t *pArgs, int nArgs, ScriptVariant_t *pReturn, HSCRIPT hScope = NULL, bool bWait = true )
	{
		if ( hScope == INVALID_HSCRIPT || !hFunction)
		{
			// DevWarning( "Invalid scope handed to script VM\n" );
			return SCRIPT_ERROR;
		}

		AssertIsScope( hScope );
		AssertIsPyObject( hFunction );
		Assert ( bWait );

		PyObject *pGlobals = ModuleDictFromScope( hScope );

		// get type of hfunction - may be string (module), function, or compiled code object

		if ( PyString_Check( (PyObject *)hFunction) )
		{
			// Import a module and set the hScope module variable.
			// NOTE: this is required before lookup/executeFunction calls into the module.
			// hFunction is a module name - may be returned by CompileScript
			// equivalent to python "import modulename"
			// note: python auto compiles the module if it is out of date, and saves the 
			// binary image to disk for faster load times (no compile) on future import calls. 
	
			ScriptStatus_t result = SCRIPT_DONE;
			PyObject *pModule;

			// char *pszModuleName = PyString_AsString( (PyObject *)hFunction ); // internal pointer

			// import and run module - always runs in global scope
			// PyObject *pystr = PyString_FromString( pszModuleName );
			pModule = PyImport_Import( (PyObject *)hFunction );
			// Py_XDECREF( pystr );
			
			// set the module object in the scope.
			if ( hScope )
				( (CPyScope *)hScope )->SetModuleObject( pModule );

			Py_XDECREF(pModule);

			
			PyPrintError();

			if ( !pModule )
				result = SCRIPT_ERROR;

			return result;	
		}
		else if ( PyFunction_Check( (PyObject *)hFunction) || PyCallable_Check((PyObject *)hFunction) )
		{
			// Run a function in a module:
			// hFunction is a python function object.
			// NOTE: hScope is ignored - it was associated with the function object during function lookup (function's module)
			// NOTE: first argument is always 'self' of the calling entity, even if NULL

			PyObject *pPyArgs = PyTuple_New(nArgs+1);
			PyObject *pValue;
			int i;
			
			// first argument is always 'self' ,even if NULL
			PyObject *pSelf = NULL;
			if ( hScope )
			{
				pSelf = ((CPyScope *)hScope)->GetInstanceObject();
			}

			if ( !pSelf )
				pSelf = Py_None;
			Py_XINCREF( pSelf );
			PyTuple_SetItem(pPyArgs, 0, pSelf); // steals ref to pSelf

			for (i = 0; i < nArgs; ++i) 
			{
				pValue = ConvertToPyObject( pArgs[i], true ); // new ref

				// pValue reference stolen here: tuple owns the objects now
				PyTuple_SetItem(pPyArgs, i+1, pValue);
			}

			PyObject *pPyReturn = PyObject_CallObject((PyObject *)hFunction, pPyArgs);
			Py_DECREF(pPyArgs); // release tuple and contents
			
			PyPrintError();

			if ( pPyReturn == NULL ) 
			{
				// call failed
				return SCRIPT_ERROR;
			}
			
			bool bFreeobj = false;

			if ( pReturn )
			{
				bFreeobj = ConvertToVariant( pPyReturn, pReturn ); // caller must free this
			}

			Py_XDECREF(pPyReturn);

			return SCRIPT_DONE;	
		}
		else
		{
			// Run compiled code in a module:
			// assume hFunction is a compiled code object

			PyObject *pValue;
			PyObject *rgpyArgs[31];
			PyObject *pPyReturn;

			Assert ( nArgs < 32 );
			
			// first argument is always 'self' ,even if NULL
			PyObject *pSelf = ((CPyScope *)hScope)->GetInstanceObject();
			if ( !pSelf )
				pSelf = Py_None;
			Py_XINCREF( pSelf );
			rgpyArgs[0] = pSelf; // arg steals ref to pSelf

			int i;

			for (i = 0; i < nArgs; i++) 
			{
				pValue = ConvertToPyObject( pArgs[i], true );	// create new python objects with new refs
				rgpyArgs[i+1] = pValue;
			}

			if (0)
			{
				char *pszdebug = PyString_AsString( ((PyCodeObject *)hFunction)->co_filename );				
				char buffer[1024];
				V_StripExtension( pszdebug, buffer, sizeof(buffer) );

				//Py_XDECREF( ((PyCodeObject *)hFunction)->co_filename );

				//((PyCodeObject *)hFunction)->co_filename = PyString_FromString( buffer );
				
				PyObject *pystr = PyString_FromString( buffer );
				PyObject *pmodule = PyImport_Import( pystr );
				Py_XDECREF( pystr );

				// BUG: this executes in the module's scope, which is not per-instance.

				// PyObject *pmodule = PyImport_ExecCodeModule( buffer, (PyObject *)hFunction );
				Py_XDECREF( pmodule );

				pPyReturn = Py_None;
				Py_INCREF( pPyReturn );
			}
			else
			{
				// NOTE: This function will actually
				// run the code within the given scope dictionary - must be a module-level dictionary.
				pPyReturn = PyEval_EvalCodeEx((PyCodeObject *)hFunction,
					pGlobals,			//PyObject *globals,
					pGlobals,			//PyObject *locals,
					rgpyArgs, nArgs,	//PyObject **args, int argc,
					NULL, 0,			//PyObject **kwds, int kwdc,
					NULL, 0,			//PyObject **defs, int defc,
					NULL);				//PyObject *closure

			}

			// release arg objects
			for (i = 0; i < nArgs; i++)
				Py_XDECREF(rgpyArgs[i]);

			if ( pPyReturn == NULL ) 
			{
				// call failed
				PyPrintError();
				return SCRIPT_ERROR;
			}
			bool bFreeobj = false;

			if ( pReturn )
			{
				bFreeobj = ConvertToVariant( pPyReturn, pReturn ); // caller must free this
			}

			Py_XDECREF(pPyReturn);

			return SCRIPT_DONE;	
		}

		// invalid hFunction 
		return SCRIPT_ERROR;
	}

	//-------------------------------------------------------------
	// register a new function so python code can call it
	//-------------------------------------------------------------
	void RegisterFunction( ScriptFunctionBinding_t *pScriptFunction )
	{
		// if ( VMInitFinalized() )
		//	return;

		RegisterFunctionGuts( pScriptFunction );

		// NOTE: DEFINE_SCRIPTFUNC and ScripRegisterFunction macros eventually call RegisterFunction.
		// Templates automatically create the ScriptFunctionBinding_t.
		
		// Following is a summary of the Template expansion for ScriptRegisterFunction(Named) which 
		// builds the ScriptFunctionBinding_t in place and then calls ResisterFunction with it.
		// This scheme effectively uses C++ compile-time templates to implement function introspection:
/*
		ScriptRegisterFunctionNamed( g_pScriptVM, ScriptCreateSceneEntity, "CreateSceneEntity", "Create a scene entity to play the specified scene." );
		
		#define ScriptRegisterFunctionNamed( pVM, func, scriptName, description )
			
			static ScriptFunctionBinding_t binding; 
			binding.m_desc.m_pszDescription = description; 
			binding.m_desc.m_Parameters.RemoveAll(); 
			
			ScriptInitFunctionBindingNamed( &binding, func, scriptName ); 
			
				#define ScriptInitFunctionBindingNamed( pScriptFunction, func, scriptName )
			
				ScriptInitFuncDescriptorNamed( (&(pScriptFunction)->m_desc), func, scriptName );
				
				#define ScriptInitFuncDescriptorNamed( pDesc, func, scriptName )
					(pDesc)->m_pszScriptName = scriptName; 
					(pDesc)->m_pszFunction = #func; 
					ScriptDeduceFunctionSignature( pDesc, &func );	
						// this is a complex macro which, at compile time, 
						// inlines the code to fill out the remaing fields in the 
						// pDesc structure with the appropriate arg types and return types
						// for the given function.
				
				(pScriptFunction)->m_pfnBinding = ScriptCreateBinding( &func ); // uses functors to create the binding function
				(pScriptFunction)->m_pFunction = (void *)&func; 
			
			pVM->RegisterFunction( &binding );
		}
*/

	}

	//-------------------------------------------------------------
	// create a new python type object encapsulating the class - 
	// NOTE: must subsequently call PyType_Ready to finalize the class
	//-------------------------------------------------------------
	PyTypeObject *CreateClass( ScriptClassDesc_t *pDesc )
	{
		// python class template
		static PyTypeObject scriptClassType = {		
			PyObject_HEAD_INIT(NULL)	/* type type */ // set up by PyType_Ready() call later
			0,							/*ob_size*/
			0,							/*tp_name*/
			sizeof(scriptClassInstance_t),    /*tp_basicsize*/
			0,							/*tp_itemsize*/
			0,							/*tp_dealloc*/		// consider
			0,							/*tp_print*/
			0,							/*tp_getattr*/
			0,							/*tp_setattr*/
			0,							/*tp_compare*/
			(reprfunc)InstanceToString,	/*tp_repr*/			// consider
			0,							/*tp_as_number*/
			0,							/*tp_as_sequence*/
			0,							/*tp_as_mapping*/
			0,							/*tp_hash */
			0,							/*tp_call*/
			0,							/*tp_str*/
			PyObject_GenericGetAttr,	/*tp_getattro*/
			PyObject_GenericSetAttr,	/*tp_setattro*/
			0,							/*tp_as_buffer*/
			Py_TPFLAGS_DEFAULT, // | Py_TPFLAGS_BASETYPE, /*tp_flags*/	// allow subclassing
			0,							/* tp_doc */
			0,							/* tp_traverse */
			0,							/* tp_clear */
			0,							/* tp_richcompare */
			0,							/* tp_weaklistoffset */
			0,							/* tp_iter */
			0,							/* tp_iternext */
			0,							/* tp_methods */
			0,							/* tp_members */
			0,							/* tp_getset */
			0,							/* tp_base */			// base class type object
			0,							/* tp_dict */
			0,							/* tp_descr_get */
			0,							/* tp_descr_set */
			offsetof(scriptClassInstance_t, pDict),	// tp_dictoffset  - used by generic getattr,setattr 				
			0,							/* tp_init */		// consider
			PyType_GenericAlloc,		/* tp_alloc */
			PyType_GenericNew,			/* tp_new */		// consider
			PyObject_Del,				/* tp_free */

		};
		
		// build a new scriptClassType for each 'CreateClass' call
		PyTypeObject *pnewtype = (PyTypeObject *) PyMem_Malloc( sizeof(PyTypeObject) ); //new PyTypeObject; 

		if ( !pnewtype )
		{
			// interperter out of memory
			Assert( false );
			return NULL;
		}
		// track it so we can free it later
		Assert (m_iClassDef < MAX_VALVE_CLASSES_EXPORTED );
		m_rgpClassDefs[m_iClassDef++] = pnewtype;

		// allow mapping between PyTypeObject (accessed from pSelf->ob_type ptr) to ScriptClassDesc_t pDesc.

		// init the pnewtype with the static template
		V_memcpy( pnewtype, &scriptClassType, sizeof(PyTypeObject) );

		pnewtype->tp_doc = pDesc->m_pszDescription;
		pnewtype->tp_name = pDesc->m_pszScriptName;	// BUG: prepend "valve." or pickling is impossible
		
		// if base class given, make sure base class type already defined in root scope
		// then hook it up to our type
		if ( pDesc->m_pBaseDesc )
		{
			PyObject *pdict = m_pValveScope->GetModuleDict();
			if ( PyDict_GetItemString( pdict, pDesc->m_pBaseDesc->m_pszScriptName ) == NULL )
			{
				Assert( false ); // base class should have been pre registered in 'valve' module
				return NULL;
			}

			// lookup the corresponding type object
			PyTypeObject *pbasetype = PyTypeFromDesc( pDesc->m_pBaseDesc );
			if ( !pbasetype )
			{
				Assert ( false );
				return NULL;
			}

			pnewtype->tp_base = pbasetype;
		}
		
		// set up constructor and destructor

		// DEALLOCATION CHAIN:  Py_XDECREF -> tp_dealloc (free any local allocations) -> tp_free (free python object allocation)

		// ALLOCATION CHAIN:  tp_new, tp_alloc (create empty python object) -> tp_init (set up local data)


		pnewtype->tp_new =			PyType_GenericNew;		 // create new uninitialized object (just calls tp_alloc) // BUG: doesn't call tp_init!
		pnewtype->tp_init =			(initproc)CPythonVM::InitInstance; // same as __init__ for class - init the context associated with the object
		pnewtype->tp_alloc =		PyType_GenericAlloc;	 // alloc space for a python object

		pnewtype->tp_free =			PyObject_Del;			 // delete the python object
		pnewtype->tp_dealloc =		(destructor)CPythonVM::FreeInstance; // called when ref count drops to 0 - release any memory held by object 
															 // (ie: call destructor on underlaying valve object if object was allocated from python )

		// UNDONE: implement these additional callbacks
		//sq_pushstring( m_hVM, "_tostring", -1 );
		//sq_newclosure( m_hVM, &InstanceToString, 0 );
		//sq_createslot( m_hVM, -3 );

		//sq_pushstring( m_hVM, "IsValid", -1 );
		//sq_newclosure( m_hVM, &InstanceIsValid, 0 );
		//sq_createslot( m_hVM, -3 );

		
		// register all methods - create tp_methods table of pnewtype
		
		int count = pDesc->m_FunctionBindings.Count();
		
		if ( count )
		{
			// create an array large enough for all method defs + null semaphore
			PyMethodDef *pmethods = (PyMethodDef *) PyMem_Malloc( sizeof(PyMethodDef) * (count + 1) ); // new PyMethodDef[count+1];
			PyMethodDef *pm;

			if ( !pmethods )
			{
				// interperter out of memory
				Assert ( false );
				return NULL;
			}
			Assert (m_iMethodDef < MAX_VALVE_FUNCTIONS_EXPORTED );
			m_rgpMethodDefs[m_iMethodDef++] = pmethods;
			
			// TEST1:m_pMethodDefs.AddToTail( pmethods );
			int i;
			for ( i = 0; i < count; i++ )
			{
				ScriptFunctionBinding_t *pScriptFunction = &(pDesc->m_FunctionBindings[i]);

				pm = &(pmethods[i]);

				// fill python method def

				pm->ml_name = pScriptFunction->m_desc.m_pszScriptName;
				pm->ml_flags = METH_VARARGS;
				pm->ml_doc = pScriptFunction->m_desc.m_pszDescription;
				
				int proxyId = GetNewProxyId();
				SetProxyBinding( proxyId, pScriptFunction );

				// the function/method callback chain - python calls Translate_XXX -> calls TranslateCall -> calls binding function -> actual function.															
				pm->ml_meth = GetProxyFunction( proxyId );
			}

			// set null semaphore at end of methods
			pm = &(pmethods[i]);
			pm->ml_name = NULL;
			pm->ml_meth = NULL;
			pm->ml_flags = 0;
			pm->ml_doc = NULL;
			
			pnewtype->tp_methods = pmethods;
		}

		return pnewtype;

	}
	//-------------------------------------------------------------
	// create new python type object for this class and include 
	// ScriptClassDesc_t in the user type data.
	//-------------------------------------------------------------
	bool RegisterClass( ScriptClassDesc_t *pClassDesc )
	{
		// if ( VMInitFinalized() )
		// 	return true;

		PyObject *valveModule = m_pValveScope->GetModule();
		PyObject *pdict = m_pValveScope->GetModuleDict();

		if ( PyDict_GetItemString( pdict, pClassDesc->m_pszScriptName ) != NULL )
			return true; // already registered

		COMPILE_TIME_ASSERT( sizeof(pClassDesc) == sizeof(int) );
		if ( PyTypeFromDesc( pClassDesc ) )
		{
			return true;
		}

		// register base class first
		if ( pClassDesc->m_pBaseDesc )
		{
			CPythonVM::RegisterClass( pClassDesc->m_pBaseDesc );
		}

		PyTypeObject *pnewtype = CreateClass( pClassDesc );
		
		if ( pnewtype == NULL )
			return false;
		
		// finalize the new python type
		if (PyType_Ready( pnewtype ) < 0)
			return false;

		// add the new class type to the 'valve' module
		Py_INCREF( pnewtype );

		PyModule_AddObject( valveModule, pClassDesc->m_pszScriptName, (PyObject *)pnewtype );

		m_TypeMap.Insert( (int)pClassDesc, pnewtype ); // mapping from pClassDesc to PyTypeObject needed for RegisterInstance
		
		m_ClassMap.Insert( (int)pnewtype, pClassDesc ); // mapping from PyTypeObject to pClassDesc needed for InitInstance constructor callback

		if ( PyDict_GetItemString( pdict, pClassDesc->m_pszScriptName ) == NULL )
		{
			Assert ( false );
			return false; // class wasn't added to valve module!
		}

		PyPrintError();

		return true;

	}


	//-------------------------------------------------------------
	// auto-register instance class, and return script instance obj:
	// such as a cbaseentity instance, Entities iterator list etc
	//-------------------------------------------------------------
	HSCRIPT RegisterInstance( ScriptClassDesc_t *pDesc, void *pInstance )
	{

		// auto-create the instance's class if not already created
		if ( !CPythonVM::RegisterClass( pDesc ) )
		{
			return NULL;
		}

		// create a new python instance - this winds up calling InitInstance to set up the instanceContext

		// x = CEntity( pInstance )  # python code 
		
		PyObject *pcallable = PyDict_GetItemString( m_pValveScope->GetModuleDict(), pDesc->m_pszScriptName ); // borrowed ref
		scriptClassInstance_t *ppyobj = NULL;

		if ( pcallable && PyCallable_Check( pcallable ) )
		{
			// create new script object
			ppyobj = (scriptClassInstance_t *)PyObject_CallObject( pcallable, NULL);  // new ref
			ppyobj->typeTag = TYPETAG_INSTANCE;

			// make sure type name matches
			if ( ppyobj->ob_type->tp_name != pDesc->m_pszScriptName )
			{
				Assert ( false );
				return NULL;
			}

			// fill in the instance context for the new object

			ppyobj->instanceContext.pInstance = pInstance;
			ppyobj->instanceContext.pPyName = NULL;
			ppyobj->instanceContext.pClassDesc = pDesc;
			ppyobj->pDict = PyDict_New();
		}

		PyPrintError();

		return (HSCRIPT)ppyobj;
		
	}

	//-------------------------------------------------------------
	// set a unique string in the instance object. 
	//-------------------------------------------------------------
	void SetInstanceUniqeId( HSCRIPT hInstance, const char *pszId )
	{
		AssertIsInstance( hInstance );

		// make sure this is an object type we have defined
		if ( pszId &&  pDescFromPyObj( (PyObject *)hInstance )  )
		{
			((scriptClassInstance_t *)hInstance)->instanceContext.pPyName = PyString_FromString( pszId );
		}

	}

	//-------------------------------------------------------------
	// set instance pointer to valve server object to null 
	// and release the python object
	//-------------------------------------------------------------
	void RemoveInstance( HSCRIPT hInstance )
	{
		AssertIsInstance( hInstance );

		// make sure this is an object type we have defined
		if ( pDescFromPyObj( (PyObject *)hInstance ) )
		{
			ReleaseScriptObject( hInstance );
			debugRemoveTrackedObject( (PyObject *) hInstance );
		}
	}

	//-------------------------------------------------------------
	// Return server-side object from python object
	//-------------------------------------------------------------
	void *GetInstanceValue( HSCRIPT hInstance, ScriptClassDesc_t *pExpectedType )
	{
		AssertIsInstance( hInstance );

		// make sure this is an object type we have defined
		if ( pDescFromPyObj( (PyObject *)hInstance ) )
		{
			InstanceContext_t *pContext = &( ((scriptClassInstance_t *)hInstance)->instanceContext );

			if ( !pExpectedType || pContext->pClassDesc == pExpectedType || IsClassDerivedFrom( pContext->pClassDesc, pExpectedType ) )
				return pContext->pInstance;
		}
		return NULL;
	}

	//-------------------------------------------------------------
	// return true if derived class derives from base class.
	//-------------------------------------------------------------
	bool IsClassDerivedFrom( const ScriptClassDesc_t *pDerivedClass, const ScriptClassDesc_t *pBaseClass )
	{
		const ScriptClassDesc_t* pType = pDerivedClass->m_pBaseDesc;
		while ( pType )
		{
			if ( pType == pBaseClass )
				return true;

			pType = pType->m_pBaseDesc;
		}
		
		return false;
	}

	bool GenerateUniqueKey( const char *pszRoot, char *pBuf, int nBufSize )
	{
		Assert( V_strlen(pszRoot) + 32 <= nBufSize );
		Q_snprintf( pBuf, nBufSize, "%x%I64x%s", RandomInt(0, 0xfff), m_iUniqueIdSerialNumber++, pszRoot ); // random to limit key compare when serial number gets large
		return true;
	}

	//-------------------------------------------------------------
	// return true if key has a value in scope's instance object.
	// CONSIDER: if no instance object, try module object, if no module
	// object, try global scope.
	//-------------------------------------------------------------
	virtual bool ValueExists(  HSCRIPT hScope, const char *pszKey )
	{
		if ( hScope == INVALID_HSCRIPT )
			return false;

		AssertIsScope( hScope );
		PyObject *pGlobals = InstanceDictFromScope( hScope );
		
		if ( !pGlobals )
			// pGlobals = ModuleDictFromScope( hScope );
			return false;

		Assert ( pGlobals );
		if ( PyObject_HasAttrString( pGlobals, pszKey ) )
			return true;

		return false;
	}

	bool SetValue( HSCRIPT hScope, const char *pszKey, const char *pszValue )
	{
		return SetValueInternal( hScope, pszKey, NULL, pszValue );
	}

	bool SetValue( HSCRIPT hScope, const char *pszKey, const ScriptVariant_t &value )
	{
		return SetValueInternal( hScope, pszKey, value, NULL );
	}

	//-------------------------------------------------------------
	// set key:value pair in given scope's instance object dict.
	// if hScope is NULL, set data in global scope. caller still owns
	// object if is an Hscript (i.e. must free it).
	//
	// !!!NOTE!!!: data is not actually flushed into the instance object 
	// until a 'self' instance value is set in the scope. At that 
	// time, all previously set data will be visible in python
	// on the instance object.  In python code, the instance is named 'self'
	// and is passed as the first param to all function calls.
	//-------------------------------------------------------------
	bool SetValueInternal( HSCRIPT hScope, const char *pszKey, const ScriptVariant_t &value, const char *pszValue )
	{
		if ( hScope == INVALID_HSCRIPT )
			return false;

		AssertIsScope( hScope );
		CPyScope *pScope = (CPyScope *)hScope;
		PyObject *pyobj; 

		if ( pszValue )
		{
			pyobj = PyString_FromString( pszValue ); // new reference
		}
		else
		{
			// trap special 'self' instance - save it in the CPyScope object
			if ( value.m_type == FIELD_HSCRIPT )
			{ 
				// instance object...

				if ( !V_strcmp(pszKey, "self") )
				{
					// save instance in scope
					PyObject *pyobj = ConvertToPyObject( value, true ); // new ref
					pScope->SetInstanceObject( pyobj );			  // inc ref to object
					Py_XDECREF( pyobj );						  // for hscript objects, caller owns the hscript within the variant, not us

					// copy scope temp dict into instance dict.
					pScope->TransferTempDictToInstance();
					return true;
				}
			}
			// set a new copy of the value in instance's dict
			pyobj = ConvertToPyObject( value, true );	

			if ( value.m_type == FIELD_HSCRIPT)
			{
				Py_XDECREF( pyobj );						// for hscript objects, caller owns the hscript within the variant, not us
				debugTrackObject( pyobj );
			}	
		}


		PyObject *pGlobals;

		if ( !hScope )
		{
			// set data in valve module scope
			pGlobals = m_pValveScope->GetModuleDict();

		}
		else
		{
			// place all other data in scope's instance object dict
			pGlobals = InstanceDictFromScope( hScope );
		}
		
		if ( !pGlobals )
		{
			// instance object 'self' not yet set up in scope - add to temporary dict
			pScope->SetTempDictValue( pszKey, pyobj );
			return true;
		}
		else
		{
			int ret = PyDict_SetItemString( pGlobals, pszKey, pyobj); // dict does not own the objects in it - but will decref old obj when overwritten.

			if ( ret == -1 )
				return false;

			return true;
		}
	}

	//------------------------------------------------------------------------------
	// Purpose: create a new python dictionary object and return wrapped in a 
	// scriptVariant HScript object.
	//----------------------------------------------------------------------------
	void CreateTable( ScriptVariant_t &Table )
	{
		PyObject *pdict = PyDict_New();
		ConvertToVariant( pdict, &Table );
		return;
	}

	//------------------------------------------------------------------------------
	// Purpose: returns the number of elements in the scope's instance object dict
	// Input  : hScope - the table
	// Output : returns the number of elements in the table
	//------------------------------------------------------------------------------
	int	GetNumTableEntries( HSCRIPT hScope )
	{
		if ( hScope == INVALID_HSCRIPT )
			return 0;
		AssertIsScope( hScope );
		PyObject *pGlobals = InstanceDictFromScope( hScope );

		int ret = (int) PyDict_Size( pGlobals ); 

		return ret;
	}


	//------------------------------------------------------------------------------
	// Purpose: Gets a key / value pair from the instance dictionary
	// Input  : hScope - the instance dictionary 
	//			nInterator - the current location inside of the table.  NOTE this is nota linear representation
	// Output : returns the next iterator spot, otherwise -1 if error or end of table
	//			pKey - the key entry
	//			pValue - the value entry
	//------------------------------------------------------------------------------
	int GetKeyValue( HSCRIPT hScope, int nIterator, ScriptVariant_t *pKey, ScriptVariant_t *pValue )
	{
		if ( hScope == INVALID_HSCRIPT )
			return -1;

		PyObject *pGlobals = InstanceDictFromScope( hScope );

		Py_ssize_t nNextIterator = (Py_ssize_t)nIterator;

		PyObject *pPyKey, *pPyValue;
		int ret;

		ret = PyDict_Next( pGlobals, &nNextIterator, &pPyKey, &pPyValue );
		if ( !ret )
		{
			// iteration complete
			return -1;
		}

		ConvertToVariant( pPyKey, pKey );
		ConvertToVariant( pPyValue, pValue );

		return (int)nNextIterator;

	}


	//-------------------------------------------------------------
	// lookup a key value in the scope's instance dictionary
	//-------------------------------------------------------------
	bool GetValue( HSCRIPT hScope, const char *pszKey, ScriptVariant_t *pValue )
	{
		PyObject *result = LookupObject( pszKey, hScope, false );
		if ( ConvertToVariant( result, pValue ) && (result != Py_None) )
		{
			return true;
		}
		return false;
	}

	//-------------------------------------------------------------
	// remove key from the given hscope - search instance scope,
	// then module scope.
	//-------------------------------------------------------------
	bool ClearValue( HSCRIPT hScope, const char *pszKey )
	{
		if ( hScope == INVALID_HSCRIPT )
			return false;


		PyObject *pInstGlobals = NULL;
		PyObject *pModuleGlobals = NULL;
		int res;
		
		if ( !hScope )
		{
			// clear data in valve module scope
			pModuleGlobals = m_pValveScope->GetModuleDict();
		}
		else
		{
			pInstGlobals = ((CPyScope *)hScope)->GetInstanceDict();
			pModuleGlobals = ((CPyScope *)hScope)->GetModuleDict();
		}

		PyObject *pystr = PyString_FromString( pszKey );

		if ( pInstGlobals && PyDict_Contains( pInstGlobals, pystr ) )
		{
			res = PyDict_DelItemString( pInstGlobals, pszKey);
			Py_XDECREF( pystr);
			if ( res == -1 )
				return false;
			return true;
		}
		
		if ( pModuleGlobals && PyDict_Contains( pModuleGlobals, pystr ) )
		{
			res = PyDict_DelItemString( pModuleGlobals, pszKey);
			Py_XDECREF( pystr);
			if ( res == -1 )
				return false;
			return true;
		}

		return false;
	}

	//-------------------------------------------------------------
	// release resources saved in script variant
	//-------------------------------------------------------------
	void ReleaseValue( ScriptVariant_t &value )
	{
		if ( value.m_flags & SV_FREE )
			DEBUG_VARIANTCOUNT--;

		if ( value.m_type == FIELD_HSCRIPT )
		{
			// drop our ref count to the python object
			Py_XDECREF((PyObject *)value.m_hScript);
		}
		else
		{
			value.Free();
		}
		value.m_type = FIELD_VOID;
	}


	bool RaiseException( const char *pszExceptionText )
	{
		PyErr_SetString(PyExc_Exception, pszExceptionText);
		return true;
	}


	virtual void DumpState()
	{
		// UNDONE:
		/*struct CIterator : public CSQStateIterator
		{
			CIterator( HPYTHONVM hVM )
			{
				indent = 0;
				m_hVM = hVM;
				m_bKey = false;
			}

			void Indent()
			{
				for ( int i = 0; i < indent; i++)
				{
					Msg( "  " );
				}
			}

			virtual void PsuedoKey( const char *pszPsuedoKey )
			{
				Indent();
				Msg( "%s: ", pszPsuedoKey );
				m_bKey = true;
			}

			virtual void Key( SQObjectPtr &key )
			{
				Indent();
				SQObjectPtr res;
				m_hVM->ToString( key, res );
				Msg( "%s: ", res._unVal.pString->_val );
				m_bKey = true;
			}

			virtual void Value( SQObjectPtr &value )
			{
				if ( !m_bKey )
				{
					Indent();
				}
				m_bKey = false;
				SQObjectPtr res;
				m_hVM->ToString( value, res );
				if ( ISREFCOUNTED(value._type) )
					Msg( "%s [%d]\n", res._unVal.pString->_val, value._unVal.pRefCounted->_uiRef );
				else
					Msg( "%s\n", res._unVal.pString->_val );
			}

			virtual bool BeginContained()
			{
				if ( m_bKey )
				{
					Msg( "\n" );
				}
				m_bKey = false;
				Indent();
				Msg( "{\n" );
				indent++;
				return true;
			}

			virtual void EndContained()
			{
				indent--;
				Indent();
				Msg( "}\n" );
			}

			int indent;
			HPYTHONVM m_hVM;
			bool m_bKey;
		};

		CIterator iter( m_hVM );
		m_hVM->_sharedstate->Iterate( m_hVM, &iter );*/
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	void WriteState( CUtlBuffer *pBuffer)
	{
		// UNDONE:
//#ifdef VPYTHON_DEBUG_SERIALIZATION
//		Msg( "BEGIN WRITE\n" );
//#endif
//		m_pBuffer = pBuffer;
//		sq_collectgarbage( m_hVM );
//
//		m_pBuffer->PutInt( SAVEVERSION );
//		m_pBuffer->PutInt64( m_iUniqueIdSerialNumber );
//		WriteVM( m_hVM );
//
//		m_pBuffer = NULL;
//
//		SQCollectable *t = m_hVM->_sharedstate->_gc_chain;
//		while(t) 
//		{
//			t->UnMark();
//			t = t->_next;
//		}
//
//		m_PtrMap.Purge();
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	void ReadState( CUtlBuffer *pBuffer )
	{
		// UNDONE:
//#ifdef VPYTHON_DEBUG_SERIALIZATION
//#ifdef VPYTHON_DEBUG_SERIALIZATION_HEAPCHK
//		g_pMemAlloc->CrtCheckMemory();
//		int flags = g_pMemAlloc->CrtSetDbgFlag( _CRTDBG_REPORT_FLAG );
//		g_pMemAlloc->CrtSetDbgFlag( flags | _CRTDBG_DELAY_FREE_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_CHECK_CRT_DF );
//#endif
//		Msg( "BEGIN READ\n" );
//#endif
//
//		if ( pBuffer->GetInt() != SAVEVERSION )
//		{
//			DevMsg( "Incompatible script version\n" );
//			return;
//		}
//		sq_collectgarbage( m_hVM );
//		m_hVM->_sharedstate->_gc_disableDepth++;
//		m_pBuffer = pBuffer;
//		int64 uniqueIdSerialNumber = m_pBuffer->GetInt64();
//		m_iUniqueIdSerialNumber = max( m_iUniqueIdSerialNumber, uniqueIdSerialNumber );
//		Verify( pBuffer->GetInt() == OT_THREAD );
//		m_PtrMap.Insert( pBuffer->GetPtr(), m_hVM );
//		ReadVM( m_hVM );
//		m_pBuffer = NULL;
//		m_PtrMap.Purge();
//		m_hVM->_sharedstate->_gc_disableDepth--;
//		sq_collectgarbage( m_hVM );
//
//#ifdef VPYTHON_DEBUG_SERIALIZATION_HEAPCHK
//		g_pMemAlloc->CrtSetDbgFlag( flags );
//#endif
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	void RemoveOrphanInstances()
	{

	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	virtual void SetOutputCallback( ScriptOutputFunc_t pFunc )
	{

	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	virtual void SetErrorCallback( ScriptErrorFunc_t pFunc )
	{

	}
	//---------------------------------------------------------------------------------
	// The main call dispatcher - dispatches to C++ functions called from python.
	// Get script binding object, translate args and dispatch to actual c function call
	// NOTE: this must be static! (ie - it is called without a this ptr from the proxy functions)
	//---------------------------------------------------------------------------------
	static PyObject *TranslateCall( ScriptFunctionBinding_t *pVMScriptFunction, scriptClassInstance_t *pSelf, PyObject *pArgs)
	{
		int nActualParams = (int) PyTuple_Size( pArgs );

		int nFormalParams = pVMScriptFunction->m_desc.m_Parameters.Count();
		CUtlVectorFixed<ScriptVariant_t, 14> params;
		ScriptVariant_t returnValue;

		params.SetSize( nFormalParams );

		// convert python params to vector of scriptVariant_t params as req'd by binding function
		int i = 0;
		PyObject *pyobj;

		if ( nActualParams )
		{
			int iLimit = MIN( nActualParams, nFormalParams );
			ScriptDataType_t *pCurParamType = pVMScriptFunction->m_desc.m_Parameters.Base();
			for ( i = 0; i < iLimit; i++, pCurParamType++ )
			{
				pyobj = PyTuple_GetItem( pArgs, (Py_ssize_t)i );

				switch ( *pCurParamType )
				{
				case FIELD_FLOAT:	
					{
						if ( !PyFloat_Check( pyobj) )
						{
							PyErr_SetString(PyExc_ValueError, "expected float argument");
							return NULL;
						}
						params[i] = PyFloat_AsDouble( pyobj ); 
					}
					break;
				case FIELD_CSTRING:		
					{
						if ( !PyString_Check( pyobj ) )
						{
							PyErr_SetString(PyExc_ValueError, "expected string argument");
							return NULL;
						}

						params[i] = PyString_AsString( pyobj );  // DO NOT FREE THIS
						Assert( !(params[i].m_flags &= SV_FREE ) );
					}
					break;  
				case FIELD_VECTOR:	
					{
						if ( pyobj->ob_type != &PyTypeVector )
						{
							if ( !PyString_Check( pyobj ) )
							{
								PyErr_SetString(PyExc_ValueError, "expected vector argument");
								return NULL;
							}
						}

						Vector *pVector = ((PyVectorInstance_t *)pyobj)->pVector; // get pointer
						if ( pVector )
						{
							params[i] = pVector;	// DO NOT FREE THIS
							Assert( !(params[i].m_flags &= SV_FREE ) );
							break;
						}
					}
					break;
				case FIELD_INTEGER:	
					{
						if ( !PyInt_Check( pyobj ) )
						{
							PyErr_SetString(PyExc_ValueError, "expected integer argument");
							return NULL;
						}
						params[i] = PyInt_AsLong( pyobj );
					}
					break;
				case FIELD_BOOLEAN:
					{
						if ( pyobj == Py_False )
						{
							params[i] = false;
						}
						else if ( pyobj == Py_True )
						{
							params[i] = true;
						}
						else
						{
							PyErr_SetString(PyExc_ValueError, "expected boolean argument");
							return NULL;
						}
					}
					break;
				case FIELD_CHARACTER:	
					{ 
						if ( !PyString_Check( pyobj ) )
						{
							PyErr_SetString(PyExc_ValueError, "expected string argument");
							return NULL;
						}
						const char *psz = PyString_AsString( pyobj );
						params[i] = *psz;
					}
				case FIELD_HSCRIPT:
					{
						if ( pyobj == Py_None )
						{
							params[i] = (HSCRIPT)NULL;
						}
						else
						{
							if ( ((scriptClassInstance_t *)pyobj)->typeTag != TYPETAG_INSTANCE )
							{
								PyErr_SetString(PyExc_ValueError, "expected HSCRIPT instance object argument");
								return NULL;
							}
							params[i] = (HSCRIPT)pyobj; // (HSCRIPT)PyCObject_AsVoidPtr( pyobj );
						}
						break;
					}
				default:				
					break;
				}
			}
		}

#ifdef _DEBUG
		for ( ; i < nFormalParams; i++ )
		{
			Assert( params[i].IsNull() );
		}
#endif

		// get object instance pointer from pSelf if this is a method call on object
		InstanceContext_t *pContext;
		void *pObject;

		if ( pVMScriptFunction->m_flags & SF_MEMBER_FUNC )
		{
			pContext = &( pSelf->instanceContext );

			if ( !pContext )
			{
				PyErr_SetString(PyExc_ValueError, "Accessed null instance");
				return NULL;
			}

			pObject = pContext->pInstance;

			if ( !pObject )
			{
				PyErr_SetString(PyExc_ValueError, "Accessed null instance");
				return NULL;
			}

			if ( pContext->pClassDesc->pHelper )
			{
				pObject = pContext->pClassDesc->pHelper->GetProxied( pObject );
			}

			if ( !pObject )
			{
				PyErr_SetString(PyExc_ValueError, "Accessed null instance");
				return NULL;
			}
		}
		else
		{
			pObject = NULL;
		}

		// call the binding function, which will make the actual C function call
		
		(*pVMScriptFunction->m_pfnBinding)( pVMScriptFunction->m_pFunction, pObject, params.Base(), params.Count(), ( pVMScriptFunction->m_desc.m_ReturnType != FIELD_VOID ) ? &returnValue : NULL );

		PyObject *pret = NULL;

		// use the returned scriptvariant to create a new python object

		if ( pVMScriptFunction->m_desc.m_ReturnType != FIELD_VOID )
		{
			// this is the ONE case where we must actually embed a reference to the returned Vector,
			// instead of creating a copy of the returned Vector.
			// this is because the binding function call above auto-creates a new Vector for the return value. (see vscript_templates.h line 278 etc)
			pret = ((CPythonVM *)g_pVm)->ConvertToPyObject( returnValue, false ); // create new ref
		}


		// NOTE: returning NULL and setting error state above should throw the python error...
		if ( pret == NULL )
			Py_RETURN_NONE;

		return pret;
	}

private:
	//----------------
	// inline Helpers
	//----------------

	//------------------------------------------------------------------------------
	// Purpose: print most recent python error to console
	//------------------------------------------------------------------------------
	inline bool PyPrintError()
	{
		if ( PyErr_Occurred() ) 
		{
			PyErr_Print();
			return true;
		}
		return false;
	}
	
	//------------------------------------------------------------------------------
	// Purpose: given hscope, return scope's instance object's dictionary.
	// returns NULL if no instance object associated with scope.
	//------------------------------------------------------------------------------
	inline PyObject *InstanceDictFromScope( HSCRIPT hScope )
	{
		if ( !hScope )
			return NULL;
		AssertIsScope( hScope );
		Assert ( hScope != INVALID_HSCRIPT );
		return ((CPyScope *)hScope)->GetInstanceDict();
	}

	//----------------------------------------------------------
	// given hscope, return scope's module-level dictionary
	//----------------------------------------------------------
	inline PyObject *ModuleDictFromScope( HSCRIPT hScope )
	{
		if ( hScope )
		{
			// get module's dict
			AssertIsScope( hScope );
			Assert ( hScope != INVALID_HSCRIPT );
			return ((CPyScope *)hScope)->GetModuleDict();
		}
		else
		{
			// get global scope dict
			return m_pRootScope->GetModuleDict();
		}
	}

	//---------------------------------------------------------------
	// given class descriptor, get our pre-defined python type object.
	// return NULL if not found.
	//---------------------------------------------------------------
	static inline PyTypeObject *PyTypeFromDesc( ScriptClassDesc_t *pDesc )

	{
		if ( !pDesc )
			return NULL;

		UtlHashFastHandle_t h = ((CPythonVM *)g_pVm)->m_TypeMap.Find( (int)pDesc );
		
		if ( h == ((CPythonVM *)g_pVm)->m_TypeMap.InvalidHandle() )
		{
			return NULL;
		}
		return (PyTypeObject *)(((CPythonVM *)g_pVm)->m_TypeMap.Element ( h ));
	}

	//---------------------------------------------------------------
	// given python object, get class descriptor associated with object's type.
	// return NULL if object is not a server-side object.
	//---------------------------------------------------------------
	static inline ScriptClassDesc_t *pDescFromPyObj( PyObject *pobj )
	{
		if ( ! pobj )
			return NULL;

		AssertIsPyObject( (HSCRIPT)pobj );
		PyTypeObject *ptype = pobj->ob_type;

		UtlHashFastHandle_t h = ((CPythonVM *)g_pVm)->m_ClassMap.Find( (int)ptype );
		
		if ( h == ((CPythonVM *)g_pVm)->m_ClassMap.InvalidHandle() )
		{
			return NULL;
		}
		return (ScriptClassDesc_t *)(((CPythonVM *)g_pVm)->m_ClassMap.Element ( h ));
	}

	////---------------------------------------------------------
	//// Callbacks
	////---------------------------------------------------------
	//static void PrintFunc(HPYTHONVM m_hVM,const SQChar* s,...)
	//{
	//	Msg( CFmtStr( &s ) );
	//}

//	//-------------------------------------------------------------
//	//
//	//-------------------------------------------------------------
//	static SQInteger ReleaseHook( SQUserPointer p, SQInteger size )
//	{
//		InstanceContext_t *pInstanceContext = (InstanceContext_t *)p;
//		pInstanceContext->pClassDesc->m_pfnDestruct( pInstanceContext->pInstance );
//		delete pInstanceContext;
//		return 0;
//	}
//
//	//-------------------------------------------------------------
//	//
//	//-------------------------------------------------------------
//	static SQInteger ExternalInstanceReleaseHook( SQUserPointer p, SQInteger size )
//	{
//		InstanceContext_t *pInstanceContext = (InstanceContext_t *)p;
//		delete pInstanceContext;
//		return 0;
//	}
//
//	//-------------------------------------------------------------
//	//
//	//-------------------------------------------------------------
//	static SQInteger GetFunctionSignature( HPYTHONVM hVM )
//	{
//		StackHandler sa(hVM);
//		if ( sa.GetParamCount() != 3 )
//		{
//			return 0;
//		}
//
//		HPYOBJECT hFunction = sa.GetObjectHandle( 2 );
//		if ( !sq_isclosure( hFunction ) )
//		{
//			return 0;
//		}
//
//		std::string result;
//		const char *pszName = sa.GetString( 3 );
//		SQClosure *pClosure = hFunction._unVal.pClosure;
//		SQFunctionProto *pProto = pClosure->_function._unVal.pFunctionProto;
//
//		result += "function ";
//		if ( pszName && *pszName )
//		{
//			result += pszName;
//		}
//		else if ( sq_isstring( pProto->_name ) )
//		{
//			result += pProto->_name._unVal.pString->_val;
//		}
//		else
//		{
//			result += "<unnamed>";
//		}
//		result += "(";
//
//		for ( int i = 1; i < pProto->_nparameters; i++ )
//		{
//			if ( i != 1 )
//				result += ", ";
//			if ( sq_isstring( pProto->_parameters[i] ) )
//			{
//				result += pProto->_parameters[i]._unVal.pString->_val;
//			}
//			else
//			{
//				result += "arg";
//			}
//		}
//		result += ")";
//
//		sa.Return( result.c_str() );
//
//		return 1;
//	}
//
//	//-------------------------------------------------------------
//	//
//	//-------------------------------------------------------------
//	static SQInteger GetDeveloper( HPYTHONVM hVM )
//	{
//		StackHandler sa(hVM);
//		sa.Return( ((CPythonVM *)hVM->_sharedstate->m_pOwnerData)->developer.GetInt() );
//		return 1;
//	}
//
	//-------------------------------------------------------------
	// called from python directly during object construction.
	// same as class __init__ function - init the member data for server object instance
	// If the context has a constructor, call it to create the server object instance.
	// args and keywords params currently ignored.
	//-------------------------------------------------------------
	static int InitInstance( scriptClassInstance_t *pSelf, PyObject *args, PyObject *kwds )
	{
		InstanceContext_t *pInstanceContext = &( pSelf->instanceContext );

		pInstanceContext->pInstance = NULL; 
		pInstanceContext->pClassDesc = NULL;
		pInstanceContext->pPyName  = NULL;
		pSelf->pDict = PyDict_New();
		
		//((scriptClassInstance_t *)pSelf)->typeTag = TYPETAG_INSTANCE;

		ScriptClassDesc_t *pDesc = pDescFromPyObj( (PyObject *)pSelf );

		if ( pDesc )
		{
			pInstanceContext->pClassDesc = pDesc;
		
			if ( pDesc->m_pfnConstruct )
			{
				pInstanceContext->pInstance = pDesc->m_pfnConstruct();
			}
		}
		else
		{
			return -1;
		}

		
		return 0;
	}

	//----------------------------------------------------------------------
	// called from python directly during object destruction (tp_dealloc)
	// call destructor on the server object instance, then release the python object
	//----------------------------------------------------------------------
	static void FreeInstance( scriptClassInstance_t *pSelf )
	{
		AssertIsInstance( (HSCRIPT)pSelf );
		InstanceContext_t *pcontext = &( pSelf->instanceContext );

		ScriptClassDesc_t *pDesc = pDescFromPyObj( (PyObject *)pSelf );

		if ( pDesc )
		{
			if ( pDesc->m_pfnDestruct )
			{
				pDesc->m_pfnDestruct( pcontext->pInstance );
			}
		}

		pcontext->pInstance = NULL;
		
		Py_XDECREF( pcontext->pPyName );
		Py_XDECREF( pSelf->pDict );			// will decref all objs held in dict
		
		pSelf->ob_type->tp_free( (PyObject *)pSelf );

	}

	//-------------------------------------------------------------
	// UNDONE: script execution throttle -  equivalent in python? 

	//static int QueryContinue( HPYTHONVM hVM )
	//{
	//	CPythonVM *pVM = ((CPythonVM *)hVM->_sharedstate->m_pOwnerData);
	//	if ( !pVM->m_hDbg )
	//	{
	//		if ( pVM->m_TimeStartExecute != 0.0f && Plat_FloatTime() - pVM->m_TimeStartExecute > 0.03f )
	//		{
	//			DevMsg( "Script running too long, terminating\n" );
	//			// @TODO: Mark the offending closure so that it won't be executed again [5/13/2008 tom]
	//			return SQ_QUERY_BREAK;
	//		}
	//	}
	//	return SQ_QUERY_CONTINUE;
	//}


	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	static PyObject *InstanceIsValid( PyObject *pSelf, PyObject *pArgs )
	{
		// UNDONE:
		//InstanceContext_t *pContext = (InstanceContext_t *)pSelf;
		//
		//if ( pContext && pContext->pInstance )
		//	Py_RETURN_TRUE;
		//else
		//	Py_RETURN_FALSE;
	}

	//-------------------------------------------------------------
	// allow C++ function to be called from python - 
	// function parameter binding is described by
	// ScriptFunctionBinding_t
	//-------------------------------------------------------------
	void RegisterFunctionGuts( ScriptFunctionBinding_t *pScriptFunction )
	{
		PyObject *pdict = m_pValveScope->GetModuleDict();

		if ( PyDict_GetItemString( pdict, pScriptFunction->m_desc.m_pszScriptName ) != NULL )
			return; // already registered


		// alloc space for small (2 element) array of PyMethodDefs - elements must not move in memory.
		PyMethodDef *pmethod = new PyMethodDef[2];
		
		// save pointers so we can free 'em all later
		Assert (m_iMethodDef < MAX_VALVE_FUNCTIONS_EXPORTED );
		m_rgpMethodDefs[m_iMethodDef++] = pmethod;

		// create NULL semaphore at tail of list
		PyMethodDef *pm = &(pmethod[1]);
		
		pm->ml_name = NULL;
		pm->ml_meth = NULL;
		pm->ml_flags = 0;
		pm->ml_doc = NULL;

		// fill the python method def
		pm = &(pmethod[0]);

		pm->ml_name = pScriptFunction->m_desc.m_pszScriptName;
		pm->ml_flags = METH_VARARGS;
		pm->ml_doc = pScriptFunction->m_desc.m_pszDescription;
		
		int proxyId = GetNewProxyId();
		SetProxyBinding( proxyId, pScriptFunction );

		// the function callback chain - python calls Translate_XXX -> calls TranslateCall -> calls binding function -> actual function.															
		pm->ml_meth = GetProxyFunction( proxyId );
		
		Py_InitModule3("valve", pmethod, "Import module for access to all exported Valve methods.");

		// set up parameter checking
		
		char szTypeMask[64];

		if ( pScriptFunction->m_desc.m_Parameters.Count() > ARRAYSIZE(szTypeMask) - 1 )
		{
			AssertMsg1( 0, "Too many arguments for script function %s\n", pScriptFunction->m_desc.m_pszFunction );
			return;
		}

		// UNDONE: implement help - function param type documenting in python

		//szTypeMask[0] = '.';
		//char *pCurrent = &szTypeMask[1];
		//for ( int i = 0; i < pScriptFunction->m_desc.m_Parameters.Count(); i++, pCurrent++ )
		//{
		//	switch ( pScriptFunction->m_desc.m_Parameters[i] )
		//	{
		//	case FIELD_CSTRING:	
		//		*pCurrent = 's';
		//		break;
		//	case FIELD_FLOAT:
		//	case FIELD_INTEGER:
		//		*pCurrent = 'n';
		//		break;
		//	case FIELD_BOOLEAN:
		//		*pCurrent = 'b';
		//		break;

		//	case FIELD_VECTOR:
		//		*pCurrent = 'x';
		//		break;

		//	case FIELD_HSCRIPT:
		//		*pCurrent = '.';
		//		break;

		//	case FIELD_CHARACTER:
		//	default:
		//		*pCurrent = FIELD_VOID;
		//		AssertMsg( 0 , "Not supported" );
		//		break;
		//	}
		//}
		//Assert( pCurrent - szTypeMask < ARRAYSIZE(szTypeMask) - 1 );
		//*pCurrent = 0;

		//sq_pushstring( m_hVM, pScriptFunction->m_desc.m_pszScriptName, -1 );
		//ScriptFunctionBinding_t **pVMScriptFunction = (ScriptFunctionBinding_t **)sq_newuserdata(m_hVM, sizeof(ScriptFunctionBinding_t *));
		//*pVMScriptFunction = pScriptFunction;
		//sq_newclosure( m_hVM, &TranslateCall, 1 );
		//HPYOBJECT hFunction;
		//sq_getstackobj( m_hVM, -1, &hFunction );
		//sq_setnativeclosurename(m_hVM, -1, pScriptFunction->m_desc.m_pszScriptName );
		//sq_setparamscheck( m_hVM, pScriptFunction->m_desc.m_Parameters.Count() + 1, szTypeMask );
		//sq_createslot( m_hVM, -3 );

		//if ( developer.GetInt() )
		//{
		//	const char *pszHide = SCRIPT_HIDE;
		//	if ( !pScriptFunction->m_desc.m_pszDescription || *pScriptFunction->m_desc.m_pszDescription != *pszHide )
		//	{
		//		std::string name;
		//		std::string signature;

		//		if ( pClassDesc )
		//		{
		//			name += pClassDesc->m_pszScriptName;
		//			name += "::";
		//		}

		//		name += pScriptFunction->m_desc.m_pszScriptName;

		//		signature += FieldTypeToString( pScriptFunction->m_desc.m_ReturnType );
		//		signature += ' ';
		//		signature += name;
		//		signature += '(';
		//		for ( int i = 0; i < pScriptFunction->m_desc.m_Parameters.Count(); i++ )
		//		{
		//			if ( i != 0 )
		//			{
		//				signature += ", ";
		//			}

		//			signature+= FieldTypeToString( pScriptFunction->m_desc.m_Parameters[i] );
		//		}
		//		signature += ')';

		//		sq_pushobject( m_hVM, LookupObject( "RegisterFunctionDocumentation", NULL, false ) );
		//		sq_pushroottable( m_hVM );
		//		sq_pushobject( m_hVM, hFunction );
		//		sq_pushstring( m_hVM, name.c_str(), name.length() );
		//		sq_pushstring( m_hVM, signature.c_str(), signature.length() );
		//		sq_pushstring( m_hVM, pScriptFunction->m_desc.m_pszDescription, -1 );
		//		sq_call( m_hVM, 5, false, /*false*/ true );
		//		sq_pop( m_hVM, 1 );
		//	}
		//}

		return;
	}

	//-------------------------------------------------------------
	// drop our ref count on the script object
	//-------------------------------------------------------------
	void ReleaseScriptObject( HSCRIPT hScript )
	{
		AssertIsPyObject( hScript );
		Py_XDECREF( (PyObject *)hScript );

	}

	//-------------------------------------------------------------
	// create a new python object from the script variant
	// UNLESS variant is an HSCRIPT - in this case, just returns
	// the embedded PyObject * with incremented ref count.
	// if bAllocNewVector is true, create a new C++ Vector,
	// otherwise, embed a reference to the variant's Vector, within new py object.
	// NOTE: all references will be freed when the py object is freed!
	//-------------------------------------------------------------
	PyObject *ConvertToPyObject( const ScriptVariant_t &value, bool bAllocNewVector )
	{
		switch ( value.m_type )
		{
		case FIELD_VOID:		Py_RETURN_NONE; 
		case FIELD_FLOAT:		return PyFloat_FromDouble( (double)value.m_float ); 
		case FIELD_CSTRING:	
			if ( value.IsNull() )
				Py_RETURN_NONE;

			return PyString_FromStringAndSize( value, (Py_ssize_t) strlen( value.m_pszString )); 
		case FIELD_VECTOR:
			{
				PyObject *pretObj;
				if ( !bAllocNewVector )
				{
					// Vector was alloc'd by caller, and in this (rare) case we are expected to free it.
					// create a python vector object that references the variant's vector object
					// NOTE: the variant's vector object will be deleted when the python object is deleted.
					DEBUG_VECCOUNT++; 
					pretObj = CreatePyVector( (Vector *)value.m_pVector );
				}
				else
				{
					// create new python vector object and copy from scriptvariant data. must be freed by caller
					pretObj = CreatePyVector( NULL );
					*(((PyVectorInstance_t*)pretObj)->pVector) = *((Vector *)value.m_pVector); // copy operator
				}
				return pretObj;
			}
		case FIELD_INTEGER:		return PyInt_FromLong( value.m_int );
		case FIELD_BOOLEAN:		return PyBool_FromLong( value.m_bool ); 
		case FIELD_CHARACTER:	
			{ 
				char sz[2]; 
				sz[0] = value.m_char; 
				sz[1] = 0; 
				return PyString_FromStringAndSize( sz, (Py_ssize_t)1 );
			}
		case FIELD_HSCRIPT:	
			{
				if ( value.m_hScript ) 
				{
					PyObject *pyobj = (PyObject *)value.m_hScript; //PyCObject_FromVoidPtr((void *)value.m_hScript, NULL);
					Py_XINCREF( pyobj );
					return pyobj;
				}
				else 
				{
					Py_RETURN_NONE;
				}
			}
		}
		Py_RETURN_NONE;
	}

	//-------------------------------------------------------------
	// fill variant struct with appropriate value from python object
	// NOTE: does not decref the python object.
	//-------------------------------------------------------------
	bool ConvertToVariant( PyObject* object, ScriptVariant_t *pReturn )
	{
		AssertIsPyObject( (HSCRIPT)object );

		if ( object == Py_None )
		{
			pReturn->m_type = FIELD_VOID;
		}
		else if ( PyLong_CheckExact( object ) )
		{
			*pReturn = (int)PyLong_AsLong( object ); // UNDONE: need error checking for overflow - will return NULL
		}
		else if ( PyInt_CheckExact( object ) )
		{
			*pReturn = (int)PyInt_AS_LONG( object ); // No error checking is performed, since we started with int
		}
		else if ( PyFloat_CheckExact( object ) )
		{
			*pReturn = (float)PyFloat_AS_DOUBLE( object );	// no error checking since we started with float
		}
		else if ( PyBool_Check( object ) )
		{
			if ( object == Py_True )
				*pReturn = true;
			else
				*pReturn = false;
		}
		else if ( PyString_Check( object ) )
		{
			// create a new string in the variant
			char *buffer;
			Py_ssize_t length;

			PyString_AsStringAndSize( object, &buffer, &length);
			
			int size = (int)length + 1; 
			pReturn->m_type = FIELD_CSTRING;
			pReturn->m_pszString = new char[size]; 
			V_memcpy( (void *)pReturn->m_pszString, buffer, size ); 
			pReturn->m_flags |= SV_FREE;
			DEBUG_VARIANTCOUNT++;
			Assert( DEBUG_VARIANTCOUNT < 1000 ); // if this fails, server is likely not freeing return values from python fn calls each frame
		}
		else if ( IsPyVector( object ) )
		{
			// create a new vector in the variant that copies the object's vector data
			Vector *pVector = ((PyVectorInstance_t *)object)->pVector;

			pReturn->m_type = FIELD_VECTOR;
			pReturn->m_pVector = new Vector( *((Vector *)pVector) ); 
			pReturn->m_flags |= SV_FREE;
			DEBUG_VARIANTCOUNT++;
			Assert( DEBUG_VARIANTCOUNT < 1000 ); // if this fails, server is likely not freeing return values from python fn calls each frame
		}
		else
		{
			// save the actual object pointer
			pReturn->m_type = FIELD_HSCRIPT;
			pReturn->m_hScript =(HSCRIPT)object; // PyCObject_AsVoidPtr( object );
			return false; // don't free object
		}
		return true; // ok to free python object

	}


//
//	//-------------------------------------------------------------------------
//	// UNDONE: Serialization for save/restore
//	//-------------------------------------------------------------------------
//	enum
//	{
//		SAVEVERSION = 2
//	};
//
//	void WriteObject( const SQObjectPtr &object )
//	{
//		switch ( object._type )
//		{
//			case OT_NULL:
//				m_pBuffer->PutInt( OT_NULL ); 
//				break;
//			case OT_INTEGER:
//				m_pBuffer->PutInt( OT_INTEGER ); 
//				m_pBuffer->PutInt( object._unVal.nInteger );
//				break;
//			case OT_FLOAT:
//				m_pBuffer->PutInt( OT_FLOAT ); 
//				m_pBuffer->PutFloat( object._unVal.fFloat);
//				break;
//			case OT_BOOL:			
//				m_pBuffer->PutInt( OT_BOOL ); 
//				m_pBuffer->PutInt( object._unVal.nInteger );
//				break;
//			case OT_STRING:			
//				m_pBuffer->PutInt( OT_STRING ); 
//				m_pBuffer->PutInt( object._unVal.pString->_len );
//				m_pBuffer->PutString( object._unVal.pString->_val );	
//				break;
//			case OT_TABLE:			WriteTable( object._unVal.pTable );					break;
//			case OT_ARRAY:			WriteArray( object._unVal.pArray );					break;
//			case OT_USERDATA:		WriteUserData( object._unVal.pUserData );			break;
//			case OT_CLOSURE:		WriteClosure( object._unVal.pClosure );				break;
//			case OT_NATIVECLOSURE:	WriteNativeClosure( object._unVal.pNativeClosure );	break;
//			case OT_GENERATOR:		WriteGenerator( object._unVal.pGenerator );			break;
//			case OT_USERPOINTER:	WriteUserPointer( object._unVal.pUserPointer );		break;
//			case OT_THREAD:			WriteVM( object._unVal.pThread );					break;
//			case OT_FUNCPROTO:		WriteFuncProto( object._unVal.pFunctionProto );		break;
//			case OT_CLASS:			WriteClass( object._unVal.pClass );					break;
//			case OT_INSTANCE:		WriteInstance( object._unVal.pInstance );			break;
//			case OT_WEAKREF:		WriteWeakRef( object._unVal.pWeakRef );				break;
//			default:				Assert( 0 ); break;
//		}
//
//#ifdef VPYTHON_DEBUG_SERIALIZATION
//		SQObjectPtr res;
//		m_hVM->ToString( object, res );
//		Msg( "%d: %s\n", m_pBuffer->TellPut(),  res._unVal.pString->_val );
//#endif
//	}
//
//	//-------------------------------------------------------------
//	//
//	//-------------------------------------------------------------
//	void WriteVM( PYVM *pVM )
//	{
//		unsigned i;
//
//		m_pBuffer->PutInt( OT_THREAD );
//		m_pBuffer->PutPtr( pVM );
//
//		if ( pVM->_uiRef & MARK_FLAG )
//			return;
//		pVM->_uiRef |= MARK_FLAG;
//
//		WriteObject( pVM->_roottable );
//		m_pBuffer->PutInt( pVM->_top );
//		m_pBuffer->PutInt( pVM->_stackbase );
//		m_pBuffer->PutUnsignedInt( pVM->_stack.size() );
//		for( i = 0; i < pVM->_stack.size(); i++ ) 
//		{
//			WriteObject( pVM->_stack[i] );
//		}
//		m_pBuffer->PutUnsignedInt( pVM->_vargsstack.size() );
//		for( i = 0; i < pVM->_vargsstack.size(); i++ ) 
//		{
//			WriteObject( pVM->_vargsstack[i] );
//		}
//	}
//
//	//-------------------------------------------------------------
//	//
//	//-------------------------------------------------------------
//	void WriteArray( SQArray *pArray )
//	{
//		m_pBuffer->PutInt( OT_ARRAY );
//		m_pBuffer->PutPtr( pArray );
//
//		if ( pArray->_uiRef & MARK_FLAG )
//			return;
//		pArray->_uiRef |= MARK_FLAG;
//
//		int len = pArray->_values.size();
//		m_pBuffer->PutInt( len );
//		for ( int i = 0; i < len; i++ )
//			WriteObject( pArray->_values[i] );
//	}
//
//	//-------------------------------------------------------------
//	//
//	//-------------------------------------------------------------
//	void WriteTable( SQTable *pTable )
//	{
//		m_pBuffer->PutInt( OT_TABLE );
//		m_pBuffer->PutPtr( pTable );
//
//		if ( pTable->_uiRef & MARK_FLAG )
//			return;
//		pTable->_uiRef |= MARK_FLAG;
//
//		m_pBuffer->PutInt( pTable->_delegate != NULL );
//		if ( pTable->_delegate )
//		{
//			WriteObject( pTable->_delegate );
//		}
//
//		int len = pTable->_numofnodes;
//		m_pBuffer->PutInt( len );
//		for(int i = 0; i < len; i++)
//		{
//			WriteObject( pTable->_nodes[i].key );
//			WriteObject( pTable->_nodes[i].val );
//		}
//	}
//
//	//-------------------------------------------------------------
//	//
//	//-------------------------------------------------------------
//	void WriteClass( SQClass *pClass )
//	{
//		m_pBuffer->PutInt( OT_CLASS );
//		m_pBuffer->PutPtr( pClass );
//
//		if ( !pClass || ( pClass->_uiRef & MARK_FLAG ) )
//			return;
//		pClass->_uiRef |= MARK_FLAG;
//
//		bool bIsNative = ( pClass->_typetag != NULL );
//		unsigned i;
//		if ( !bIsNative )
//		{
//			for( i = 0; i < pClass->_methods.size(); i++) 
//			{
//				if ( sq_isnativeclosure( pClass->_methods[i].val ) )
//				{
//					bIsNative = true;
//					break;
//				}
//			}
//		}
//		m_pBuffer->PutInt( bIsNative );
//		if ( !bIsNative )
//		{
//			m_pBuffer->PutInt( pClass->_base != NULL );
//			if ( pClass->_base )
//			{
//				WriteObject( pClass->_base );
//			}
//
//			WriteObject( pClass->_members );
//			WriteObject( pClass->_attributes );
//			m_pBuffer->PutInt( pClass->_defaultvalues.size() );
//			for( i = 0; i< pClass->_defaultvalues.size(); i++) 
//			{
//				WriteObject(pClass->_defaultvalues[i].val);
//				WriteObject(pClass->_defaultvalues[i].attrs);
//			}
//			m_pBuffer->PutInt( pClass->_methods.size() );
//			for( i = 0; i < pClass->_methods.size(); i++) 
//			{
//				WriteObject(pClass->_methods[i].val);
//				WriteObject(pClass->_methods[i].attrs);
//			}
//			m_pBuffer->PutInt( pClass->_metamethods.size() );
//			for( i = 0; i < pClass->_metamethods.size(); i++) 
//			{
//				WriteObject(pClass->_metamethods[i]);
//			}
//		}
//		else
//		{
//			if ( pClass->_typetag )
//			{
//				if ( pClass->_typetag == TYPETAG_VECTOR )
//				{
//					m_pBuffer->PutString( "Vector" );
//				}
//				else
//				{
//					ScriptClassDesc_t *pDesc = (ScriptClassDesc_t *)pClass->_typetag;
//					m_pBuffer->PutString( pDesc->m_pszScriptName );
//				}
//			}
//			else
//			{
//				// Have to grovel for the name
//				SQObjectPtr key;
//				if ( FindKeyForObject( m_hVM->_roottable, pClass, key ) )
//				{
//					m_pBuffer->PutString( key._unVal.pString->_val );
//				}
//				else
//				{
//					Assert( 0 );
//					m_pBuffer->PutString( "" );
//				}
//			}
//		}
//	}
//
//	//-------------------------------------------------------------
//	//
//	//-------------------------------------------------------------
//	void WriteInstance( SQInstance *pInstance )
//	{
//		m_pBuffer->PutInt( OT_INSTANCE );
//		m_pBuffer->PutPtr( pInstance );
//
//		if ( pInstance->_uiRef & MARK_FLAG )
//			return;
//		pInstance->_uiRef |= MARK_FLAG;
//
//		WriteObject( pInstance->_class );
//
//		unsigned nvalues = pInstance->_class->_defaultvalues.size();
//		m_pBuffer->PutInt( nvalues );
//		for ( unsigned i =0; i< nvalues; i++ ) 
//		{
//			WriteObject( pInstance->_values[i] );
//		}
//
//		m_pBuffer->PutPtr( pInstance->_class->_typetag );
//
//		if ( pInstance->_class->_typetag )
//		{
//			if ( pInstance->_class->_typetag == TYPETAG_VECTOR )
//			{
//				Vector *pVector = (Vector *)pInstance->_userpointer;
//				m_pBuffer->PutFloat( pVector->x );
//				m_pBuffer->PutFloat( pVector->y );
//				m_pBuffer->PutFloat( pVector->z );
//			}
//			else
//			{
//				InstanceContext_t *pContext = ((InstanceContext_t *)pInstance->_userpointer);
//				WriteObject( pContext->name );
//				m_pBuffer->PutPtr( pContext->pInstance );
//			}
//		}
//		else
//		{
//			WriteUserPointer( NULL );
//		}
//	}
//
//	//-------------------------------------------------------------
//	//
//	//-------------------------------------------------------------
//	void WriteGenerator( SQGenerator *pGenerator )
//	{
//		ExecuteOnce( Msg( "Save load of generators not well tested. caveat emptor\n" ) );
//		WriteObject(pGenerator->_closure);
//
//		m_pBuffer->PutInt( OT_GENERATOR );
//		m_pBuffer->PutPtr( pGenerator );
//
//		if ( pGenerator->_uiRef & MARK_FLAG )
//			return;
//		pGenerator->_uiRef |= MARK_FLAG;
//		
//		WriteObject( pGenerator->_closure );
//		m_pBuffer->PutInt( pGenerator->_stack.size() );
//		for(SQUnsignedInteger i = 0; i < pGenerator->_stack.size(); i++) WriteObject(pGenerator->_stack[i]);
//		m_pBuffer->PutInt( pGenerator->_vargsstack.size() );
//		for(SQUnsignedInteger j = 0; j < pGenerator->_vargsstack.size(); j++) WriteObject(pGenerator->_vargsstack[j]);
//	}
//
//	//-------------------------------------------------------------
//	//
//	//-------------------------------------------------------------
//	void WriteClosure( SQClosure *pClosure )
//	{
//		m_pBuffer->PutInt( OT_CLOSURE );
//		m_pBuffer->PutPtr( pClosure );
//		if ( pClosure->_uiRef & MARK_FLAG )
//			return;
//		pClosure->_uiRef |= MARK_FLAG;
//
//		WriteObject( pClosure->_function );
//		WriteObject( pClosure->_env );
//
//		m_pBuffer->PutInt( pClosure->_outervalues.size() );
//		for(SQUnsignedInteger i = 0; i < pClosure->_outervalues.size(); i++) WriteObject(pClosure->_outervalues[i]);
//		m_pBuffer->PutInt( pClosure->_defaultparams.size() );
//		for(SQUnsignedInteger i = 0; i < pClosure->_defaultparams.size(); i++) WriteObject(pClosure->_defaultparams[i]);
//	}
//
//	//-------------------------------------------------------------
//	//
//	//-------------------------------------------------------------
//	void WriteNativeClosure( SQNativeClosure *pNativeClosure )
//	{
//		m_pBuffer->PutInt( OT_NATIVECLOSURE );
//		m_pBuffer->PutPtr( pNativeClosure );
//
//		if ( pNativeClosure->_uiRef & MARK_FLAG )
//			return;
//		pNativeClosure->_uiRef |= MARK_FLAG;
//
//		WriteObject( pNativeClosure->_name );
//		return;
//	}
//
//	//-------------------------------------------------------------
//	//
//	//-------------------------------------------------------------
//	void WriteUserData( SQUserData *pUserData )
//	{
//		m_pBuffer->PutInt( OT_USERDATA );
//		m_pBuffer->PutPtr( pUserData );
//
//		if ( pUserData->_uiRef & MARK_FLAG )
//			return;
//		pUserData->_uiRef |= MARK_FLAG;
//		
//		// Need to call back or something. Unsure, TBD. [4/3/2008 tom]
//	}
//
//	//-------------------------------------------------------------
//	//
//	//-------------------------------------------------------------
//	void WriteUserPointer( SQUserPointer pUserPointer )
//	{
//		m_pBuffer->PutInt( OT_USERPOINTER );
//		// Need to call back or something. Unsure, TBD. [4/3/2008 tom]
//		m_pBuffer->PutPtr( pUserPointer );
//	}
//
//	//-------------------------------------------------------------
//	//
//	//-------------------------------------------------------------
//	static SQInteger SqWriteFunc(SQUserPointer up,SQUserPointer data, SQInteger size)
//	{
//		CPythonVM *pThis = (CPythonVM *)up;
//		pThis->m_pBuffer->Put( data, size );
//		return size;
//	}
//
//	void WriteFuncProto( SQFunctionProto *pFuncProto )
//	{
//		m_pBuffer->PutInt( OT_FUNCPROTO );
//		m_pBuffer->PutPtr( pFuncProto );
//
//		// Using the map to track these as they're not collectables
//		if ( m_PtrMap.Find( pFuncProto ) != m_PtrMap.InvalidIndex() )
//		{
//			return;
//		}
//		m_PtrMap.Insert( pFuncProto, pFuncProto );
//
//		pFuncProto->Save( m_hVM, this, &SqWriteFunc );
//	}
//
//	//-------------------------------------------------------------
//	//
//	//-------------------------------------------------------------
//	void WriteWeakRef( SQWeakRef *pWeakRef )
//	{
//		m_pBuffer->PutInt( OT_WEAKREF );
//		WriteObject( pWeakRef->_obj );
//	}
//
//	//--------------------------------------------------------
//	template <typename T>
//	bool BeginRead( T **ppOld, T **ppNew )
//	{
//		*ppOld = (T *)m_pBuffer->GetPtr();
//		if ( *ppOld )
//		{
//			int iNew = m_PtrMap.Find( *ppOld );
//			if ( iNew != m_PtrMap.InvalidIndex() )
//			{
//				*ppNew = (T*)m_PtrMap[iNew];
//				return false;
//			}
//		}
//		*ppNew = NULL;
//		return true;
//	}
//
//	//-------------------------------------------------------------
//	//
//	//-------------------------------------------------------------
//	void MapPtr( void *pOld, void *pNew )
//	{
//		Assert( m_PtrMap.Find( pOld ) == m_PtrMap.InvalidIndex() );
//		m_PtrMap.Insert( pOld, pNew );
//	}
//
//	//-------------------------------------------------------------
//	//
//	//-------------------------------------------------------------
//	bool ReadObject( SQObjectPtr &objectOut, const char *pszName = NULL )
//	{
//		SQObject object;
//		bool bResult = true;
//		object._type = (SQObjectType)m_pBuffer->GetInt();
//		if ( _RAW_TYPE(object._type) < _RT_TABLE )
//		{
//			switch ( object._type )
//			{
//			case OT_NULL:
//				object._unVal.pUserPointer = 0;
//				break;
//			case OT_INTEGER:
//				object._unVal.nInteger = m_pBuffer->GetInt();
//				break;
//			case OT_FLOAT:
//				object._unVal.fFloat = m_pBuffer->GetFloat();
//				break;
//			case OT_BOOL:			
//				object._unVal.nInteger = m_pBuffer->GetInt();
//				break;
//			case OT_STRING:
//				{
//					int len = m_pBuffer->GetInt();
//					char *pString = (char *)stackalloc( len + 1 );
//					m_pBuffer->GetString( pString, len + 1 );
//					pString[len] = 0;
//					object._unVal.pString = SQString::Create( m_hVM->_sharedstate, pString, len );
//					break;
//				}
//			default:
//				Assert( 0 );
//				break;
//			}
//		}
//		else
//		{
//			switch ( object._type )
//			{
//			case OT_TABLE:
//				{
//					object._unVal.pTable = ReadTable();
//					break;
//				}
//			case OT_ARRAY:			
//				{
//					object._unVal.pArray = ReadArray();
//					break;
//				}
//			case OT_USERDATA:
//				{
//					object._unVal.pUserData = ReadUserData();			
//					break;
//				}
//			case OT_CLOSURE:
//				{
//					object._unVal.pClosure = ReadClosure();
//					break;
//				}
//			case OT_NATIVECLOSURE:	
//				{
//					object._unVal.pNativeClosure = ReadNativeClosure();
//					break;
//				}
//			case OT_GENERATOR:
//				{
//					object._unVal.pGenerator = ReadGenerator();
//					break;
//				}
//			case OT_USERPOINTER:
//				{
//					object._unVal.pUserPointer = ReadUserPointer();
//					break;
//				}
//			case OT_THREAD:			
//				{
//					object._unVal.pThread = ReadVM();
//					break;
//				}
//			case OT_FUNCPROTO:
//				{
//					object._unVal.pFunctionProto = ReadFuncProto();
//					break;
//				}
//			case OT_CLASS:			
//				{
//					object._unVal.pClass = ReadClass();			
//					break;
//				}
//			case OT_INSTANCE:
//				{
//					object._unVal.pInstance = ReadInstance();
//					if ( !object._unVal.pInstance )
//					{
//						// Look for a match in the current root table
//						HPYOBJECT hExistingObject = LookupObject( pszName, NULL, false );
//						if ( sq_isinstance( hExistingObject ) )
//						{
//							object._unVal.pInstance = hExistingObject._unVal.pInstance;	
//						}
//					}
//					break;
//				}
//			case OT_WEAKREF:		
//				{
//					object._unVal.pWeakRef = ReadWeakRef();
//					break;
//				}
//			default:				
//				{
//					object._unVal.pUserPointer = NULL;
//					Assert( 0 );
//				}
//			}
//			if ( !object._unVal.pUserPointer )
//			{
//				DevMsg( "Failed to restore a Python object of type %s\n", SQTypeToString( object._type ) );
//				object._type = OT_NULL;
//				bResult = false;
//			}
//		}
//
//#ifdef VPYTHON_DEBUG_SERIALIZATION
//		lastType = object._type;
//		SQObjectPtr res;
//		if ( ISREFCOUNTED(object._type) )
//			object._unVal.pRefCounted->_uiRef++;
//		m_hVM->ToString( object, res );
//		if ( ISREFCOUNTED(object._type) )
//			object._unVal.pRefCounted->_uiRef--;
//		Msg( "%d: %s [%d]\n", m_pBuffer->TellGet(),  res._unVal.pString->_val, ( ISREFCOUNTED(object._type) ) ? object._unVal.pRefCounted->_uiRef : -1 );
//#ifdef VPYTHON_DEBUG_SERIALIZATION_HEAPCHK
//		_heapchk();
//#endif
//#endif
//		objectOut = object;
//		return bResult;
//	}
//
//	//-------------------------------------------------------------
//	//
//	//-------------------------------------------------------------
//	PYVM *ReadVM()
//	{
//		PYVM *pVM = sq_newthread( m_hVM, MIN_STACK_OVERHEAD + 2 );
//		m_hVM->Pop();
//		return pVM;
//	}
//
//	//-------------------------------------------------------------
//	//
//	//-------------------------------------------------------------
//	void ReadVM( PYVM *pVM )
//	{
//		unsigned i;
//
//		ReadObject( pVM->_roottable );
//		pVM->_top = m_pBuffer->GetInt();
//		pVM->_stackbase  =  m_pBuffer->GetInt();
//		unsigned stackSize = m_pBuffer->GetUnsignedInt();
//		pVM->_stack.resize( stackSize );
//		for( i = 0; i < pVM->_stack.size(); i++ ) 
//		{
//			ReadObject( pVM->_stack[i] );
//		}
//		stackSize = m_pBuffer->GetUnsignedInt();
//		for( i = 0; i < pVM->_vargsstack.size(); i++ ) 
//		{
//			ReadObject( pVM->_vargsstack[i] );
//		}
//	}
//
//	//-------------------------------------------------------------
//	//
//	//-------------------------------------------------------------
//	SQTable *ReadTable()
//	{
//		SQTable *pOld;
//		SQTable *pTable;
//
//		if ( !BeginRead( &pOld, &pTable ) )
//		{
//			return pTable;
//		}
//
//		pTable = SQTable::Create(_ss(m_hVM), 0);
//
//		MapPtr( pOld, pTable );
//
//		if ( m_pBuffer->GetInt() )
//		{
//			SQObjectPtr delegate;
//			ReadObject( delegate );
//			pTable->SetDelegate( delegate._unVal.pTable );
//		}
//		else
//		{
//			pTable->_delegate = NULL;
//		}
//		int n = m_pBuffer->GetInt();
//		while ( n-- )
//		{
//			SQObjectPtr key, value;
//			ReadObject( key );
//			if ( !ReadObject( value, ( key._type == OT_STRING ) ? key._unVal.pString->_val : NULL ) )
//			{
//				DevMsg( "Failed to read Python table entry %s", ( key._type == OT_STRING ) ? key._unVal.pString->_val : SQTypeToString( key._type ) );
//			}
//			if ( key._type != OT_NULL )
//			{
//				pTable->NewSlot( key, value );
//			}
//		}
//		return pTable;
//	}
//
//	//-------------------------------------------------------------
//	//
//	//-------------------------------------------------------------
	//SQArray *ReadArray()
	//{
	//	SQArray *pOld;
	//	SQArray *pArray;
	//	if ( !BeginRead( &pOld, &pArray ) )
	//	{
	//		return pArray;
	//	}

	//	pArray = SQArray::Create(_ss(m_hVM), 0);

	//	MapPtr( pOld, pArray );

	//	int n = m_pBuffer->GetInt();
	//	pArray->Reserve( n );

	//	while ( n-- )
	//	{
	//		SQObjectPtr value;
	//		ReadObject( value );
	//		pArray->Append( value );
	//	}
	//	return pArray;
	//}

	////-------------------------------------------------------------
	////
	////-------------------------------------------------------------
	//SQClass *ReadClass()
	//{
	//	SQClass *pOld;
	//	SQClass *pClass;
	//	if ( !BeginRead( &pOld, &pClass ) )
	//	{
	//		return pClass;
	//	}

	//	SQClass *pBase = NULL;
 //		bool bIsNative = !!m_pBuffer->GetInt();
	//	// If it's not a C++ defined type...
	//	if ( !bIsNative )
	//	{
	//		if ( m_pBuffer->GetInt() )
	//		{
	//			SQObjectPtr base;
	//			ReadObject( base );
	//			pBase = base._unVal.pClass;
	//		}

	//		SQClass *pClass = SQClass::Create( _ss(m_hVM), pBase );
	//		MapPtr( pOld, pClass );

	//		SQObjectPtr members;
	//		ReadObject( members );
	//		pClass->_members->Release();
	//		pClass->_members = members._unVal.pTable;
	//		__ObjAddRef( members._unVal.pTable );

	//		ReadObject( pClass->_attributes );
	//		unsigned i, n;

	//		n = m_pBuffer->GetUnsignedInt();
	//		pClass->_defaultvalues.resize( n );
	//		for ( i = 0; i < n; i++ ) 
	//		{
	//			ReadObject(pClass->_defaultvalues[i].val);
	//			ReadObject(pClass->_defaultvalues[i].attrs);
	//		}

	//		n = m_pBuffer->GetUnsignedInt();
	//		pClass->_methods.resize( n );
	//		for ( i = 0; i < n; i++ ) 
	//		{
	//			ReadObject(pClass->_methods[i].val);
	//			ReadObject(pClass->_methods[i].attrs);
	//		}

	//		n = m_pBuffer->GetUnsignedInt();
	//		pClass->_metamethods.resize( n );
	//		for ( i = 0; i < n; i++ ) 
	//		{
	//			ReadObject(pClass->_metamethods[i]);
	//		}
	//		return pClass;
	//	}
	//	else
	//	{
	//		char *pszName = (char *)stackalloc( 1024 );
	//		m_pBuffer->GetString( pszName, 1024 );
	//		pszName[1023] = 0;

	//		SQObjectPtr value;
	//		if ( m_hVM->_roottable._unVal.pTable->Get( SQString::Create( _ss(m_hVM ), pszName ), value ) && sq_isclass( value ) )
	//		{
	//			MapPtr( pOld, value._unVal.pClass );
	//			return value._unVal.pClass;
	//		}
	//		MapPtr( pOld, NULL );
	//	}
	//	return NULL;
	//}

	////-------------------------------------------------------------
	////
	////-------------------------------------------------------------
	//SQInstance *ReadInstance()
	//{
	//	SQInstance *pOld;
	//	SQInstance *pInstance;
	//	if ( !BeginRead( &pOld, &pInstance ) )
	//	{
	//		return pInstance;
	//	}

	//	SQObjectPtr pClass;
	//	ReadObject( pClass );

	//	unsigned i, n;
	//	if ( pClass._unVal.pClass )
	//	{
	//		pInstance = SQInstance::Create( _ss(m_hVM), pClass._unVal.pClass );

	//		n = m_pBuffer->GetUnsignedInt();
	//		for ( i = 0; i < n; i++ ) 
	//		{
	//			ReadObject(pInstance->_values[i]);
	//		}
	//		m_pBuffer->GetPtr(); // ignored in this path
	//		if ( pInstance->_class->_typetag )
	//		{
	//			if ( pInstance->_class->_typetag == TYPETAG_VECTOR )
	//			{
	//				Vector *pValue = new Vector;
	//				pValue->x = m_pBuffer->GetFloat();
	//				pValue->y = m_pBuffer->GetFloat();
	//				pValue->z = m_pBuffer->GetFloat();
	//				pInstance->_userpointer = pValue;
	//			}
	//			else
	//			{
	//				InstanceContext_t *pContext = new InstanceContext_t;
	//				pContext->pInstance = NULL;
	//				ReadObject( pContext->name );
	//				pContext->pClassDesc = (ScriptClassDesc_t *)( pInstance->_class->_typetag );
	//				void *pOldInstance = m_pBuffer->GetPtr();
	//				if ( sq_isstring(pContext->name) )
	//				{
	//					char *pszName = pContext->name._unVal.pString->_val;
	//					if ( pContext->pClassDesc->pHelper )
	//					{
	//						HPYOBJECT *pInstanceHandle = new HPYOBJECT;
	//						pInstanceHandle->_type = OT_INSTANCE;
	//						pInstanceHandle->_unVal.pInstance = pInstance;
	//						pContext->pInstance = pContext->pClassDesc->pHelper->BindOnRead( (HSCRIPT)pInstanceHandle, pOldInstance, pszName );
	//						if ( pContext->pInstance )
	//						{
	//							pInstance->_uiRef++;
	//							sq_addref( m_hVM, pInstanceHandle );
	//							pInstance->_uiRef--;
	//						}
	//						else
	//						{
	//							delete pInstanceHandle;
	//						}
	//					}

	//					if ( !pContext->pInstance )
	//					{
	//						// Look for a match in the current root table
	//						HPYOBJECT hExistingObject = LookupObject( pszName, NULL, false );
	//						if ( sq_isinstance(hExistingObject) && hExistingObject._unVal.pInstance->_class == pInstance->_class )
	//						{
	//							delete pInstance;
	//							return hExistingObject._unVal.pInstance;	
	//						}

	//						pContext->pInstance = NULL;
	//					}
	//				}
	//				pInstance->_userpointer = pContext;
	//			}
	//		}
	//		else 
	//		{
	//			Verify( m_pBuffer->GetInt() == OT_USERPOINTER );
	//			pInstance->_userpointer = ReadUserPointer();
	//			Assert( pInstance->_userpointer == NULL );
	//		}

	//		MapPtr( pOld, pInstance );
	//	}
	//	else
	//	{
	//		MapPtr( pOld, NULL );
	//		n = m_pBuffer->GetUnsignedInt();
	//		for ( i = 0; i < n; i++ ) 
	//		{
	//			SQObjectPtr ignored;
	//			ReadObject(ignored);
	//		}
	//		void *pOldTypeTag = m_pBuffer->GetPtr(); // ignored in this path

	//		if ( pOldTypeTag )
	//		{
	//			if ( pOldTypeTag == TYPETAG_VECTOR )
	//			{
	//				m_pBuffer->GetFloat();
	//				m_pBuffer->GetFloat();
	//				m_pBuffer->GetFloat();
	//			}
	//			else
	//			{
	//				SQObjectPtr ignored;
	//				ReadObject( ignored );
	//				m_pBuffer->GetPtr();
	//			}
	//		}
	//		else 
	//		{
	//			Verify( m_pBuffer->GetInt() == OT_USERPOINTER );
	//			ReadUserPointer();
	//		}
	//		pInstance = NULL;
	//	}
	//	return pInstance;
	//}

	////-------------------------------------------------------------
	////
	////-------------------------------------------------------------
	//SQGenerator *ReadGenerator()
	//{
	//	SQGenerator *pOld;
	//	SQGenerator *pGenerator;
	//	if ( !BeginRead( &pOld, &pGenerator ) )
	//	{
	//		return pGenerator;
	//	}

	//	SQObjectPtr closure;
	//	ReadObject( closure );

	//	pGenerator = SQGenerator::Create( _ss(m_hVM), closure._unVal.pClosure );
	//	MapPtr( pOld, pGenerator );

	//	unsigned i, n;
	//	n = m_pBuffer->GetUnsignedInt();
	//	pGenerator->_stack.resize( n );
	//	for ( i = 0; i < n; i++ ) 
	//	{
	//		ReadObject(pGenerator->_stack[i]);
	//	}
	//	n = m_pBuffer->GetUnsignedInt();
	//	pGenerator->_vargsstack.resize( n );
	//	for ( i = 0; i < n; i++ ) 
	//	{
	//		ReadObject(pGenerator->_vargsstack[i]);
	//	}
	//	return pGenerator;
	//}

	////-------------------------------------------------------------
	////
	////-------------------------------------------------------------
	//SQClosure *ReadClosure()
	//{
	//	SQClosure *pOld;
	//	SQClosure *pClosure;
	//	if ( !BeginRead( &pOld, &pClosure ) )
	//	{
	//		return pClosure;
	//	}

	//	SQObjectPtr proto;
	//	ReadObject( proto );
	//	pClosure = SQClosure::Create( _ss(m_hVM), proto._unVal.pFunctionProto );
	//	MapPtr( pOld, pClosure );

	//	ReadObject( pClosure->_env );

	//	unsigned i, n;
	//	n = m_pBuffer->GetUnsignedInt();
	//	pClosure->_outervalues.resize( n );
	//	for ( i = 0; i < n; i++ ) 
	//	{
	//		ReadObject(pClosure->_outervalues[i]);
	//	}

	//	n = m_pBuffer->GetUnsignedInt();
	//	pClosure->_defaultparams.resize( n );
	//	for ( i = 0; i < n; i++ ) 
	//	{
	//		ReadObject(pClosure->_defaultparams[i]);
	//	}

	//	return pClosure;
	//}

	////-------------------------------------------------------------
	////
	////-------------------------------------------------------------
	//SQNativeClosure *ReadNativeClosure()
	//{
	//	SQNativeClosure *pOld;
	//	SQNativeClosure *pClosure;
	//	if ( !BeginRead( &pOld, &pClosure ) )
	//	{
	//		return pClosure;
	//	}

	//	SQObjectPtr name;
	//	ReadObject( name );
	//	SQObjectPtr value;
	//	if ( m_hVM->_roottable._unVal.pTable->Get( name, value ) && sq_isnativeclosure(value) )
	//	{
	//		MapPtr( pOld, value._unVal.pNativeClosure );
	//		return value._unVal.pNativeClosure;
	//	}
	//	MapPtr( pOld, NULL );
	//	return NULL; // @TBD [4/15/2008 tom]
	//}

	////-------------------------------------------------------------
	////
	////-------------------------------------------------------------
	//SQUserData *ReadUserData()
	//{
	//	m_pBuffer->GetPtr();
	//	return NULL; // @TBD [4/15/2008 tom]
	//}

	////-------------------------------------------------------------
	////
	////-------------------------------------------------------------
	//SQUserPointer *ReadUserPointer()
	//{
	//	m_pBuffer->GetPtr();
	//	return NULL; // @TBD [4/15/2008 tom]
	//}

	////-------------------------------------------------------------
	////
	////-------------------------------------------------------------
	//static SQInteger SqReadFunc(SQUserPointer up,SQUserPointer data, SQInteger size)
 //	{
 //		CPythonVM *pThis = (CPythonVM *)up;
 //		pThis->m_pBuffer->Get( data, size );
 //		return size;
 //	}
 //
	////-------------------------------------------------------------
	////
	////-------------------------------------------------------------
	//SQFunctionProto *ReadFuncProto()
	//{
	//	SQFunctionProto *pOld;
	//	SQFunctionProto *pResult;
	//	if ( !BeginRead( &pOld, &pResult ) )
	//	{
	//		return pResult;
	//	}

	//	SQObjectPtr result;
	//	SQFunctionProto::Load( m_hVM, this, &SqReadFunc, result );
	//	pResult = result._unVal.pFunctionProto;
	//	pResult->_uiRef++;
	//	result.Null();
	//	pResult->_uiRef--;
	//	MapPtr( pOld, pResult );
	//	return pResult;
	//}

	////-------------------------------------------------------------
	////
	////-------------------------------------------------------------
	//SQWeakRef *ReadWeakRef( )
	//{
	//	SQObjectPtr obj;
	//	ReadObject( obj );
	//	if ( !obj._unVal.pRefCounted )
	//	{
	//		return NULL;
	//	}

	//	// Need to up ref count if read order has weak ref loading first
	//	Assert( ISREFCOUNTED(obj._type) );
	//	SQRefCounted *pRefCounted = obj._unVal.pRefCounted;
	//	pRefCounted->_uiRef++;

	//	SQWeakRef *pResult = obj._unVal.pRefCounted->GetWeakRef( obj._type );

	//	obj.Null();
	//	pRefCounted->_uiRef--;

	//	return pResult;
	//}

	////-------------------------------------------------------------
	////
	////-------------------------------------------------------------
	//bool FindKeyForObject( const SQObjectPtr &table, void *p, SQObjectPtr &key )
	//{
	//	SQTable *pTable = table._unVal.pTable;
	//	int len = pTable->_numofnodes;
	//	for(int i = 0; i < len; i++)
	//	{
	//		if ( pTable->_nodes[i].val._unVal.pUserPointer == p )
	//		{
	//			key = pTable->_nodes[i].key;
	//			return true;
	//		}
	//		if ( sq_istable( pTable->_nodes[i].val ) )
	//		{
	//			if ( FindKeyForObject( pTable->_nodes[i].val, p, key ) )
	//			{
	//				return true;
	//			}
	//		}
	//	}
	//	return false;
	//}

	// add instance object to consistency checker
	void debugTrackObject( PyObject *pobj )
	{
#ifdef DEBUG_PY
		if (m_debugObjCount < 1000)
			m_debugObjects[m_debugObjCount++] = pobj;
#endif // DEBUG_PY
	}

	void debugRemoveTrackedObject( PyObject *pobj )
	{
#ifdef DEBUG_PY
		for (int i = 0; i < m_debugObjCount; i++)
			if ( m_debugObjects[i] == pobj )
				m_debugObjects[i] = NULL;
#endif // DEBUG_PY
	}

	CPyScope		*m_pRootScope;							// __main__ module scope
	CPyScope		*m_pValveScope;							// valve module scope

public:
	
	int m_iMethodDef;
	int m_iClassDef;
	
	void *m_rgpMethodDefs[MAX_VALVE_FUNCTIONS_EXPORTED];	// array of pointers to fixed blocks of memory - passed to python, must not move!
	void *m_rgpClassDefs[MAX_VALVE_CLASSES_EXPORTED];
	
	bool m_bInitialized;

private:
	char			m_szScriptPath[MAX_PATH];				// full path to scripts/vscript directory
	int64			m_iUniqueIdSerialNumber;
	float			m_TimeStartExecute;

	int m_debugObjCount;
	PyObject *m_debugObjects[1000];

#ifdef VPYTHON_TEST
	ConVar			developer;
#else
	ConVarRef		developer;
#endif

	CUtlHashFast<PyTypeObject *, CUtlHashFastGenericHash> m_TypeMap;		// map pClassDesc to PyTypeObject python type
	CUtlHashFast<ScriptClassDesc_t *, CUtlHashFastGenericHash> m_ClassMap;	// map PyTypeObject to pClassDesc

	//	friend class CVPythonSerializer;

//	// Serialization support
//	CUtlBuffer *m_pBuffer;
//	CUtlMap<void *, void *> m_PtrMap;
};


//-------------------------------------------------------------------------
// Serialization and Debug Helpers
//-------------------------------------------------------------------------

//const char *FieldTypeToString( int type )
//{
//	switch( type )
//	{
//		case FIELD_VOID:		return "void";
//		case FIELD_FLOAT:		return "float";
//		case FIELD_CSTRING:		return "string";
//		case FIELD_VECTOR:		return "Vector";
//		case FIELD_INTEGER:		return "int";
//		case FIELD_BOOLEAN:		return "bool";
//		case FIELD_CHARACTER:	return "char";
//		case FIELD_HSCRIPT:		return "handle";
//		default:				return "<unknown>";
//	}
//}

//static const char *SQTypeToString( SQObjectType sqType )
//{
//	switch( sqType )
//	{
//		case OT_FLOAT:			return "FLOAT";
//		case OT_INTEGER:		return "INTEGER";
//		case OT_BOOL:			return "BOOL";
//		case OT_STRING:			return "STRING";
//		case OT_NULL:			return "NULL";
//		case OT_TABLE:			return "TABLE";
//		case OT_ARRAY:			return "ARRAY";
//		case OT_CLOSURE:		return "CLOSURE";
//		case OT_NATIVECLOSURE:	return "NATIVECLOSURE";
//		case OT_USERDATA:		return "USERDATA";
//		case OT_GENERATOR:		return "GENERATOR";
//		case OT_THREAD:			return "THREAD";
//		case OT_USERPOINTER:	return "USERPOINTER";
//		case OT_CLASS:			return "CLASS";
//		case OT_INSTANCE:		return "INSTANCE";
//		case OT_WEAKREF:		return "WEAKREF";
//	}
//	return "<unknown>";
//}

//----------------------------------------------------------------------------------
// return true if interpreter init is finalized - block attempts at re-init.
//----------------------------------------------------------------------------------
inline bool VMInitFinalized( void )
{
	return ((CPythonVM *)g_pVm)->m_bInitialized;
}

//----------------------------------------------------------------------------------
// create interpreter singleton and save pointer to interface in g_pVm
//----------------------------------------------------------------------------------
IScriptVM *ScriptCreatePythonVM()
{
	if ( !g_pVm )
	{
		g_pVm = new CPythonVM;
	}
	else
	{
		// set semaphore to block more than one init of interpreter, 
		// this blocks registration of any more classes or functions, but not of instances
		((CPythonVM *)g_pVm)->m_bInitialized = true;	
	}

	return (IScriptVM *)g_pVm;
}

//----------------------------------------------------------------------------------
// called after shutdown() on restart or level load or exit
// NOTE: we should not actually kill the interpreter unless it's a full game exit.
// UNDONE: we don't cleanly shut down the interpeter on game exit.
//----------------------------------------------------------------------------------
void ScriptDestroyPythonVM( IScriptVM *pVm )
{
	// UNDONE: ivscript interface needs a flag to indicate game exit - the only case we should actually kill the interpreter
	bool bGameExit = false; 

	if ( bGameExit )
	{

		// release memory associated with dynamically allocated data
		CPythonVM *pPythonVM = assert_cast<CPythonVM *>( pVm );

		if (1)
		{	// UNDONE: ONE shutdown per game session only! 
			// whether these are allocated using new/delete, python PyObject_Malloc/Free, 
			// this dealloc causes the next Py_Initialize/Py_Finalize to fail intermittently (ie on restart).
			// Python documentation indicates that Py_Finalize does NOT reliably release all allocated resources,
			// and users of various versions report problems with Py_Finalize followed by a subsequent
			// Py_Initialize.  

			int i;

			for (i = 0; i < pPythonVM->m_iMethodDef; i++)
			{
				PyMem_Free( pPythonVM->m_rgpMethodDefs[i] );
			}

			for (i = 0; i < pPythonVM->m_iClassDef; i++)
			{
				PyMem_Free( pPythonVM->m_rgpClassDefs[i] );
			}
			pPythonVM->m_iMethodDef = 0;
			pPythonVM->m_iClassDef = 0;

		}


		if (1) // TEST2:
			delete pPythonVM;
	}

}

//-------------------------------------------------------------------
// C function & method call proxies:
// Do a bunch of proxy work to expose server-side functions/methods
// with their specific bindings to python.  
//
// Since python does not support
// specifying user data along with c-defined function/method callbacks, 
// and we need specific python->c binding data for each function/method callback, 
// we have to create unique proxy functions for every callback.
//
// Each unique proxy gets the correct binding info for the function, 
// then calls the general TranslateCall routine to do the dispatch. 
//-------------------------------------------------------------------

//------------------------------------------------------------------
// define a static callback proxy - python calls this back to access 
// a function or method definition of a valve class
//-----------------------------------------------------------------
#define DPX(N) \
	static PyObject *Translate_##N( PyObject *pSelf, PyObject *pArgs ) \
	{ \
		ScriptFunctionBinding_t *pBinding = GetProxyBinding( ##N ); \
		return ( CPythonVM::TranslateCall( pBinding, (scriptClassInstance_t *)pSelf, pArgs ) ); \
	} \

// set proxy definition
#define SPX(N) g_proxies[##N].pfn = Translate_##N;

static int g_proxyid = 0;

static int GetNewProxyId( void )
{
	int proxyId = g_proxyid;
	Assert ( proxyId < MAX_VALVE_FUNCTIONS_EXPORTED ); // if this fails, just need to bump up MAX_VALVE_FUNCTIONS_EXPORTED and add more DPX(n),SPX(n) entries
	g_proxyid++;
	return proxyId;
}

// associate the function and/or class binding with the proxy function id
static void SetProxyBinding( int proxyId, ScriptFunctionBinding_t *pBinding )
{
	Assert( proxyId < MAX_VALVE_FUNCTIONS_EXPORTED ); // if this fails, just need to bump up MAX_VALVE_FUNCTIONS_EXPORTED and add more DPX(n) entries
	g_proxies[proxyId].pBinding = pBinding;
}

static PyCFunction GetProxyFunction( int proxyId )
{
	Assert( proxyId < MAX_VALVE_FUNCTIONS_EXPORTED ); // invalid proxyId
	return g_proxies[proxyId].pfn;
}

// retrieve the function binding, given the proxy index
static ScriptFunctionBinding_t *GetProxyBinding( int proxyId )
{
	Assert( proxyId < MAX_VALVE_FUNCTIONS_EXPORTED ); // invalid proxyId
	return ( g_proxies[proxyId].pBinding );
}

DPX(0) DPX(1) DPX(2) DPX(3) DPX(4) DPX(5) DPX(6) DPX(7) DPX(8) DPX(9) DPX(10) DPX(11) DPX(12) DPX(13) DPX(14) DPX(15) DPX(16) DPX(17) DPX(18) DPX(19)
DPX(20) DPX(21) DPX(22) DPX(23) DPX(24) DPX(25) DPX(26) DPX(27) DPX(28) DPX(29) DPX(30) DPX(31) DPX(32) DPX(33) DPX(34) DPX(35) DPX(36) DPX(37) DPX(38) DPX(39)
DPX(40) DPX(41) DPX(42) DPX(43) DPX(44) DPX(45) DPX(46) DPX(47) DPX(48) DPX(49) DPX(50) DPX(51) DPX(52) DPX(53) DPX(54) DPX(55) DPX(56) DPX(57) DPX(58) DPX(59) 
DPX(60) DPX(61) DPX(62) DPX(63) DPX(64) DPX(65) DPX(66) DPX(67) DPX(68) DPX(69) DPX(70) DPX(71) DPX(72) DPX(73) DPX(74) DPX(75) DPX(76) DPX(77) DPX(78) DPX(79) 
DPX(80) DPX(81) DPX(82) DPX(83) DPX(84) DPX(85) DPX(86) DPX(87) DPX(88) DPX(89) DPX(90) DPX(91) DPX(92) DPX(93) DPX(94) DPX(95) DPX(96) DPX(97) DPX(98) DPX(99) 
DPX(100) DPX(101) DPX(102) DPX(103) DPX(104) DPX(105) DPX(106) DPX(107) DPX(108) DPX(109) DPX(110) DPX(111) DPX(112) DPX(113) DPX(114) DPX(115) DPX(116) DPX(117) DPX(118) DPX(119) 
DPX(120) DPX(121) DPX(122) DPX(123) DPX(124) DPX(125) DPX(126) DPX(127) DPX(128) DPX(129) DPX(130) DPX(131) DPX(132) DPX(133) DPX(134) DPX(135) DPX(136) DPX(137) DPX(138) DPX(139) 
DPX(140) DPX(141) DPX(142) DPX(143) DPX(144) DPX(145) DPX(146) DPX(147) DPX(148) DPX(149) DPX(150) DPX(151) DPX(152) DPX(153) DPX(154) DPX(155) DPX(156) DPX(157) DPX(158) DPX(159) 
DPX(160) DPX(161) DPX(162) DPX(163) DPX(164) DPX(165) DPX(166) DPX(167) DPX(168) DPX(169) DPX(170) DPX(171) DPX(172) DPX(173) DPX(174) DPX(175) DPX(176) DPX(177) DPX(178) DPX(179) 
DPX(180) DPX(181) DPX(182) DPX(183) DPX(184) DPX(185) DPX(186) DPX(187) DPX(188) DPX(189) DPX(190) DPX(191) DPX(192) DPX(193) DPX(194) DPX(195) DPX(196) DPX(197) DPX(198) DPX(199) 
DPX(200) DPX(201) DPX(202) DPX(203) DPX(204) DPX(205) DPX(206) DPX(207) DPX(208) DPX(209) DPX(210) DPX(211) DPX(212) DPX(213) DPX(214) DPX(215) DPX(216) DPX(217) DPX(218) DPX(219) 
DPX(220) DPX(221) DPX(222) DPX(223) DPX(224) DPX(225) DPX(226) DPX(227) DPX(228) DPX(229) DPX(230) DPX(231) DPX(232) DPX(233) DPX(234) DPX(235) DPX(236) DPX(237) DPX(238) DPX(239) 
DPX(240) DPX(241) DPX(242) DPX(243) DPX(244) DPX(245) DPX(246) DPX(247) DPX(248) DPX(249) DPX(250) DPX(251) DPX(252) DPX(253) DPX(254) DPX(255) DPX(256) DPX(257) DPX(258) DPX(259) 

static void InitProxyTable( void )
{
	g_proxyid = 0;

SPX(0) SPX(1) SPX(2) SPX(3) SPX(4) SPX(5) SPX(6) SPX(7) SPX(8) SPX(9) SPX(10) SPX(11) SPX(12) SPX(13) SPX(14) SPX(15) SPX(16) SPX(17) SPX(18) SPX(19)
SPX(20) SPX(21) SPX(22) SPX(23) SPX(24) SPX(25) SPX(26) SPX(27) SPX(28) SPX(29) SPX(30) SPX(31) SPX(32) SPX(33) SPX(34) SPX(35) SPX(36) SPX(37) SPX(38) SPX(39)
SPX(40) SPX(41) SPX(42) SPX(43) SPX(44) SPX(45) SPX(46) SPX(47) SPX(48) SPX(49) SPX(50) SPX(51) SPX(52) SPX(53) SPX(54) SPX(55) SPX(56) SPX(57) SPX(58) SPX(59) 
SPX(60) SPX(61) SPX(62) SPX(63) SPX(64) SPX(65) SPX(66) SPX(67) SPX(68) SPX(69) SPX(70) SPX(71) SPX(72) SPX(73) SPX(74) SPX(75) SPX(76) SPX(77) SPX(78) SPX(79) 
SPX(80) SPX(81) SPX(82) SPX(83) SPX(84) SPX(85) SPX(86) SPX(87) SPX(88) SPX(89) SPX(90) SPX(91) SPX(92) SPX(93) SPX(94) SPX(95) SPX(96) SPX(97) SPX(98) SPX(99) 
SPX(100) SPX(101) SPX(102) SPX(103) SPX(104) SPX(105) SPX(106) SPX(107) SPX(108) SPX(109) SPX(110) SPX(111) SPX(112) SPX(113) SPX(114) SPX(115) SPX(116) SPX(117) SPX(118) SPX(119) 
SPX(120) SPX(121) SPX(122) SPX(123) SPX(124) SPX(125) SPX(126) SPX(127) SPX(128) SPX(129) SPX(130) SPX(131) SPX(132) SPX(133) SPX(134) SPX(135) SPX(136) SPX(137) SPX(138) SPX(139) 
SPX(140) SPX(141) SPX(142) SPX(143) SPX(144) SPX(145) SPX(146) SPX(147) SPX(148) SPX(149) SPX(150) SPX(151) SPX(152) SPX(153) SPX(154) SPX(155) SPX(156) SPX(157) SPX(158) SPX(159) 
SPX(160) SPX(161) SPX(162) SPX(163) SPX(164) SPX(165) SPX(166) SPX(167) SPX(168) SPX(169) SPX(170) SPX(171) SPX(172) SPX(173) SPX(174) SPX(175) SPX(176) SPX(177) SPX(178) SPX(179) 
SPX(180) SPX(181) SPX(182) SPX(183) SPX(184) SPX(185) SPX(186) SPX(187) SPX(188) SPX(189) SPX(190) SPX(191) SPX(192) SPX(193) SPX(194) SPX(195) SPX(196) SPX(197) SPX(198) SPX(199) 
SPX(200) SPX(201) SPX(202) SPX(203) SPX(204) SPX(205) SPX(206) SPX(207) SPX(208) SPX(209) SPX(210) SPX(211) SPX(212) SPX(213) SPX(214) SPX(215) SPX(216) SPX(217) SPX(218) SPX(219) 
SPX(220) SPX(221) SPX(222) SPX(223) SPX(224) SPX(225) SPX(226) SPX(227) SPX(228) SPX(229) SPX(230) SPX(231) SPX(232) SPX(233) SPX(234) SPX(235) SPX(236) SPX(237) SPX(238) SPX(239) 
SPX(240) SPX(241) SPX(242) SPX(243) SPX(244) SPX(245) SPX(246) SPX(247) SPX(248) SPX(249) SPX(250) SPX(251) SPX(252) SPX(253) SPX(254) SPX(255) SPX(256) SPX(257) SPX(258) SPX(259) 
}



#ifdef VPYTHON_TEST
CPythonVM g_PythonVM;
IScriptVM *g_pScriptVM = &g_PythonVM;


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

#include <time.h>

int main( int argc, const char **argv)
{
	if ( argc < 2 )
	{
		printf( "No script specified" );
		return 1;
	}
	g_pScriptVM->Init();

	const char *pszbuild;
	pszbuild = Py_GetVersion( ) ;

	printf("You should be using python 2.5.x. You are using python \"%s\".\n", pszbuild);

	// run a script without a source file - remote debugger will ask for a source file
	g_pScriptVM->Run(
					 "import pydevd\n"
					 //"pydevd.settrace(stdoutToServer = True, stderrToServer = True, port = 5678, suspend = True)\n"
					 "from time import time,ctime\n"
                     "print 'Today is',ctime(time())\n"
					 );



	//CCycleCount count;
	//count.Sample();
	//RandomSeed( time( NULL ) ^ count.GetMicroseconds() );
	ScriptRegisterFunction( g_pScriptVM, RandomFloat, "" );
	ScriptRegisterFunction( g_pScriptVM, RandomInt, "" );

	if ( argc == 3 && *argv[2] == 'd' )
	{
		g_pScriptVM->ConnectDebugger();
		
		// exec a script that invokes the remote python debugger in eclipse pydev
		// NOTE: this is the only valid way to get an hFile to use with Py_ calls. FILE * returned from fopen is not compatible.
		PyObject* pyfile = PyFile_FromString( "debug.py", "r");
		FILE* hFile = PyFile_AsFile( pyfile); 
		int ret = PyRun_AnyFile(hFile, "debug.py");
		Py_XDECREF(pyfile);
	}

	int key;
	CScriptScope scope;
	scope.Init( "TestScope" );
	do 
	{
		const char *pszScript = argv[1];
		FILE *hFile = fopen( pszScript, "rb" );
		if ( !hFile )
		{
			printf( "\"%s\" not found.\n", pszScript );
			return 1;
		}

		int nFileLen = _filelength( _fileno( hFile ) );
		char *pBuf = new char[nFileLen + 1];
		fread( pBuf, 1, nFileLen, hFile );
		pBuf[nFileLen] = 0;
		fclose( hFile );
		
		if (1)
		{
			printf( "Executing script \"%s\"\n----------------------------------------\n", pszScript );
			HSCRIPT hScript = g_pScriptVM->CompileScript( pBuf, ( strrchr( pszScript, '\\' ) ? strrchr( pszScript, '\\' ) + 1 : pszScript ) );
			if ( hScript )
			{
				if ( scope.Run( hScript ) != SCRIPT_ERROR )
				{
					printf( "----------------------------------------\n" );
					printf("Script complete.  Press q to exit, m to dump memory usage, enter to run again.\n");
				}
				else
				{
					printf( "----------------------------------------\n" );
					printf("Script execution error.  Press q to exit, m to dump memory usage, enter to run again.\n");
				}
				g_pScriptVM->ReleaseScript( hScript );
			}
			else
			{
				printf( "----------------------------------------\n" );
				printf("Script failed to compile.  Press q to exit, m to dump memory usage, enter to run again.\n");
			}
		}
 		key = _getch(); // Keypress before exit
		if ( key == 'm' )
		{
			Msg( "%d\n", g_pMemAlloc->GetSize( NULL ) );
		}
		delete pBuf;
	} while ( key != 'q' );

	scope.Term();
	g_pScriptVM->DisconnectDebugger();

	g_pScriptVM->Shutdown();
	return 0;
}

#endif
