
//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UIEVENT_H
#define UIEVENT_H
#pragma once

#include "tier1/utlsymbol.h"
#include "tier1/utldelegate.h"
#include "controls/panelhandle.h"
#include "controls/panelptr.h"
#include "iuipanel.h"
#include "iuipanelclient.h"
#include "tier1/fmtstr.h"
#include "panoramacxx.h"

// Strict template usage is just a dev-time convenience
// for finding missing template specializations.
// It should never be enabled by default as there are
// types that do not have or need template support by-design.
#if 0
#define PANORAMA_STRICT_EVENT_TEMPLATE_USAGE
#endif
#if 0
#define PANORAMA_STRICT_V8_TEMPLATE_USAGE
#endif
#if defined(PANORAMA_STRICT_EVENT_TEMPLATE_USAGE) || defined(PANORAMA_STRICT_V8_TEMPLATE_USAGE)
#define PANORAMA_ANY_STRICT_EVENT_TEMPLATE_USAGE
#endif

namespace panorama
{


extern void RegisterEventTypesWithEngine( IUIEngine *pEngine );

#ifdef DBGFLAG_VALIDATE
void ValidateGlobalEvents( CValidator &validator );
#endif

inline bool IsEndOfUIEventString( const char *pchEvent, const char **pchEndOfEvent )
{
	{
		while( pchEvent[0] != '\0' && pchEvent[0] != ')' )
		{
			if( !V_isspace( pchEvent[0] ) )
				return false;

			pchEvent++;
		}

		if( pchEvent[0] == ')' )
			++pchEvent;

		*pchEndOfEvent = pchEvent;

		return true;
	}
}
class IUIEvent;

inline v8::Isolate *GetV8Isolate() { return UIEngine()->GetV8Isolate(); }

inline const char *GetPanelID( const panorama::IUIPanel *pPanel ) { return pPanel->GetID(); }

//-----------------------------------------------------------------------------
// Purpose: Helpers to create an event from string
//-----------------------------------------------------------------------------
template < typename T >
bool ParseUIEventParam( T *pOut, panorama::IUIPanel *pPanel, const char *pchEvent, const char **pchNextParam )
{
#ifdef PANORAMA_STRICT_EVENT_TEMPLATE_USAGE
	TEMPLATE_USAGE_INVALID( T );
#else
	AssertMsg( false, "ParseUIEventParam not implemented for type" );
	// Zero-fill so that the compiler doesn't complain about use
	// of uninitialized data, even though this code path is not functional.
	memset( pOut, 0, sizeof(*pOut) );
	return false;
#endif
}

template <> bool ParseUIEventParam< const char * >( const char **pOut, panorama::IUIPanel *pPanel, const char *pchEvent, const char **pchNextParam );
template <> bool ParseUIEventParam< uint8 >( uint8 *pOut, panorama::IUIPanel *pPanel, const char *pchEvent, const char **pchNextParam );
template <> bool ParseUIEventParam< uint16 >( uint16 *pOut, panorama::IUIPanel *pPanel, const char *pchEvent, const char **pchNextParam );
template <> bool ParseUIEventParam< uint32 >( uint32 *pOut, panorama::IUIPanel *pPanel, const char *pchEvent, const char **pchNextParam );
template <> bool ParseUIEventParam< uint64 >( uint64 *pOut, panorama::IUIPanel *pPanel, const char *pchEvent, const char **pchNextParam );
template <> bool ParseUIEventParam< int32 >( int32 *pOut, panorama::IUIPanel *pPanel, const char *pchEvent, const char **pchNextParam );
template <> bool ParseUIEventParam< int64 >( int64 *pOut, panorama::IUIPanel *pPanel, const char *pchEvent, const char **pchNextParam );
template <> bool ParseUIEventParam< float >( float *pOut, panorama::IUIPanel *pPanel, const char *pchEvent, const char **pchNextParam );
template <> bool ParseUIEventParam< bool >( bool *pOut, panorama::IUIPanel *pPanel, const char *pchEvent, const char **pchNextParam );
template <> bool ParseUIEventParam< IUIEvent * >( IUIEvent **pOut, panorama::IUIPanel *pPanel, const char *pchEvent, const char **pchNextParam );
template <> bool ParseUIEventParam< panorama::EPanelEventSource_t >( panorama::EPanelEventSource_t *pOut, panorama::IUIPanel *pPanel, const char *pchEvent, const char **pchNextParam );
template <> bool ParseUIEventParam< panorama::ScrollBehavior_t >( panorama::ScrollBehavior_t *pOut, panorama::IUIPanel *pPanel, const char *pchEvent, const char **pchNextParam );

int CountUIEventParams( const char *pchParams );
bool ParseUIEventParamHelper( CUtlBuffer &bufValue, const char *pchEvent, const char **pchNextParam );
bool IsEndOfUIEventString( const char *pchEvent, const char **pchEndOfEvent );

//-----------------------------------------------------------------------------
// Purpose: Helpers to turn event params in JS params
//-----------------------------------------------------------------------------

// Default, complain not implemented!
template < typename T > typename panorama_enable_if< !panorama_is_enum< T >::value, void>::type 
PanoramaTypeToV8Param( T &pIn, v8::Handle<v8::Value> *pValueOut )
{
#ifdef PANORAMA_STRICT_V8_TEMPLATE_USAGE
	TEMPLATE_USAGE_INVALID( T );
#else
	AssertMsg( false, "EventParamToV8Param not implemented for type" );
#endif
}

// Default for enum types
template < typename T > typename panorama_enable_if< panorama_is_enum< T >::value, void>::type
PanoramaTypeToV8Param( T &pIn, v8::Handle<v8::Value> *pValueOut )
{
	COMPILE_TIME_ASSERT( sizeof( pIn ) <= sizeof( int32 ) );
	int32 iVal = (int32)pIn;
	return PanoramaTypeToV8Param<int32>( iVal, pValueOut );
}

// Basic non pointer specializations
template <> void PanoramaTypeToV8Param< CUtlSymbol >( CUtlSymbol &pIn, v8::Handle<v8::Value> *pValueOut );
template <> void PanoramaTypeToV8Param< panorama::CPanoramaSymbol >( panorama::CPanoramaSymbol &pIn, v8::Handle<v8::Value> *pValueOut );
template <> void PanoramaTypeToV8Param< uint32 >( uint32 &pIn, v8::Handle<v8::Value> *pValueOut );
template <> void PanoramaTypeToV8Param< uint64 >( uint64 &pIn, v8::Handle<v8::Value> *pValueOut );
template <> void PanoramaTypeToV8Param< int32 >( int32 &pIn, v8::Handle<v8::Value> *pValueOut );
template <> void PanoramaTypeToV8Param< int64 >( int64 &pIn, v8::Handle<v8::Value> *pValueOut );
template <> void PanoramaTypeToV8Param< float >( float &pIn, v8::Handle<v8::Value> *pValueOut );
template <> void PanoramaTypeToV8Param< double >( double &pIn, v8::Handle<v8::Value> *pValueOut );
template <> void PanoramaTypeToV8Param< bool >( bool &pIn, v8::Handle<v8::Value> *pValueOut );
template <> void PanoramaTypeToV8Param< CUtlVector< IUIPanel * > const >( CUtlVector< IUIPanel * > const &pIn, v8::Handle<v8::Value> *pValueOut );
template <> void PanoramaTypeToV8Param< panorama::EPanelEventSource_t >( panorama::EPanelEventSource_t &pIn, v8::Handle<v8::Value> *pValueOut );
template <> void PanoramaTypeToV8Param< v8::Local<v8::Value> >( v8::Local<v8::Value> &pIn, v8::Handle<v8::Value> *pValueOut );
template <> void PanoramaTypeToV8Param< v8::Local<v8::Object> >( v8::Local<v8::Object> &pIn, v8::Handle<v8::Value> *pValueOut );
template <> void PanoramaTypeToV8Param< v8::Local<v8::Array> >( v8::Local<v8::Array> &pIn, v8::Handle<v8::Value> *pValueOut );

// Helper for specific pointer types we have special handling for via rules in the base T* specialization
void PanoramaTypeToV8ParamJSObject( IUIJSObject *pJSObj, void *pIn, v8::Handle<v8::Value> *pValueOut );
void PanoramaPanelTypeToV8Param( IUIPanel * &pIn, v8::Handle<v8::Value> *pValueOut );
void PanoramaPanelStyleTypeToV8Param( IUIPanelStyle * &pIn, v8::Handle<v8::Value> *pValueOut );

// Template overload for ptr types, so we can see if we should turn into base CPanel2D, or other special known types,
// otherwise we'll call PanoramaPtrTypeToV8ParamJSObject and let the specialization rules figure it out
template < typename T >
void PanoramaTypeToV8Param( T * pIn, v8::Handle<v8::Value> *pValueOut )
{
	if( panorama_is_base_of< IUIPanelClient, T >::value )
	{
		IUIPanel *pPanel = pIn ? ( (IUIPanelClient*)pIn )->UIPanel() : NULL;
		return PanoramaPanelTypeToV8Param( pPanel, pValueOut );
	}
	else if( panorama_is_base_of< IUIPanel, T >::value )
	{
		IUIPanel *pPanel = (IUIPanel*)pIn;
		return PanoramaPanelTypeToV8Param( pPanel, pValueOut );
	}
	else if( panorama_is_base_of< IUIPanelStyle, T >::value )
	{
		IUIPanelStyle *pPanel = (IUIPanelStyle*)pIn;
		return PanoramaPanelStyleTypeToV8Param( pPanel, pValueOut );
	}
	else if( panorama_is_base_of< IUIJSObject, T>::value )
	{
		IUIJSObject *pObject = (IUIJSObject*)pIn;
		return PanoramaTypeToV8ParamJSObject( pObject, (void*)pIn, pValueOut );
	}

	AssertMsg( false, "PanoramaTypeToV8Param not implemented for type" );
}

// Specialization of above ptr overload
template <> void PanoramaTypeToV8Param<const char>( const char * pIn, v8::Handle<v8::Value> *pValueOut );
template <> void PanoramaTypeToV8Param<char>( char * pIn, v8::Handle<v8::Value> *pValueOut );

// bugbug jmccaskey - add IUIEvent, panel source, panel2d? more?


//-----------------------------------------------------------------------------
// Purpose: Helpers to turn JS params into native params
//-----------------------------------------------------------------------------
template < typename T >
void V8ParamToPanoramaType( const v8::Handle<v8::Value> &pValueIn, T *out )
{
#ifdef PANORAMA_STRICT_V8_TEMPLATE_USAGE
	TEMPLATE_USAGE_INVALID( T );
#else
	AssertMsg( false, "V8ParamToPanoramaType not implemented for type" );
#endif
}
template <> void V8ParamToPanoramaType< const char *>( const v8::Handle<v8::Value> &pValueIn, const char ** out );
template <> void V8ParamToPanoramaType< CUtlSymbol >( const v8::Handle<v8::Value> &pValueIn, CUtlSymbol* out );
template <> void V8ParamToPanoramaType< panorama::CPanoramaSymbol >( const v8::Handle<v8::Value> &pValueIn, panorama::CPanoramaSymbol* out );
template <> void V8ParamToPanoramaType< float >( const v8::Handle<v8::Value> &pValueIn, float *out );
template <> void V8ParamToPanoramaType< double >( const v8::Handle<v8::Value> &pValueIn, double *out );
template <> void V8ParamToPanoramaType< int >( const v8::Handle<v8::Value> &pValueIn, int *out );
template <> void V8ParamToPanoramaType< uint >( const v8::Handle<v8::Value> &pValueIn, uint *out );
template <> void V8ParamToPanoramaType< bool >( const v8::Handle<v8::Value> &pValueIn, bool *out );
template <> void V8ParamToPanoramaType< IUIPanel * >( const v8::Handle<v8::Value> &pValueIn, IUIPanel **out );
#ifndef PANORAMA_EXPORTS
template <> void V8ParamToPanoramaType< CPanel2D * >( const v8::Handle<v8::Value> &pValueIn, CPanel2D **out );
#endif
template <> void V8ParamToPanoramaType< IUIPanelStyle * >( const v8::Handle<v8::Value> &pValueIn, IUIPanelStyle **out );
#ifdef PANORAMA_EXPORTS
template <> void V8ParamToPanoramaType< CPanelStyle * >( const v8::Handle<v8::Value> &pValueIn, CPanelStyle **out );
#endif
template <> void V8ParamToPanoramaType< v8::Persistent<v8::Function> * >( const v8::Handle<v8::Value> &pValueIn, v8::Persistent<v8::Function> **out );
template <> void V8ParamToPanoramaType< CUtlVector< IUIPanel *> >( const v8::Handle<v8::Value> &pValueIn, CUtlVector< IUIPanel *>*out );
template <> void V8ParamToPanoramaType< v8::Local<v8::Value> >( const v8::Handle<v8::Value> &pValueIn, v8::Local<v8::Value> *out );
template <> void V8ParamToPanoramaType< v8::Local<v8::Object> >( const v8::Handle<v8::Value> &pValueIn, v8::Local<v8::Object> *out );
template <> void V8ParamToPanoramaType< v8::Local<v8::Array> >( const v8::Handle<v8::Value> &pValueIn, v8::Local<v8::Array> *out );

template <typename T> void FreeConvertedParam(T out) {  }
template <> void FreeConvertedParam< const char *>( const char *out );
template <> void FreeConvertedParam< v8::Persistent<v8::Function> *>( v8::Persistent<v8::Function> *out );

//-----------------------------------------------------------------------------
// Purpose: Wrappers to handle copying params. const char * is specialized to dup the string
//-----------------------------------------------------------------------------
template < class T >
void UIEventSet( T* pTo, T &pFrom )
{
	*pTo = pFrom;
}

template <>
void UIEventSet( const char** pTo, const char *&pFrom );


template <>
void UIEventSet( IUIEvent** pTo, IUIEvent *&pFrom );


template <>
void UIEventSet( v8::Persistent<v8::Function>** pTo, v8::Persistent<v8::Function> *&pFrom );

template < class T >
void UIEventFree( T &p )
{
}

template <>
void UIEventFree( const char *& p );


template <>
void UIEventFree( IUIEvent *& p );

template <>
void UIEventFree( v8::Persistent<v8::Function> *&p );

#ifdef DBGFLAG_VALIDATE

template < class T >
void UIEventValidate( CValidator &validator, T &p )
{
}

template <>
void UIEventValidate( CValidator &validator, const char *& p );


template <>
void UIEventValidate( CValidator &validator, IUIEvent *& p );

template <>
void UIEventValidate( CValidator &validator, v8::Persistent<v8::Function> *& p );

#endif

//-----------------------------------------------------------------------------
// Purpose: Macros to declare a event
//			Includes Register/Unregister calls to enforce type safety
//-----------------------------------------------------------------------------

//
// Events with 0 params
//
namespace UIEvent
{
	template < class T >
	IUIEvent *CreateEventFromString( panorama::IUIPanel *pPanel, const char *pchEvent, const char **pchEventEnd  )
	{
		if ( !IsEndOfUIEventString( pchEvent, &pchEvent ) )
			return NULL;

		*pchEventEnd = pchEvent;
		IUIEvent *pEvent = T::MakeEvent( pPanel ? pPanel->ClientPtr() : NULL );
		return pEvent;
	}
}


#define DECLARE_PANORAMA_EVENT0( name ) \
	class  name \
	{ \
	public: \
		static const int cParams = 0; static const bool bPanelEvent = false; static panorama::CPanoramaSymbol symbol; static const char *pchEvent;\
		static panorama::CPanoramaSymbol GetEventType() { Assert( symbol.IsValid() ); return symbol; } \
		static panorama::IUIEvent *MakeEvent( const panorama::IUIPanelClient *pTarget ) { return new panorama::CUIEvent0( symbol, pTarget ? pTarget->UIPanel() : NULL ); } \
		static panorama::IUIEvent *CreateEventFromString( panorama::IUIPanel *pPanel, const char *pchEventCreate, const char **pchEventEnd  ) { return panorama::UIEvent::CreateEventFromString< name >( pPanel, pchEventCreate, pchEventEnd ); } \
	}; \
	template< class T, class U > void RegisterEventHandler( const name &t, T *pHandlerPanel, bool (U::*memberfunc)() ) { panorama::UIEngine()->RegisterEventHandler( t.GetEventType(), pHandlerPanel, UtlMakeDelegate( pHandlerPanel, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void UnregisterEventHandler( const name &t, T *pHandlerPanel, bool (U::*memberfunc)() ) { panorama::UIEngine()->UnregisterEventHandler( t.GetEventType(), pHandlerPanel, UtlMakeDelegate( pHandlerPanel, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void RegisterEventHandlerOnPanel( const name &t, panorama::IUIPanel *pPanel, T *pHandler, bool (U::*memberfunc)() ) { panorama::UIEngine()->RegisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pHandler, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void UnregisterEventHandlerOnPanel( const name &t, panorama::IUIPanel *pPanel, T *pHandler, bool (U::*memberfunc)() ) { panorama::UIEngine()->UnregisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pHandler, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void RegisterForUnhandledEvent( const name &t, T *pHandlerObj, bool (U::*memberfunc)() ) { panorama::UIEngine()->RegisterForUnhandledEvent( t.GetEventType(), UtlMakeDelegate( pHandlerObj, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void UnregisterForUnhandledEvent( const name &t, T *pHandlerObj, bool (U::*memberfunc)() ) { panorama::UIEngine()->UnregisterForUnhandledEvent( t.GetEventType(), UtlMakeDelegate( pHandlerObj, memberfunc ).GetAbstractDelegate() ); } \
	template< class PanelType > void RegisterEventHandlerOnPanelType( const name &t, bool (PanelType::*memberfunc)() ) { panorama::UIEngine()->RegisterPanelTypeEventHandler( t.GetEventType(), PanelType::GetPanelSymbol(), UtlMakeDelegate( (PanelType*)NULL, memberfunc ).GetAbstractDelegate() ); }

#define DECLARE_PANEL_EVENT0( name ) \
class  name \
	{ \
	public: \
		static const int cParams = 0; static const bool bPanelEvent = true; static panorama::CPanoramaSymbol symbol; static const char *pchEvent;\
		static panorama::CPanoramaSymbol GetEventType() { Assert( symbol.IsValid() ); return symbol; } \
		static panorama::IUIEvent *MakeEvent( const panorama::IUIPanelClient *pTarget ) { return new panorama::CUIPanelEvent0( symbol, pTarget ? pTarget->UIPanel() : NULL ); } \
		static panorama::IUIEvent *CreateEventFromString( panorama::IUIPanel *pPanel, const char *pchEventCreate, const char **pchEventEnd  ) { return panorama::UIEvent::CreateEventFromString< name >( pPanel, pchEventCreate, pchEventEnd ); } \
	}; \
	template< class T, class U > void RegisterEventHandler( const name &t, T * pPanel, bool (U::*memberfunc)( const panorama::CPanelPtr< panorama::IUIPanel > & ) ) { panorama::UIEngine()->RegisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pPanel, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void UnregisterEventHandler( const name &t, T *pPanel, bool (U::*memberfunc)( const panorama::CPanelPtr< panorama::IUIPanel > & ) ) { panorama::UIEngine()->UnregisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pPanel, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void RegisterEventHandlerOnPanel( const name &t, panorama::IUIPanel *pPanel, T * pHandler, bool (U::*memberfunc)( const panorama::CPanelPtr< panorama::IUIPanel > & ) ) { panorama::UIEngine()->RegisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pHandler, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void UnregisterEventHandlerOnPanel( const name &t, panorama::IUIPanel *pPanel, T *pHandler, bool (U::*memberfunc)( const panorama::CPanelPtr< panorama::IUIPanel > & ) ) { panorama::UIEngine()->UnregisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pHandler, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void RegisterForUnhandledEvent( const name &t, T *pObj, bool (U::*memberfunc)( const panorama::CPanelPtr< panorama::IUIPanel > & ) ) { panorama::UIEngine()->RegisterForUnhandledEvent( t.GetEventType(), UtlMakeDelegate( pObj, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void UnregisterForUnhandledEvent( const name &t, T *pObj, bool (U::*memberfunc)( const panorama::CPanelPtr< panorama::IUIPanel > & ) ) { panorama::UIEngine()->UnregisterForUnhandledEvent( t.GetEventType(), UtlMakeDelegate( pObj, memberfunc ).GetAbstractDelegate() ); } \
	template< class PanelType > void RegisterEventHandlerOnPanelType( const name &t, bool( PanelType::*memberfunc )( const panorama::CPanelPtr< panorama::IUIPanel > & ) ) { panorama::UIEngine()->RegisterPanelTypeEventHandler( t.GetEventType(), PanelType::GetPanelSymbol(), UtlMakeDelegate( (PanelType*)NULL, memberfunc ).GetAbstractDelegate() ); }

//
// Events with 1 params
//
namespace UIEvent
{
	template < class T, typename param1 >
	IUIEvent *CreateEventFromString( panorama::IUIPanel *pPanel, const char *pchEvent, const char **pchEventEnd  )
	{
		param1 p1;
		if ( !ParseUIEventParam< param1 >( &p1, pPanel, pchEvent, &pchEvent ) )
			return NULL;

		if ( !IsEndOfUIEventString( pchEvent, &pchEvent ) )
			return NULL;

		*pchEventEnd  = pchEvent;

		IUIEvent *pEvent = T::MakeEvent( pPanel ? pPanel->ClientPtr() : NULL, p1 );
		UIEventFree( p1 );
		return pEvent;
	}
}

#define DECLARE_PANORAMA_EVENT1( name, param1 ) \
	class  name \
	{ \
	public: \
		typedef param1 TypenameParam1; static const int cParams = 1; static const bool bPanelEvent = false; static panorama::CPanoramaSymbol symbol; static const char *pchEvent;\
		static panorama::CPanoramaSymbol GetEventType() { Assert( symbol.IsValid() ); return symbol; } \
		static panorama::IUIEvent *MakeEvent( const panorama::IUIPanelClient *pTarget, param1 p1 ) { return new panorama::CUIEvent1< param1 >( symbol, pTarget ? pTarget->UIPanel() : NULL, p1 ); } \
		static panorama::IUIEvent *CreateEventFromString( panorama::IUIPanel *pPanel, const char *pchEventCreate, const char **pchEventEnd  ) { return panorama::UIEvent::CreateEventFromString< name, param1 >( pPanel, pchEventCreate, pchEventEnd ); } \
	}; \
	template< class T, class U > void RegisterEventHandler( const name &t, T *pPanel, bool (U::*memberfunc)( param1 ) ) { panorama::UIEngine()->RegisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pPanel, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void UnregisterEventHandler( const name &t, T *pPanel, bool (U::*memberfunc)( param1 ) ) { panorama::UIEngine()->UnregisterEventHandler( t.GetEventType(), pPanel->UIPanel(), UtlMakeDelegate( pPanel, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void RegisterEventHandlerOnPanel( const name &t, panorama::IUIPanel *pPanel, T *pHandler, bool (U::*memberfunc)( param1 ) ) { panorama::UIEngine()->RegisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pHandler, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void UnregisterEventHandlerOnPanel( const name &t, panorama::IUIPanel *pPanel, T *pHandler, bool (U::*memberfunc)( param1 ) ) { panorama::UIEngine()->UnregisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pHandler, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void RegisterForUnhandledEvent( const name &t, T *pObj, bool (U::*memberfunc)( param1 ) ) { panorama::UIEngine()->RegisterForUnhandledEvent( t.GetEventType(), UtlMakeDelegate( pObj, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void UnregisterForUnhandledEvent( const name &t, T *pObj, bool (U::*memberfunc)( param1 ) ) { panorama::UIEngine()->UnregisterForUnhandledEvent( t.GetEventType(), UtlMakeDelegate( pObj, memberfunc ).GetAbstractDelegate() ); } \
	template< class PanelType > void RegisterEventHandlerOnPanelType( const name &t, bool( PanelType::*memberfunc )( param1 ) ) { panorama::UIEngine()->RegisterPanelTypeEventHandler( t.GetEventType(), PanelType::GetPanelSymbol(), UtlMakeDelegate( (PanelType*)NULL, memberfunc ).GetAbstractDelegate() ); }


#define DECLARE_PANEL_EVENT1( name, param1 ) \
class  name \
	{ \
	public: \
		typedef param1 TypenameParam1; static const int cParams = 1; static const bool bPanelEvent = true; static panorama::CPanoramaSymbol symbol; static const char *pchEvent;\
		static panorama::CPanoramaSymbol GetEventType() { Assert( symbol.IsValid() ); return symbol; } \
		static panorama::IUIEvent *MakeEvent( const panorama::IUIPanelClient *pTarget, param1 p1 ) { return new panorama::CUIPanelEvent1< param1 >( symbol, pTarget ? pTarget->UIPanel() : NULL, p1 ); } \
		static panorama::IUIEvent *CreateEventFromString( panorama::IUIPanel *pPanel, const char *pchEventCreate, const char **pchEventEnd ) { return panorama::UIEvent::CreateEventFromString< name, param1 >( pPanel, pchEventCreate, pchEventEnd ); } \
	}; \
	template< class T, class U > void RegisterEventHandler( const name &t, T *pPanel, bool (U::*memberfunc)( const panorama::CPanelPtr< panorama::IUIPanel > &, param1 ) ) { panorama::UIEngine()->RegisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pPanel, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void UnregisterEventHandler( const name &t, T *pPanel, bool (U::*memberfunc)( const panorama::CPanelPtr< panorama::IUIPanel > &, param1 ) ) { panorama::UIEngine()->UnregisterEventHandler( t.GetEventType(), pPanel->UIPanel(), UtlMakeDelegate( pPanel, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void RegisterEventHandlerOnPanel( const name &t, panorama::IUIPanel *pPanel, T *pHandler, bool (U::*memberfunc)( const panorama::CPanelPtr< panorama::IUIPanel > &, param1 ) ) { panorama::UIEngine()->RegisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pHandler, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void UnregisterEventHandlerOnPanel( const name &t, panorama::IUIPanel *pPanel, T *pHandler, bool (U::*memberfunc)( const panorama::CPanelPtr< panorama::IUIPanel > &, param1 ) ) { panorama::UIEngine()->UnregisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pHandler, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void RegisterForUnhandledEvent( const name &t, T *pObj, bool (U::*memberfunc)( const panorama::CPanelPtr< panorama::IUIPanel > &, param1 ) ) { panorama::UIEngine()->RegisterForUnhandledEvent( t.GetEventType(), UtlMakeDelegate( pObj, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void UnregisterForUnhandledEvent( const name &t, T *pObj, bool (U::*memberfunc)( const panorama::CPanelPtr< panorama::IUIPanel > &, param1 ) ) { panorama::UIEngine()->UnregisterForUnhandledEvent( t.GetEventType(), UtlMakeDelegate( pObj, memberfunc ).GetAbstractDelegate() ); } \
	template< class PanelType > void RegisterEventHandlerOnPanelType( const name &t, bool( PanelType::*memberfunc )( const panorama::CPanelPtr< panorama::IUIPanel > &, param1 ) ) { panorama::UIEngine()->RegisterPanelTypeEventHandler( t.GetEventType(), PanelType::GetPanelSymbol(), UtlMakeDelegate( (PanelType*)NULL, memberfunc ).GetAbstractDelegate() ); }


//
// Events with 2 params
//
namespace UIEvent
{
	template < class T, typename param1, typename param2 >
	IUIEvent *CreateEventFromString( panorama::IUIPanel *pPanel, const char *pchEvent, const char **pchEventEnd )
	{
		param1 p1;
		if ( !ParseUIEventParam< param1 >( &p1, pPanel, pchEvent, &pchEvent ) )
			return NULL;

		param2 p2;
		if ( !ParseUIEventParam< param2 >( &p2, pPanel, pchEvent, &pchEvent ) )
			return NULL;

		if ( !IsEndOfUIEventString( pchEvent, &pchEvent ) )
			return NULL;

		*pchEventEnd = pchEvent;
		IUIEvent *pEvent = T::MakeEvent( pPanel ? pPanel->ClientPtr() : NULL, p1, p2 );
		UIEventFree( p1 );
		UIEventFree( p2 );
		return pEvent;
	}
}

#define DECLARE_PANORAMA_EVENT2( name, param1, param2 ) \
	class  name \
	{ \
	public: \
		typedef param1 TypenameParam1; typedef param2 TypenameParam2; static const int cParams = 2; static const bool bPanelEvent = false; static panorama::CPanoramaSymbol symbol; static const char *pchEvent;\
		static panorama::CPanoramaSymbol GetEventType() { Assert( symbol.IsValid() ); return symbol; } \
		static panorama::IUIEvent *MakeEvent( const panorama::IUIPanelClient *pTarget, param1 p1, param2 p2 ) { return new panorama::CUIEvent2< param1, param2 >( symbol, pTarget ? pTarget->UIPanel() : NULL, p1, p2 ); } \
		static panorama::IUIEvent *CreateEventFromString( panorama::IUIPanel *pPanel, const char *pchEventCreate, const char **pchEventEnd ) { return panorama::UIEvent::CreateEventFromString< name, param1, param2 >( pPanel, pchEventCreate, pchEventEnd ); } \
	}; \
	template< class T, class U > void RegisterEventHandler( const name &t, T *pPanel, bool (U::*memberfunc)( param1, param2 ) ) { panorama::UIEngine()->RegisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pPanel, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void UnregisterEventHandler( const name &t, T *pPanel, bool (U::*memberfunc)( param1, param2 ) ) { panorama::UIEngine()->UnregisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pPanel, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void RegisterEventHandlerOnPanel( const name &t, panorama::IUIPanel *pPanel, T *pHandler, bool (U::*memberfunc)( param1, param2 ) ) { panorama::UIEngine()->RegisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pHandler, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void UnregisterEventHandlerOnPanel( const name &t, panorama::IUIPanel *pPanel, T *pHandler, bool (U::*memberfunc)( param1, param2 ) ) { panorama::UIEngine()->UnregisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pHandler, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void RegisterForUnhandledEvent( const name &t, T *pObj, bool (U::*memberfunc)( param1, param2 ) ) { panorama::UIEngine()->RegisterForUnhandledEvent( t.GetEventType(), UtlMakeDelegate( pObj, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void UnregisterForUnhandledEvent( const name &t, T *pObj, bool (U::*memberfunc)( param1, param2 ) ) { panorama::UIEngine()->UnregisterForUnhandledEvent( t.GetEventType(), UtlMakeDelegate( pObj, memberfunc ).GetAbstractDelegate() ); } \
	template< class PanelType > void RegisterEventHandlerOnPanelType( const name &t, bool( PanelType::*memberfunc )( param1, param2 ) ) { panorama::UIEngine()->RegisterPanelTypeEventHandler( t.GetEventType(), PanelType::GetPanelSymbol(), UtlMakeDelegate( (PanelType*)NULL, memberfunc ).GetAbstractDelegate() ); }


#define DECLARE_PANEL_EVENT2( name, param1, param2 ) \
	class  name \
	{ \
	public: \
		typedef param1 TypenameParam1; typedef param2 TypenameParam2; static const int cParams = 2; static const bool bPanelEvent = true; static panorama::CPanoramaSymbol symbol; static const char *pchEvent;\
		static panorama::CPanoramaSymbol GetEventType() { Assert( symbol.IsValid() ); return symbol; } \
		static panorama::IUIEvent *MakeEvent( const panorama::IUIPanelClient *pTarget, param1 p1, param2 p2 ) { return new panorama::CUIPanelEvent2< param1, param2 >( symbol, pTarget ? pTarget->UIPanel() : NULL, p1, p2 ); } \
		static panorama::IUIEvent *CreateEventFromString( panorama::IUIPanel *pPanel, const char *pchEventCreate, const char **pchEventEnd ) { return panorama::UIEvent::CreateEventFromString< name, param1, param2 >( pPanel, pchEventCreate, pchEventEnd ); } \
	}; \
	template< class T, class U > void RegisterEventHandler( const name &t, T *pPanel, bool (U::*memberfunc)( const panorama::CPanelPtr< panorama::IUIPanel > &, param1, param2 ) ) { panorama::UIEngine()->RegisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pPanel, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void UnregisterEventHandler( const name &t, T *pPanel, bool (U::*memberfunc)( const panorama::CPanelPtr< panorama::IUIPanel > &, param1, param2 ) ) { panorama::UIEngine()->UnregisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pPanel, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void RegisterEventHandlerOnPanel( const name &t, panorama::IUIPanel *pPanel, T *pHandler, bool (U::*memberfunc)( const panorama::CPanelPtr< panorama::IUIPanel > &, param1, param2 ) ) { panorama::UIEngine()->RegisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pHandler, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void UnregisterEventHandlerOnPanel( const name &t, panorama::IUIPanel *pPanel, T *pHandler, bool (U::*memberfunc)( const panorama::CPanelPtr< panorama::IUIPanel > &, param1, param2 ) ) { panorama::UIEngine()->UnregisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pHandler, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void RegisterForUnhandledEvent( const name &t, T *pObj, bool (U::*memberfunc)( const panorama::CPanelPtr< panorama::IUIPanel > &, param1, param2 ) ) { panorama::UIEngine()->RegisterForUnhandledEvent( t.GetEventType(), UtlMakeDelegate( pObj, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void UnregisterForUnhandledEvent( const name &t, T *pObj, bool (U::*memberfunc)( const panorama::CPanelPtr< panorama::IUIPanel > &, param1, param2 ) ) { panorama::UIEngine()->UnregisterForUnhandledEvent( t.GetEventType(), UtlMakeDelegate( pObj, memberfunc ).GetAbstractDelegate() ); } \
	template< class PanelType > void RegisterEventHandlerOnPanelType( const name &t, bool( PanelType::*memberfunc )( const panorama::CPanelPtr< panorama::IUIPanel > &, param1, param2 ) ) { panorama::UIEngine()->RegisterPanelTypeEventHandler( t.GetEventType(), PanelType::GetPanelSymbol(), UtlMakeDelegate( (PanelType*)NULL, memberfunc ).GetAbstractDelegate() ); }

//
// Events with 3 params
//
namespace UIEvent
{
	template < class T, typename param1, typename param2, typename param3 >
	IUIEvent *CreateEventFromString( panorama::IUIPanel *pPanel, const char *pchEvent, const char **pchEventEnd )
	{
		param1 p1;
		if ( !ParseUIEventParam< param1 >( &p1, pPanel, pchEvent, &pchEvent ) )
			return NULL;

		param2 p2;
		if ( !ParseUIEventParam< param2 >( &p2, pPanel, pchEvent, &pchEvent ) )
			return NULL;

		param3 p3;
		if ( !ParseUIEventParam< param3 >( &p3, pPanel, pchEvent, &pchEvent ) )
			return NULL;

		if ( !IsEndOfUIEventString( pchEvent, &pchEvent ) )
			return NULL;

		*pchEventEnd = pchEvent;

		IUIEvent *pEvent = T::MakeEvent( pPanel ? pPanel->ClientPtr() : NULL, p1, p2, p3 );
		UIEventFree( p1 );
		UIEventFree( p2 );
		UIEventFree( p3 );
		return pEvent;
	}
}

#define DECLARE_PANORAMA_EVENT3( name, param1, param2, param3 ) \
class  name \
	{ \
	public: \
		typedef param1 TypenameParam1; typedef param2 TypenameParam2; typedef param3 TypenameParam3; static const int cParams = 3; static const bool bPanelEvent = false; static panorama::CPanoramaSymbol symbol; static const char *pchEvent; \
		static panorama::CPanoramaSymbol GetEventType() { Assert( symbol.IsValid() ); return symbol; } \
		static panorama::IUIEvent *MakeEvent( const panorama::IUIPanelClient *pTarget, param1 p1, param2 p2, param3 p3 ) { return new panorama::CUIEvent3< param1, param2, param3 >( symbol, pTarget ? pTarget->UIPanel() : NULL, p1, p2, p3 ); } \
		static panorama::IUIEvent *CreateEventFromString( panorama::IUIPanel *pPanel, const char *pchEventCreate, const char **pchEventEnd ) { return panorama::UIEvent::CreateEventFromString< name, param1, param2, param3 >( pPanel, pchEventCreate, pchEventEnd ); } \
	}; \
	template< class T, class U > void RegisterEventHandler( const name &t, T *pPanel, bool (U::*memberfunc)( param1, param2, param3 ) ) { panorama::UIEngine()->RegisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pPanel, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void UnregisterEventHandler( const name &t, T *pPanel, bool (U::*memberfunc)( param1, param2, param3 ) ) { panorama::UIEngine()->UnregisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pPanel, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void RegisterEventHandlerOnPanel( const name &t, panorama::IUIPanel *pPanel, T *pHandler, bool (U::*memberfunc)( param1, param2, param3 ) ) { panorama::UIEngine()->RegisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pHandler, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void UnregisterEventHandlerOnPanel( const name &t, panorama::IUIPanel *pPanel, T *pHandler, bool (U::*memberfunc)( param1, param2, param3 ) ) { panorama::UIEngine()->UnregisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pHandler, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void RegisterForUnhandledEvent( const name &t, T *pObj, bool (U::*memberfunc)( param1, param2, param3 ) ) { panorama::UIEngine()->RegisterForUnhandledEvent( t.GetEventType(), UtlMakeDelegate( pObj, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void UnregisterForUnhandledEvent( const name &t, T *pObj, bool (U::*memberfunc)( param1, param2, param3 ) ) { panorama::UIEngine()->UnregisterForUnhandledEvent( t.GetEventType(), UtlMakeDelegate( pObj, memberfunc ).GetAbstractDelegate() ); } \
	template< class PanelType > void RegisterEventHandlerOnPanelType( const name &t, bool( PanelType::*memberfunc )( param1, param2, param3 ) ) { panorama::UIEngine()->RegisterPanelTypeEventHandler( t.GetEventType(), PanelType::GetPanelSymbol(), UtlMakeDelegate( (PanelType*)NULL, memberfunc ).GetAbstractDelegate() ); }


#define DECLARE_PANEL_EVENT3( name, param1, param2, param3 ) \
	class  name \
	{ \
	public: \
		typedef param1 TypenameParam1; typedef param2 TypenameParam2; typedef param3 TypenameParam3; static const int cParams = 3; static const bool bPanelEvent = true; static panorama::CPanoramaSymbol symbol; static const char *pchEvent;\
		static panorama::CPanoramaSymbol GetEventType() { Assert( symbol.IsValid() ); return symbol; } \
		static panorama::IUIEvent *MakeEvent( const panorama::IUIPanelClient *pTarget, param1 p1, param2 p2, param3 p3  ) { return new panorama::CUIPanelEvent3< param1, param2, param3 >( symbol, pTarget ? pTarget->UIPanel() : NULL, p1, p2, p3 ); } \
		static panorama::IUIEvent *CreateEventFromString( panorama::IUIPanel *pPanel, const char *pchEventCreate, const char **pchEventEnd ) { return panorama::UIEvent::CreateEventFromString< name, param1, param2, param3 >( pPanel, pchEventCreate, pchEventEnd ); } \
	}; \
	template< class T, class U > void RegisterEventHandler( const name &t, T *pPanel, bool (U::*memberfunc)( const panorama::CPanelPtr< panorama::IUIPanel > &, param1, param2, param3 ) ) { panorama::UIEngine()->RegisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pPanel, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void UnregisterEventHandler( const name &t, T *pPanel, bool (U::*memberfunc)( const panorama::CPanelPtr< panorama::IUIPanel > &, param1, param2, param3 ) ) { panorama::UIEngine()->UnregisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pPanel, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void RegisterEventHandlerOnPanel( const name &t, panorama::IUIPanel *pPanel, T *pHandler, bool (U::*memberfunc)( const panorama::CPanelPtr< panorama::IUIPanel > &, param1, param2, param3 ) ) { panorama::UIEngine()->RegisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pHandler, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void UnregisterEventHandlerOnPanel( const name &t, panorama::IUIPanel *pPanel, T *pHandler, bool (U::*memberfunc)( const panorama::CPanelPtr< panorama::IUIPanel > &, param1, param2, param3 ) ) { panorama::UIEngine()->UnregisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pHandler, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void RegisterForUnhandledEvent( const name &t, T *pObj, bool (U::*memberfunc)( const panorama::CPanelPtr< panorama::IUIPanel > &, param1, param2, param3 ) ) { panorama::UIEngine()->RegisterForUnhandledEvent( t.GetEventType(), UtlMakeDelegate( pObj, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void UnregisterForUnhandledEvent( const name &t, T *pObj, bool (U::*memberfunc)( const panorama::CPanelPtr< panorama::IUIPanel > &, param1, param2, param3 ) ) { panorama::UIEngine()->UnregisterForUnhandledEvent( t.GetEventType(), UtlMakeDelegate( pObj, memberfunc ).GetAbstractDelegate() ); } \
	template< class PanelType > void RegisterEventHandlerOnPanelType( const name &t, bool( PanelType::*memberfunc )( const panorama::CPanelPtr< panorama::IUIPanel > &, param1, param2, param3 ) ) { panorama::UIEngine()->RegisterPanelTypeEventHandler( t.GetEventType(), PanelType::GetPanelSymbol(), UtlMakeDelegate( (PanelType*)NULL, memberfunc ).GetAbstractDelegate() ); }


//
// Events with 4 params
//
namespace UIEvent
{
	template < class T, typename param1, typename param2, typename param3, typename param4 >
	IUIEvent *CreateEventFromString( panorama::IUIPanel *pPanel, const char *pchEvent, const char **pchEventEnd )
	{
		param1 p1;
		if ( !ParseUIEventParam< param1 >( &p1, pPanel, pchEvent, &pchEvent ) )
			return NULL;

		param2 p2;
		if ( !ParseUIEventParam< param2 >( &p2, pPanel, pchEvent, &pchEvent ) )
			return NULL;

		param3 p3;
		if ( !ParseUIEventParam< param3 >( &p3, pPanel, pchEvent, &pchEvent ) )
			return NULL;

		param4 p4;
		if ( !ParseUIEventParam< param4 >( &p4, pPanel, pchEvent, &pchEvent ) )
			return NULL;

		if ( !IsEndOfUIEventString( pchEvent, &pchEvent ) )
			return NULL;

		*pchEventEnd = pchEvent;
		IUIEvent *pEvent = T::MakeEvent( pPanel ? pPanel->ClientPtr() : NULL, p1, p2, p3, p4 );
		UIEventFree( p1 );
		UIEventFree( p2 );
		UIEventFree( p3 );
		UIEventFree( p4 );
		return pEvent;
	}
}

#define DECLARE_PANORAMA_EVENT4( name, param1, param2, param3, param4 ) \
	class  name \
	{ \
	public: \
		typedef param1 TypenameParam1; typedef param2 TypenameParam2; typedef param3 TypenameParam3; typedef param4 TypenameParam4; static const int cParams = 4; static const bool bPanelEvent = false; static panorama::CPanoramaSymbol symbol; static const char *pchEvent;\
		static panorama::CPanoramaSymbol GetEventType() { Assert( symbol.IsValid() ); return symbol; } \
		static panorama::IUIEvent *MakeEvent( const panorama::IUIPanelClient *pTarget, param1 p1, param2 p2, param3 p3, param4 p4 ) { return new panorama::CUIEvent4< param1, param2, param3, param4 >( symbol, pTarget ? pTarget->UIPanel() : NULL, p1, p2, p3, p4 ); } \
		static panorama::IUIEvent *CreateEventFromString( panorama::IUIPanel *pPanel, const char *pchEventCreate, const char **pchEventEnd ) { return panorama::UIEvent::CreateEventFromString< name, param1, param2, param3, param4 >( pPanel, pchEventCreate, pchEventEnd ); } \
	}; \
	template< class T, class U > void RegisterEventHandler( const name &t, T *pPanel, bool (U::*memberfunc)( param1, param2, param3, param4 ) ) { panorama::UIEngine()->RegisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pPanel, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void UnregisterEventHandler( const name &t, T *pPanel, bool (U::*memberfunc)( param1, param2, param3, param4 ) ) { panorama::UIEngine()->UnregisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pPanel, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void RegisterEventHandlerOnPanel( const name &t, panorama::IUIPanel *pPanel, T *pHandler, bool (U::*memberfunc)( param1, param2, param3, param4 ) ) { panorama::UIEngine()->RegisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pHandler, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void UnregisterEventHandlerOnPanel( const name &t, panorama::IUIPanel *pPanel, T *pHandler, bool (U::*memberfunc)( param1, param2, param3, param4 ) ) { panorama::UIEngine()->UnregisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pHandler, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void RegisterForUnhandledEvent( const name &t, T *pObj, bool (U::*memberfunc)( param1, param2, param3, param4 ) ) { panorama::UIEngine()->RegisterForUnhandledEvent( t.GetEventType(), UtlMakeDelegate( pObj, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void UnregisterForUnhandledEvent( const name &t, T *pObj, bool (U::*memberfunc)( param1, param2, param3, param4 ) ) { panorama::UIEngine()->UnregisterForUnhandledEvent( t.GetEventType(), UtlMakeDelegate( pObj, memberfunc ).GetAbstractDelegate() ); } \
	template< class PanelType > void RegisterEventHandlerOnPanelType( const name &t, bool( PanelType::*memberfunc )( param1, param2, param3, param4 ) ) { panorama::UIEngine()->RegisterPanelTypeEventHandler( t.GetEventType(), PanelType::GetPanelSymbol(), UtlMakeDelegate( (PanelType*)NULL, memberfunc ).GetAbstractDelegate() ); }


#define DECLARE_PANEL_EVENT4( name, param1, param2, param3, param4 ) \
	class  name \
	{ \
	public: \
		typedef param1 TypenameParam1; typedef param2 TypenameParam2; typedef param3 TypenameParam3; typedef param4 TypenameParam4; static const int cParams = 4; static const bool bPanelEvent = true; static panorama::CPanoramaSymbol symbol; static const char *pchEvent;\
		static panorama::CPanoramaSymbol GetEventType() { Assert( symbol.IsValid() ); return symbol; } \
		static panorama::IUIEvent *MakeEvent( const panorama::IUIPanelClient *pTarget, param1 p1, param2 p2, param3 p3, param4 p4 ) { return new panorama::CUIPanelEvent4< param1, param2, param3, param4 >( symbol, pTarget ? pTarget->UIPanel() : NULL, p1, p2, p3, p4 ); } \
		static panorama::IUIEvent *CreateEventFromString( panorama::IUIPanel *pPanel, const char *pchEventCreate, const char **pchEventEnd ) { return panorama::UIEvent::CreateEventFromString< name, param1, param2, param3, param4 >( pPanel, pchEventCreate, pchEventEnd ); } \
	}; \
	template< class T, class U > void RegisterEventHandler( const name &t, T *pPanel, bool (U::*memberfunc)( const panorama::CPanelPtr< panorama::IUIPanel > &, param1, param2, param3, param4 ) ) { panorama::UIEngine()->RegisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pPanel, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void UnregisterEventHandler( const name &t, T *pPanel, bool (U::*memberfunc)( const panorama::CPanelPtr< panorama::IUIPanel > &, param1, param2, param3, param4 ) ) { panorama::UIEngine()->UnregisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pPanel, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void RegisterEventHandlerOnPanel( const name &t, panorama::IUIPanel *pPanel, T *pHandler, bool (U::*memberfunc)( const panorama::CPanelPtr< panorama::IUIPanel > &, param1, param2, param3, param4 ) ) { panorama::UIEngine()->RegisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pHandler, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void UnregisterEventHandlerOnPanel( const name &t, panorama::IUIPanel *pPanel, T *pHandler, bool (U::*memberfunc)( const panorama::CPanelPtr< panorama::IUIPanel > &, param1, param2, param3, param4 ) ) { panorama::UIEngine()->UnregisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pHandler, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void RegisterForUnhandledEvent( const name &t, T *pObj, bool (U::*memberfunc)( const panorama::CPanelPtr< panorama::IUIPanel > &, param1, param2, param3, param4 ) ) { panorama::UIEngine()->RegisterForUnhandledEvent( t.GetEventType(), UtlMakeDelegate( pObj, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void UnregisterForUnhandledEvent( const name &t, T *pObj, bool (U::*memberfunc)( const panorama::CPanelPtr< panorama::IUIPanel > &, param1, param2, param3, param4 ) ) { panorama::UIEngine()->UnregisterForUnhandledEvent( t.GetEventType(), UtlMakeDelegate( pObj, memberfunc ).GetAbstractDelegate() ); } \
	template< class PanelType > void RegisterEventHandlerOnPanelType( const name &t, bool( PanelType::*memberfunc )( const panorama::CPanelPtr< panorama::IUIPanel > &, param1, param2, param3, param4 ) ) { panorama::UIEngine()->RegisterPanelTypeEventHandler( t.GetEventType(), PanelType::GetPanelSymbol(), UtlMakeDelegate( (PanelType*)NULL, memberfunc ).GetAbstractDelegate() ); }


//
// Events with 5 params
//
namespace UIEvent
{
	template < class T, typename param1, typename param2, typename param3, typename param4, typename param5 >
	IUIEvent *CreateEventFromString( panorama::IUIPanel *pPanel, const char *pchEvent, const char **pchEventEnd )
	{
		param1 p1;
		if ( !ParseUIEventParam< param1 >( &p1, pPanel, pchEvent, &pchEvent ) )
			return NULL;

		param2 p2;
		if ( !ParseUIEventParam< param2 >( &p2, pPanel, pchEvent, &pchEvent ) )
			return NULL;

		param3 p3;
		if ( !ParseUIEventParam< param3 >( &p3, pPanel, pchEvent, &pchEvent ) )
			return NULL;

		param4 p4;
		if ( !ParseUIEventParam< param4 >( &p4, pPanel, pchEvent, &pchEvent ) )
			return NULL;

		param5 p5;
		if ( !ParseUIEventParam< param5 >( &p5, pPanel, pchEvent, &pchEvent ) )
			return NULL;

		if ( !IsEndOfUIEventString( pchEvent, &pchEvent ) )
			return NULL;

		*pchEventEnd = pchEvent;

		IUIEvent *pEvent = T::MakeEvent( pPanel ? pPanel->ClientPtr() : NULL, p1, p2, p3, p4, p5 );
		UIEventFree( p1 );
		UIEventFree( p2 );
		UIEventFree( p3 );
		UIEventFree( p4 );
		UIEventFree( p5 );
		return pEvent;
	}
}

#define DECLARE_PANORAMA_EVENT5( name, param1, param2, param3, param4, param5 ) \
class  name \
{ \
public: \
	typedef param1 TypenameParam1; typedef param2 TypenameParam2; typedef param3 TypenameParam3; typedef param4 TypenameParam4; typedef param5 TypenameParam5; static const int cParams = 5; static const bool bPanelEvent = false; static panorama::CPanoramaSymbol symbol; static const char *pchEvent;\
	static panorama::CPanoramaSymbol GetEventType() { Assert( symbol.IsValid() ); return symbol; } \
	static panorama::IUIEvent *MakeEvent( const panorama::IUIPanelClient *pTarget, param1 p1, param2 p2, param3 p3, param4 p4, param5 p5 ) { return new panorama::CUIEvent5< param1, param2, param3, param4, param5 >( symbol, pTarget ? pTarget->UIPanel() : NULL, p1, p2, p3, p4, p5 ); } \
	static panorama::IUIEvent *CreateEventFromString( panorama::IUIPanel *pPanel, const char *pchEventCreate, const char **pchEventEnd ) { return panorama::UIEvent::CreateEventFromString< name, param1, param2, param3, param4, param5 >( pPanel, pchEventCreate, pchEventEnd ); } \
}; \
	template< class T, class U > void RegisterEventHandler( const name &t, T *pPanel, bool (U::*memberfunc)( param1, param2, param3, param4, param5 ) ) { panorama::UIEngine()->RegisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pPanel, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void UnregisterEventHandler( const name &t, T *pPanel, bool (U::*memberfunc)( param1, param2, param3, param4, param5 ) ) { panorama::UIEngine()->UnregisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pPanel, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void RegisterEventHandlerOnPanel( const name &t, panorama::IUIPanel *pPanel, T *pHandler, bool (U::*memberfunc)( param1, param2, param3, param4, param5 ) ) { panorama::UIEngine()->RegisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pHandler, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void UnregisterEventHandlerOnPanel( const name &t, panorama::IUIPanel *pPanel, T *pHandler, bool (U::*memberfunc)( param1, param2, param3, param4, param5 ) ) { panorama::UIEngine()->UnregisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pHandler, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void RegisterForUnhandledEvent( const name &t, T *pObj, bool (U::*memberfunc)( param1, param2, param3, param4, param5 ) ) { panorama::UIEngine()->RegisterForUnhandledEvent( t.GetEventType(), UtlMakeDelegate( pObj, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void UnregisterForUnhandledEvent( const name &t, T *pObj, bool (U::*memberfunc)( param1, param2, param3, param4, param5 ) ) { panorama::UIEngine()->UnregisterForUnhandledEvent( t.GetEventType(), UtlMakeDelegate( pObj, memberfunc ).GetAbstractDelegate() ); } \
	template< class PanelType > void RegisterEventHandlerOnPanelType( const name &t, bool( PanelType::*memberfunc )( param1, param2, param3, param4, param5 ) ) { panorama::UIEngine()->RegisterPanelTypeEventHandler( t.GetEventType(), PanelType::GetPanelSymbol(), UtlMakeDelegate( (PanelType*)NULL, memberfunc ).GetAbstractDelegate() ); }


#define DECLARE_PANEL_EVENT5( name, param1, param2, param3, param4, param5 ) \
class  name \
{ \
public: \
	typedef param1 TypenameParam1; typedef param2 TypenameParam2; typedef param3 TypenameParam3; typedef param4 TypenameParam4; typedef param5 TypenameParam5; static const int cParams = 5; static const bool bPanelEvent = true; static panorama::CPanoramaSymbol symbol; static const char *pchEvent;\
	static panorama::CPanoramaSymbol GetEventType() { return symbol; } \
	static panorama::IUIEvent *MakeEvent( const panorama::IUIPanelClient *pTarget, param1 p1, param2 p2, param3 p3, param4 p4, param5 p5 ) { return new panorama::CUIPanelEvent5< param1, param2, param3, param4, param5 >( symbol, pTarget ? pTarget->UIPanel() : NULL, p1, p2, p3, p4, p5 ); } \
	static panorama::IUIEvent *CreateEventFromString( panorama::IUIPanel *pPanel, const char *pchEventCreate, const char **pchEventEnd ) { return panorama::UIEvent::CreateEventFromString< name, param1, param2, param3, param4, param5 >( pPanel, pchEventCreate, pchEventEnd ); } \
}; \
	template< class T, class U > void RegisterEventHandler( const name &t, T *pPanel, bool (U::*memberfunc)( const panorama::CPanelPtr< panorama::IUIPanel > &, param1, param2, param3, param4, param5 ) ) { panorama::UIEngine()->RegisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pPanel, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void UnregisterEventHandler( const name &t, T *pPanel, bool (U::*memberfunc)( const panorama::CPanelPtr< panorama::IUIPanel > &, param1, param2, param3, param4, param5 ) ) { panorama::UIEngine()->UnregisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pPanel, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void RegisterEventHandlerOnPanel( const name &t, panorama::IUIPanel *pPanel, T *pHandler, bool (U::*memberfunc)( const panorama::CPanelPtr< panorama::IUIPanel > &, param1, param2, param3, param4, param5 ) ) { panorama::UIEngine()->RegisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pHandler, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void UnregisterEventHandlerOnPanel( const name &t, panorama::IUIPanel *pPanel, T *pHandler, bool (U::*memberfunc)( const panorama::CPanelPtr< panorama::IUIPanel > &, param1, param2, param3, param4, param5 ) ) { panorama::UIEngine()->UnregisterEventHandler( t.GetEventType(), pPanel, UtlMakeDelegate( pHandler, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void RegisterForUnhandledEvent( const name &t, T *pObj, bool (U::*memberfunc)( const panorama::CPanelPtr< panorama::IUIPanel > &, param1, param2, param3, param4, param5 ) ) { panorama::UIEngine()->RegisterForUnhandledEvent( t.GetEventType(), UtlMakeDelegate( pObj, memberfunc ).GetAbstractDelegate() ); } \
	template< class T, class U > void UnregisterForUnhandledEvent( const name &t, T *pObj, bool (U::*memberfunc)( const panorama::CPanelPtr< panorama::IUIPanel > &, param1, param2, param3, param4, param5 ) ) { panorama::UIEngine()->UnregisterForUnhandledEvent( t.GetEventType(), UtlMakeDelegate( pObj, memberfunc ).GetAbstractDelegate() ); } \
	template< class PanelType > void RegisterEventHandlerOnPanelType( const name &t, bool( PanelType::*memberfunc )( const panorama::CPanelPtr< panorama::IUIPanel > &, param1, param2, param3, param4, param5 ) ) { panorama::UIEngine()->RegisterPanelTypeEventHandler( t.GetEventType(), PanelType::GetPanelSymbol(), UtlMakeDelegate( (PanelType*)NULL, memberfunc ).GetAbstractDelegate() ); }


#define DEFINE_PANORAMA_EVENT( name ) \
	panorama::CPanoramaSymbol name::symbol; \
	const char *name::pchEvent = #name; \
	panorama::CAutoRegisterUIEvent< name, name::cParams > g_##name##_EventAutoRegister( #name );


//-----------------------------------------------------------------------------
// Purpose: Class to automatically register events at startup
//-----------------------------------------------------------------------------
void RegisterUIEvent( panorama::CPanoramaSymbol *pSymEvent, const char *pchEventType, int cParams, bool bPanelEvent, 
	PFN_ParseUIEvent pfnParseUIEvent, PFN_MakeUIEvent0 pfnMakeUIEvent0, PFN_MakeUIEvent1Repeats pfnMakeUIEvent1Repeats, PFN_MakeUIEvent1Source pfnMakeUIEvent1Source );


template < class T, int N >
class CAutoRegisterUIEvent
{
public:
	CAutoRegisterUIEvent( const char *pch )
	{
		RegisterUIEvent( &T::symbol, T::pchEvent, T::cParams, T::bPanelEvent, T::CreateEventFromString, NULL, NULL, NULL );
	}
};

template < class T >
class CAutoRegisterUIEvent<T, 0>
{
public:
	CAutoRegisterUIEvent( const char *pch )
	{
		RegisterUIEvent( &T::symbol, T::pchEvent, T::cParams, T::bPanelEvent, T::CreateEventFromString, T::MakeEvent, NULL, NULL );
	}
};

template < class T, typename TParam1 >
class CAutoRegisterUIEventWithParam1
{
public:
	CAutoRegisterUIEventWithParam1( const char *pch )
	{
		RegisterUIEvent( &T::symbol, T::pchEvent, T::cParams, T::bPanelEvent, T::CreateEventFromString, NULL, NULL, NULL );
	}
};

template < class T >
class CAutoRegisterUIEventWithParam1< T, panorama::EPanelEventSource_t >
{
public:
	CAutoRegisterUIEventWithParam1( const char *pch )
	{
		RegisterUIEvent( &T::symbol, T::pchEvent, T::cParams, T::bPanelEvent, T::CreateEventFromString, NULL, NULL, T::MakeEvent );
	}
};

template < class T >
class CAutoRegisterUIEventWithParam1< T, int >
{
public:
	CAutoRegisterUIEventWithParam1( const char *pch )
	{
		RegisterUIEvent( &T::symbol, T::pchEvent, T::cParams, T::bPanelEvent, T::CreateEventFromString, NULL, T::MakeEvent, NULL );
	}
};

template < class T >
class CAutoRegisterUIEvent<T, 1>
{
public:
	typedef typename T::TypenameParam1 TTypenameParam1;
	CAutoRegisterUIEventWithParam1< T, TTypenameParam1 > m_autoregister;
	CAutoRegisterUIEvent( const char *pch ) : m_autoregister( pch )
	{
	}
};





//-----------------------------------------------------------------------------
// Purpose: Event interface
//-----------------------------------------------------------------------------
class IUIEvent
{
public:
	virtual ~IUIEvent() {}

	virtual const CPanelPtr< const IUIPanel > &GetTargetPanel() const = 0;
	virtual void SetTargetPanel( const IUIPanel *pTarget ) = 0;
	virtual panorama::CPanoramaSymbol GetEventType() const = 0;
	virtual bool CanBubble() const { return false; }

	virtual bool Dispatch( CUtlAbstractDelegate pFunc ) = 0;
	virtual IUIEvent *Copy() const = 0;

	virtual void GetJavaScriptArgs( int *pCount, v8::Handle<v8::Value> **pArgs ) = 0;

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName ) = 0;
#endif

};


//-----------------------------------------------------------------------------
// Purpose: Event base class
//-----------------------------------------------------------------------------
class CUIEventBase : public IUIEvent
{
public:
	CUIEventBase( panorama::CPanoramaSymbol symEvent, const IUIPanel *pTargetPanel )
	{
		m_symEvent = symEvent;
		m_pTargetPanel = pTargetPanel;
	}

	virtual const CPanelPtr< const IUIPanel > &GetTargetPanel() const
	{
		return m_pTargetPanel;
	}

	virtual void SetTargetPanel( const IUIPanel *pTarget )
	{
		m_pTargetPanel = pTarget;
	}

	virtual panorama::CPanoramaSymbol GetEventType() const
	{
		return m_symEvent;
	}

private:
	panorama::CPanoramaSymbol m_symEvent;
	CPanelPtr< const IUIPanel > m_pTargetPanel;
};


//-----------------------------------------------------------------------------
// Purpose: UI Event types
//-----------------------------------------------------------------------------
class CUIEvent0 : public CUIEventBase
{
public:
	CUIEvent0( panorama::CPanoramaSymbol symEvent, const IUIPanel *pTargetPanel ) : CUIEventBase( symEvent, pTargetPanel )
	{
	}

	virtual ~CUIEvent0()
	{
	}

	virtual bool Dispatch( CUtlAbstractDelegate pFunc )
	{
		CUtlDelegate< bool ( void ) > del;
		del.SetAbstractDelegate( pFunc );
		return del();
	}

	virtual IUIEvent *Copy() const
	{
		return new CUIEvent0( GetEventType(), GetTargetPanel().Get() );
	}

	virtual void GetJavaScriptArgs( int *pCount, v8::Handle<v8::Value> **pArgs )
	{
		*pCount = 0;
		*pArgs = NULL;
	}

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName )
	{

	}
#endif
};

class CUIPanelEvent0 : public CUIEventBase
{
public:
	CUIPanelEvent0( panorama::CPanoramaSymbol symEvent, const IUIPanel *pTargetPanel ) : CUIEventBase( symEvent, pTargetPanel )
	{
	}

	virtual ~CUIPanelEvent0()
	{
	}

	virtual bool Dispatch( CUtlAbstractDelegate pFunc )
	{
		CUtlDelegate< bool( const CPanelPtr< const IUIPanel > & ) > del;
		del.SetAbstractDelegate( pFunc );
		return del( GetTargetPanel() );
	}

	virtual IUIEvent *Copy() const
	{
		return new CUIPanelEvent0( GetEventType(), GetTargetPanel().Get() );
	}

	virtual void GetJavaScriptArgs( int *pCount, v8::Handle<v8::Value> **pArgs )
	{
		*pCount = 1;
		*pArgs = new v8::Handle< v8::Value >[1];
		*pArgs[0] = v8::String::NewFromUtf8( GetV8Isolate(), GetPanelID( GetTargetPanel().Get() ) );
	}

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName )
	{
		
	}
#endif
};



template < typename PARAM1_TYPE >
class CUIEvent1 : public CUIEventBase
{
public:
	CUIEvent1( panorama::CPanoramaSymbol symEvent, const IUIPanel *pTargetPanel, PARAM1_TYPE param1 ) : CUIEventBase( symEvent, pTargetPanel )
	{
		UIEventSet( &m_param1, param1 );
	}

	virtual ~CUIEvent1()
	{
		UIEventFree( m_param1 );
	}

	virtual bool Dispatch( CUtlAbstractDelegate pFunc )
	{
		CUtlDelegate< bool ( PARAM1_TYPE ) > del;
		del.SetAbstractDelegate( pFunc );
		return del( m_param1 );
	}

	virtual IUIEvent *Copy() const
	{
		return new CUIEvent1( GetEventType(), GetTargetPanel().Get(), m_param1 );
	}

	virtual void GetJavaScriptArgs( int *pCount, v8::Handle<v8::Value> **pArgs )
	{
		*pCount = 1;
		*pArgs = new v8::Handle< v8::Value>[1];	
		PanoramaTypeToV8Param( m_param1, *pArgs );
	}

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName )
	{
		UIEventValidate( validator, m_param1 );
	}
#endif

	const PARAM1_TYPE &GetParam1() const { return m_param1; }

private:
	PARAM1_TYPE m_param1;
};

template < typename PARAM1_TYPE >
class CUIPanelEvent1 : public CUIEventBase
{
public:
	CUIPanelEvent1( panorama::CPanoramaSymbol symEvent, const IUIPanel *pTargetPanel, PARAM1_TYPE param1 ) : CUIEventBase( symEvent, pTargetPanel )
	{
		UIEventSet( &m_param1, param1 );
	}

	virtual ~CUIPanelEvent1()
	{
		UIEventFree( m_param1 );
	}

	virtual bool Dispatch( CUtlAbstractDelegate pFunc )
	{
		CUtlDelegate< bool( const CPanelPtr< const IUIPanel > &, PARAM1_TYPE ) > del;
		del.SetAbstractDelegate( pFunc );
		return del( GetTargetPanel(), m_param1 );
	}

	virtual IUIEvent *Copy() const
	{
		return new CUIPanelEvent1( GetEventType(), GetTargetPanel().Get(), m_param1 );
	}

	virtual void GetJavaScriptArgs( int *pCount, v8::Handle<v8::Value> **pArgs )
	{
		*pCount = 2;
		*pArgs = new v8::Handle< v8::Value >[2];
		*pArgs[0] = v8::String::NewFromUtf8( GetV8Isolate(), GetPanelID( GetTargetPanel().Get() ) );
		PanoramaTypeToV8Param( m_param1, *pArgs+1 );
	}

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName )
	{
		UIEventValidate( validator, m_param1 );
	}
#endif

	const PARAM1_TYPE &GetParam1() const { return m_param1; }

private:
	PARAM1_TYPE m_param1;
};

template < typename PARAM1_TYPE, typename PARAM2_TYPE >
class CUIPanelEvent2 : public CUIEventBase
{
public:
	CUIPanelEvent2( panorama::CPanoramaSymbol symEvent, const IUIPanel *pTargetPanel, PARAM1_TYPE param1, PARAM2_TYPE param2 ) : CUIEventBase( symEvent, pTargetPanel )
	{
		UIEventSet( &m_param1, param1 );
		UIEventSet( &m_param2, param2 );
	}

	virtual ~CUIPanelEvent2()
	{
		UIEventFree( m_param1 );
		UIEventFree( m_param2 );
	}

	virtual bool Dispatch( CUtlAbstractDelegate pFunc )
	{
		CUtlDelegate< bool( const CPanelPtr< const IUIPanel > &, PARAM1_TYPE, PARAM2_TYPE ) > del;
		del.SetAbstractDelegate( pFunc );
		return del( GetTargetPanel(), m_param1, m_param2 );
	}

	virtual IUIEvent *Copy() const
	{
		return new CUIPanelEvent2( GetEventType(), GetTargetPanel().Get(), m_param1, m_param2 );
	}

	virtual void GetJavaScriptArgs( int *pCount, v8::Handle<v8::Value> **pArgs )
	{
		*pCount = 3;
		*pArgs = new v8::Handle< v8::Value >[3];
		*pArgs[0] = v8::String::NewFromUtf8( GetV8Isolate(), GetPanelID( GetTargetPanel().Get() ) );
		PanoramaTypeToV8Param( m_param1, *pArgs+1 );
		PanoramaTypeToV8Param( m_param2, *pArgs+2 );
	}

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName )
	{
		UIEventValidate( validator, m_param1 );
		UIEventValidate( validator, m_param2 );
	}
#endif

	const PARAM1_TYPE &GetParam1() const { return m_param1; }
	const PARAM2_TYPE &GetParam2() const { return m_param2; }

private:
	PARAM1_TYPE m_param1;
	PARAM2_TYPE m_param2;
};


template < typename PARAM1_TYPE, typename PARAM2_TYPE >
class CUIEvent2 : public CUIEventBase
{
public:
	CUIEvent2( panorama::CPanoramaSymbol symEvent, const IUIPanel *pTargetPanel, PARAM1_TYPE param1, PARAM2_TYPE param2 ) : CUIEventBase( symEvent, pTargetPanel )
	{
		UIEventSet( &m_param1, param1 );
		UIEventSet( &m_param2, param2 );
	}

	virtual ~CUIEvent2()
	{
		UIEventFree( m_param1 );
		UIEventFree( m_param2 );
	}

	virtual bool Dispatch( CUtlAbstractDelegate pFunc )
	{
		CUtlDelegate< bool ( PARAM1_TYPE, PARAM2_TYPE ) > del;
		del.SetAbstractDelegate( pFunc );
		return del( m_param1, m_param2 );
	}

	virtual IUIEvent *Copy() const
	{
		return new CUIEvent2( GetEventType(), GetTargetPanel().Get(), m_param1, m_param2 );
	}

	virtual void GetJavaScriptArgs( int *pCount, v8::Handle<v8::Value> **pArgs )
	{
		*pCount = 2;
		*pArgs = new v8::Handle< v8::Value >[2];
		PanoramaTypeToV8Param( m_param1, *pArgs+0 );
		PanoramaTypeToV8Param( m_param2, *pArgs+1 );
	}

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName )
	{
		UIEventValidate( validator, m_param1 );
		UIEventValidate( validator, m_param2 );
	}
#endif

	const PARAM1_TYPE &GetParam1() const { return m_param1; }
	const PARAM2_TYPE &GetParam2() const { return m_param2; }

private:
	PARAM1_TYPE m_param1;
	PARAM2_TYPE m_param2;
};

template < typename PARAM1_TYPE, typename PARAM2_TYPE, typename PARAM3_TYPE >
class CUIEvent3 : public CUIEventBase
{
public:
	CUIEvent3( panorama::CPanoramaSymbol symEvent, const IUIPanel *pTargetPanel, PARAM1_TYPE param1, PARAM2_TYPE param2, PARAM3_TYPE param3 ) : CUIEventBase( symEvent, pTargetPanel )
	{
		UIEventSet( &m_param1, param1 );
		UIEventSet( &m_param2, param2 );
		UIEventSet( &m_param3, param3 );
	}

	virtual ~CUIEvent3()
	{
		UIEventFree( m_param1 );
		UIEventFree( m_param2 );
		UIEventFree( m_param3 );
	}

	virtual bool Dispatch( CUtlAbstractDelegate pFunc )
	{
		CUtlDelegate< bool ( PARAM1_TYPE, PARAM2_TYPE, PARAM3_TYPE ) > del;
		del.SetAbstractDelegate( pFunc );
		return del( m_param1, m_param2, m_param3 );
	}

	virtual IUIEvent *Copy() const
	{
		return new CUIEvent3( GetEventType(), GetTargetPanel().Get(), m_param1, m_param2, m_param3 );
	}

	virtual void GetJavaScriptArgs( int *pCount, v8::Handle<v8::Value> **pArgs )
	{
		*pCount = 3;
		*pArgs = new v8::Handle< v8::Value >[3];
		PanoramaTypeToV8Param( m_param1, *pArgs+0 );
		PanoramaTypeToV8Param( m_param2, *pArgs+1 );
		PanoramaTypeToV8Param( m_param3, *pArgs+2 );
	}

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName )
	{
		UIEventValidate( validator, m_param1 );
		UIEventValidate( validator, m_param2 );
		UIEventValidate( validator, m_param3 );
	}
#endif

	const PARAM1_TYPE &GetParam1() const { return m_param1; }
	const PARAM2_TYPE &GetParam2() const { return m_param2; }
	const PARAM3_TYPE &GetParam3() const { return m_param3; }

private:
	PARAM1_TYPE m_param1;
	PARAM2_TYPE m_param2;
	PARAM3_TYPE m_param3;
};

template < typename PARAM1_TYPE, typename PARAM2_TYPE, typename PARAM3_TYPE >
class CUIPanelEvent3 : public CUIEventBase
{
public:
	CUIPanelEvent3( panorama::CPanoramaSymbol symEvent, const IUIPanel *pTargetPanel, PARAM1_TYPE param1, PARAM2_TYPE param2, PARAM3_TYPE param3 ) : CUIEventBase( symEvent, pTargetPanel )
	{
		UIEventSet( &m_param1, param1 );
		UIEventSet( &m_param2, param2 );
		UIEventSet( &m_param3, param3 );
	}

	virtual ~CUIPanelEvent3()
	{
		UIEventFree( m_param1 );
		UIEventFree( m_param2 );
		UIEventFree( m_param3 );
	}

	virtual bool Dispatch( CUtlAbstractDelegate pFunc )
	{
		CUtlDelegate< bool( const CPanelPtr< const IUIPanel > &, PARAM1_TYPE, PARAM2_TYPE, PARAM3_TYPE ) > del;
		del.SetAbstractDelegate( pFunc );
		return del( GetTargetPanel(), m_param1, m_param2, m_param3 );
	}

	virtual IUIEvent *Copy() const
	{
		return new CUIPanelEvent3( GetEventType(), GetTargetPanel().Get(), m_param1, m_param2, m_param3 );
	}

	virtual void GetJavaScriptArgs( int *pCount, v8::Handle<v8::Value> **pArgs )
	{
		*pCount = 4;
		*pArgs = new v8::Handle< v8::Value >[4];
		*pArgs[0] = v8::String::NewFromUtf8( GetV8Isolate(), GetPanelID( GetTargetPanel().Get() ) );
		PanoramaTypeToV8Param( m_param1, *pArgs+1 );
		PanoramaTypeToV8Param( m_param2, *pArgs+2 );
		PanoramaTypeToV8Param( m_param3, *pArgs+3 );
	}

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName )
	{
		UIEventValidate( validator, m_param1 );
		UIEventValidate( validator, m_param2 );
		UIEventValidate( validator, m_param3 );
	}
#endif

	const PARAM1_TYPE &GetParam1() const { return m_param1; }
	const PARAM2_TYPE &GetParam2() const { return m_param2; }
	const PARAM3_TYPE &GetParam3() const { return m_param3; }

private:
	PARAM1_TYPE m_param1;
	PARAM2_TYPE m_param2;
	PARAM3_TYPE m_param3;
};

template < typename PARAM1_TYPE, typename PARAM2_TYPE, typename PARAM3_TYPE, typename PARAM4_TYPE >
class CUIEvent4 : public CUIEventBase
{
public:
	CUIEvent4( panorama::CPanoramaSymbol symEvent, const IUIPanel *pTargetPanel, PARAM1_TYPE param1, PARAM2_TYPE param2, PARAM3_TYPE param3, PARAM4_TYPE param4 ) : CUIEventBase( symEvent, pTargetPanel )
	{
		UIEventSet( &m_param1, param1 );
		UIEventSet( &m_param2, param2 );
		UIEventSet( &m_param3, param3 );
		UIEventSet( &m_param4, param4 );
	}

	virtual ~CUIEvent4()
	{
		UIEventFree( m_param1 );
		UIEventFree( m_param2 );
		UIEventFree( m_param3 );
		UIEventFree( m_param4 );
	}

	virtual bool Dispatch( CUtlAbstractDelegate pFunc )
	{
		CUtlDelegate< bool ( PARAM1_TYPE, PARAM2_TYPE, PARAM3_TYPE, PARAM4_TYPE ) > del;
		del.SetAbstractDelegate( pFunc );
		return del( m_param1, m_param2, m_param3, m_param4 );
	}

	virtual IUIEvent *Copy() const
	{
		return new CUIEvent4( GetEventType(), GetTargetPanel().Get(), m_param1, m_param2, m_param3, m_param4 );
	}

	virtual void GetJavaScriptArgs( int *pCount, v8::Handle<v8::Value> **pArgs )
	{
		*pCount = 4;
		*pArgs = new v8::Handle< v8::Value >[4];
		PanoramaTypeToV8Param( m_param1, *pArgs+0 );
		PanoramaTypeToV8Param( m_param2, *pArgs+1 );
		PanoramaTypeToV8Param( m_param3, *pArgs+2 );
		PanoramaTypeToV8Param( m_param4, *pArgs+3 );
	}

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName )
	{
		UIEventValidate( validator, m_param1 );
		UIEventValidate( validator, m_param2 );
		UIEventValidate( validator, m_param3 );
		UIEventValidate( validator, m_param4 );
	}
#endif

	const PARAM1_TYPE &GetParam1() const { return m_param1; }
	const PARAM2_TYPE &GetParam2() const { return m_param2; }
	const PARAM3_TYPE &GetParam3() const { return m_param3; }
	const PARAM4_TYPE &GetParam4() const { return m_param4; }

private:
	PARAM1_TYPE m_param1;
	PARAM2_TYPE m_param2;
	PARAM3_TYPE m_param3;
	PARAM4_TYPE m_param4;
};


template < typename PARAM1_TYPE, typename PARAM2_TYPE, typename PARAM3_TYPE, typename PARAM4_TYPE >
class CUIPanelEvent4 : public CUIEventBase
{
public:
	CUIPanelEvent4( panorama::CPanoramaSymbol symEvent, const IUIPanel *pTargetPanel, PARAM1_TYPE param1, PARAM2_TYPE param2, PARAM3_TYPE param3, PARAM4_TYPE param4 ) : CUIEventBase( symEvent, pTargetPanel )
	{
		UIEventSet( &m_param1, param1 );
		UIEventSet( &m_param2, param2 );
		UIEventSet( &m_param3, param3 );
		UIEventSet( &m_param4, param4 );
	}

	virtual ~CUIPanelEvent4()
	{
		UIEventFree( m_param1 );
		UIEventFree( m_param2 );
		UIEventFree( m_param3 );
		UIEventFree( m_param4 );
	}

	virtual bool Dispatch( CUtlAbstractDelegate pFunc )
	{
		CUtlDelegate< bool( const CPanelPtr< const IUIPanel > &, PARAM1_TYPE, PARAM2_TYPE, PARAM3_TYPE, PARAM4_TYPE ) > del;
		del.SetAbstractDelegate( pFunc );
		return del( GetTargetPanel(), m_param1, m_param2, m_param3, m_param4 );
	}

	virtual IUIEvent *Copy() const
	{
		return new CUIPanelEvent4( GetEventType(), GetTargetPanel().Get(), m_param1, m_param2, m_param3, m_param4 );
	}

	virtual void GetJavaScriptArgs( int *pCount, v8::Handle<v8::Value> **pArgs )
	{
		*pCount = 5;
		*pArgs = new v8::Handle< v8::Value >[5];
		*pArgs[0] = v8::String::NewFromUtf8( GetV8Isolate(), GetPanelID( GetTargetPanel().Get() ) );
		PanoramaTypeToV8Param( m_param1, *pArgs+1 );
		PanoramaTypeToV8Param( m_param2, *pArgs+2 );
		PanoramaTypeToV8Param( m_param3, *pArgs+3 );
		PanoramaTypeToV8Param( m_param4, *pArgs+4 );
	}

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName )
	{
		UIEventValidate( validator, m_param1 );
		UIEventValidate( validator, m_param2 );
		UIEventValidate( validator, m_param3 );
		UIEventValidate( validator, m_param4 );
	}
#endif

	const PARAM1_TYPE &GetParam1() const { return m_param1; }
	const PARAM2_TYPE &GetParam2() const { return m_param2; }
	const PARAM3_TYPE &GetParam3() const { return m_param3; }
	const PARAM4_TYPE &GetParam4() const { return m_param4; }

private:
	PARAM1_TYPE m_param1;
	PARAM2_TYPE m_param2;
	PARAM3_TYPE m_param3;
	PARAM4_TYPE m_param4;
};


template < typename PARAM1_TYPE, typename PARAM2_TYPE, typename PARAM3_TYPE, typename PARAM4_TYPE, typename PARAM5_TYPE >
class CUIEvent5 : public CUIEventBase
{
public:
	CUIEvent5( panorama::CPanoramaSymbol symEvent, const IUIPanel *pTargetPanel, PARAM1_TYPE param1, PARAM2_TYPE param2, PARAM3_TYPE param3, PARAM4_TYPE param4, PARAM5_TYPE param5 ) : CUIEventBase( symEvent, pTargetPanel )
	{
		UIEventSet( &m_param1, param1 );
		UIEventSet( &m_param2, param2 );
		UIEventSet( &m_param3, param3 );
		UIEventSet( &m_param4, param4 );
		UIEventSet( &m_param5, param5 );
	}

	virtual ~CUIEvent5()
	{
		UIEventFree( m_param1 );
		UIEventFree( m_param2 );
		UIEventFree( m_param3 );
		UIEventFree( m_param4 );
		UIEventFree( m_param5 );
	}

	virtual bool Dispatch( CUtlAbstractDelegate pFunc )
	{
		CUtlDelegate< bool ( PARAM1_TYPE, PARAM2_TYPE, PARAM3_TYPE, PARAM4_TYPE, PARAM5_TYPE ) > del;
		del.SetAbstractDelegate( pFunc );
		return del( m_param1, m_param2, m_param3, m_param4, m_param5  );
	}

	virtual IUIEvent *Copy() const
	{
		return new CUIEvent5( GetEventType(), GetTargetPanel().Get(), m_param1, m_param2, m_param3, m_param4, m_param5 );
	}

	virtual void GetJavaScriptArgs( int *pCount, v8::Handle<v8::Value> **pArgs )
	{
		*pCount = 5;
		*pArgs = new v8::Handle< v8::Value >[5];
		PanoramaTypeToV8Param( m_param1, *pArgs+0 );
		PanoramaTypeToV8Param( m_param2, *pArgs+1 );
		PanoramaTypeToV8Param( m_param3, *pArgs+2 );
		PanoramaTypeToV8Param( m_param4, *pArgs+3 );
		PanoramaTypeToV8Param( m_param5, *pArgs+4 );
	}

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName )
	{
		UIEventValidate( validator, m_param1 );
		UIEventValidate( validator, m_param2 );
		UIEventValidate( validator, m_param3 );
		UIEventValidate( validator, m_param4 );
		UIEventValidate( validator, m_param5 );
	}
#endif

	const PARAM1_TYPE &GetParam1() const { return m_param1; }
	const PARAM2_TYPE &GetParam2() const { return m_param2; }
	const PARAM3_TYPE &GetParam3() const { return m_param3; }
	const PARAM4_TYPE &GetParam4() const { return m_param4; }
	const PARAM5_TYPE &GetParam5() const { return m_param5; }

private:
	PARAM1_TYPE m_param1;
	PARAM2_TYPE m_param2;
	PARAM3_TYPE m_param3;
	PARAM4_TYPE m_param4;
	PARAM5_TYPE m_param5;
};


template < typename PARAM1_TYPE, typename PARAM2_TYPE, typename PARAM3_TYPE, typename PARAM4_TYPE, typename PARAM5_TYPE >
class CUIPanelEvent5 : public CUIEventBase
{
public:
	CUIPanelEvent5( panorama::CPanoramaSymbol symEvent, const IUIPanel *pTargetPanel, PARAM1_TYPE param1, PARAM2_TYPE param2, PARAM3_TYPE param3, PARAM4_TYPE param4, PARAM5_TYPE param5 ) : CUIEventBase( symEvent, pTargetPanel )
	{
		UIEventSet( &m_param1, param1 );
		UIEventSet( &m_param2, param2 );
		UIEventSet( &m_param3, param3 );
		UIEventSet( &m_param4, param4 );
		UIEventSet( &m_param5, param5 );
	}

	virtual ~CUIPanelEvent5()
	{
		UIEventFree( m_param1 );
		UIEventFree( m_param2 );
		UIEventFree( m_param3 );
		UIEventFree( m_param4 );
		UIEventFree( m_param5 );
	}

	virtual bool Dispatch( CUtlAbstractDelegate pFunc )
	{
		CUtlDelegate< bool( const CPanelPtr< const IUIPanel > &, PARAM1_TYPE, PARAM2_TYPE, PARAM3_TYPE, PARAM4_TYPE, PARAM5_TYPE ) > del;
		del.SetAbstractDelegate( pFunc );
		return del( GetTargetPanel(), m_param1, m_param2, m_param3, m_param4, m_param5 );
	}

	virtual IUIEvent *Copy() const
	{
		return new CUIPanelEvent5( GetEventType(), GetTargetPanel().Get(), m_param1, m_param2, m_param3, m_param4, m_param5 );
	}

	virtual void GetJavaScriptArgs( int *pCount, v8::Handle<v8::Value> **pArgs )
	{
		*pCount = 6;
		*pArgs = new v8::Handle< v8::Value >[6];
		*pArgs[0] = v8::String::NewFromUtf8( GetV8Isolate(), GetPanelID( GetTargetPanel().Get() ) );
		PanoramaTypeToV8Param( m_param1, *pArgs+1 );
		PanoramaTypeToV8Param( m_param2, *pArgs+2 );
		PanoramaTypeToV8Param( m_param3, *pArgs+3 );
		PanoramaTypeToV8Param( m_param4, *pArgs+4 );
		PanoramaTypeToV8Param( m_param5, *pArgs+5 );
	}

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName )
	{
		UIEventValidate( validator, m_param1 );
		UIEventValidate( validator, m_param2 );
		UIEventValidate( validator, m_param3 );
		UIEventValidate( validator, m_param4 );
		UIEventValidate( validator, m_param5 );
	}
#endif

	const PARAM1_TYPE &GetParam1() const { return m_param1; }
	const PARAM2_TYPE &GetParam2() const { return m_param2; }
	const PARAM3_TYPE &GetParam3() const { return m_param3; }
	const PARAM4_TYPE &GetParam4() const { return m_param4; }
	const PARAM5_TYPE &GetParam5() const { return m_param5; }

private:
	PARAM1_TYPE m_param1;
	PARAM2_TYPE m_param2;
	PARAM3_TYPE m_param3;
	PARAM4_TYPE m_param4;
	PARAM5_TYPE m_param5;
};


//-----------------------------------------------------------------------------
// Purpose: Dispatch synchronous event helpers
//-----------------------------------------------------------------------------
#ifdef PANORAMA_EXPORTS
template < typename T >
bool DispatchEvent( T t, const IUIPanel *pTarget )
{
	if ( !UIEngine()->BAnyHandlerRegisteredForEvent( T::symbol ) ) 
		return false;

	return UIEngine()->DispatchEvent( t.MakeEvent( pTarget ? pTarget->ClientPtr() : NULL ) );
}

template < typename T, typename PARAM1_TYPE >
bool DispatchEvent( T t, const IUIPanel *pTarget, PARAM1_TYPE param1 )
{
	if ( !UIEngine()->BAnyHandlerRegisteredForEvent( T::symbol ) ) 
		return false;

	return UIEngine()->DispatchEvent( t.MakeEvent( pTarget ? pTarget->ClientPtr() : NULL, param1 ) );
}

template < typename T, typename PARAM1_TYPE, typename PARAM2_TYPE >
bool DispatchEvent( T t, const IUIPanel *pTarget, PARAM1_TYPE param1, PARAM2_TYPE param2 )
{
	if ( !UIEngine()->BAnyHandlerRegisteredForEvent( T::symbol ) ) 
		return false;

	return UIEngine()->DispatchEvent( t.MakeEvent( pTarget ? pTarget->ClientPtr() : NULL, param1, param2 ) );
}

template < typename T, typename PARAM1_TYPE, typename PARAM2_TYPE, typename PARAM3_TYPE >
bool DispatchEvent( T t, const IUIPanel *pTarget, PARAM1_TYPE param1, PARAM2_TYPE param2, PARAM3_TYPE param3 )
{
	if ( !UIEngine()->BAnyHandlerRegisteredForEvent( T::symbol ) ) 
		return false;

	return UIEngine()->DispatchEvent( t.MakeEvent( pTarget ? pTarget->ClientPtr() : NULL, param1, param2, param3 ) );
}

template < typename T, typename PARAM1_TYPE, typename PARAM2_TYPE, typename PARAM3_TYPE, typename PARAM4_TYPE >
bool DispatchEvent( T t, const IUIPanel *pTarget, PARAM1_TYPE param1, PARAM2_TYPE param2, PARAM3_TYPE param3, PARAM4_TYPE param4 )
{
	if ( !UIEngine()->BAnyHandlerRegisteredForEvent( T::symbol ) ) 
		return false;

	return UIEngine()->DispatchEvent( t.MakeEvent( pTarget ? pTarget->ClientPtr() : NULL, param1, param2, param3, param4 ) );
}

template < typename T, typename PARAM1_TYPE, typename PARAM2_TYPE, typename PARAM3_TYPE, typename PARAM4_TYPE, typename PARAM5_TYPE >
bool DispatchEvent( T t, const IUIPanel *pTarget, PARAM1_TYPE param1, PARAM2_TYPE param2, PARAM3_TYPE param3, PARAM4_TYPE param4, PARAM5_TYPE param5 )
{
	if ( !UIEngine()->BAnyHandlerRegisteredForEvent( T::symbol ) ) 
		return false;

	return UIEngine()->DispatchEvent( t.MakeEvent( pTarget ? pTarget->ClientPtr() : NULL, param1, param2, param3, param4, param5 ) );
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Dispatch synchronous event helpers
//-----------------------------------------------------------------------------
template < typename T >
bool DispatchEvent( T t, const IUIPanelClient *pTarget )
{
	if( !UIEngine()->BAnyHandlerRegisteredForEvent( T::symbol ) )
		return false;

	return UIEngine()->DispatchEvent( t.MakeEvent( pTarget ) );
}

template < typename T, typename PARAM1_TYPE >
bool DispatchEvent( T t, const IUIPanelClient *pTarget, PARAM1_TYPE param1 )
{
	if( !UIEngine()->BAnyHandlerRegisteredForEvent( T::symbol ) )
		return false;

	return UIEngine()->DispatchEvent( t.MakeEvent( pTarget, param1 ) );
}

template < typename T, typename PARAM1_TYPE, typename PARAM2_TYPE >
bool DispatchEvent( T t, const IUIPanelClient *pTarget, PARAM1_TYPE param1, PARAM2_TYPE param2 )
{
	if( !UIEngine()->BAnyHandlerRegisteredForEvent( T::symbol ) )
		return false;

	return UIEngine()->DispatchEvent( t.MakeEvent( pTarget, param1, param2 ) );
}

template < typename T, typename PARAM1_TYPE, typename PARAM2_TYPE, typename PARAM3_TYPE >
bool DispatchEvent( T t, const IUIPanelClient *pTarget, PARAM1_TYPE param1, PARAM2_TYPE param2, PARAM3_TYPE param3 )
{
	if( !UIEngine()->BAnyHandlerRegisteredForEvent( T::symbol ) )
		return false;

	return UIEngine()->DispatchEvent( t.MakeEvent( pTarget, param1, param2, param3 ) );
}

template < typename T, typename PARAM1_TYPE, typename PARAM2_TYPE, typename PARAM3_TYPE, typename PARAM4_TYPE >
bool DispatchEvent( T t, const IUIPanelClient *pTarget, PARAM1_TYPE param1, PARAM2_TYPE param2, PARAM3_TYPE param3, PARAM4_TYPE param4 )
{
	if( !UIEngine()->BAnyHandlerRegisteredForEvent( T::symbol ) )
		return false;

	return UIEngine()->DispatchEvent( t.MakeEvent( pTarget, param1, param2, param3, param4 ) );
}

template < typename T, typename PARAM1_TYPE, typename PARAM2_TYPE, typename PARAM3_TYPE, typename PARAM4_TYPE, typename PARAM5_TYPE >
bool DispatchEvent( T t, const IUIPanelClient *pTarget, PARAM1_TYPE param1, PARAM2_TYPE param2, PARAM3_TYPE param3, PARAM4_TYPE param4, PARAM5_TYPE param5 )
{
	if( !UIEngine()->BAnyHandlerRegisteredForEvent( T::symbol ) )
		return false;

	return UIEngine()->DispatchEvent( t.MakeEvent( pTarget, param1, param2, param3, param4, param5 ) );
}

#ifdef PANORAMA_EXPORTS
//-----------------------------------------------------------------------------
// Purpose: Dispatch asynchronous event helpers
//-----------------------------------------------------------------------------
template < typename T >
void DispatchEventAsync( T t, const IUIPanel *pTarget )
{
	if ( !UIEngine()->BAnyHandlerRegisteredForEvent( T::symbol ) ) 
		return;

	UIEngine()->DispatchEventAsync( 0.0f, t.MakeEvent( pTarget ? pTarget->ClientPtr() : NULL ) );
}

template < typename T >
void DispatchEventAsync( float flDelay, T t, const IUIPanel *pTarget )
{
	if ( !UIEngine()->BAnyHandlerRegisteredForEvent( T::symbol ) ) 
		return;

	UIEngine()->DispatchEventAsync( flDelay, t.MakeEvent( pTarget ? pTarget->ClientPtr() : NULL ) );
}


template < typename T, typename PARAM1_TYPE >
void DispatchEventAsync( T t, const IUIPanel *pTarget, PARAM1_TYPE param1 )
{
	if ( !UIEngine()->BAnyHandlerRegisteredForEvent( T::symbol ) ) 
		return;

	UIEngine()->DispatchEventAsync( 0.0f, t.MakeEvent( pTarget ? pTarget->ClientPtr() : NULL, param1 ) );
}

template < typename T, typename PARAM1_TYPE >
void DispatchEventAsync( float flDelay, T t, const IUIPanel *pTarget, PARAM1_TYPE param1 )
{
	if ( !UIEngine()->BAnyHandlerRegisteredForEvent( T::symbol ) ) 
		return;

	UIEngine()->DispatchEventAsync( flDelay, t.MakeEvent( pTarget ? pTarget->ClientPtr() : NULL, param1 ) );
}

template < typename T, typename PARAM1_TYPE, typename PARAM2_TYPE  >
void DispatchEventAsync( T t, const IUIPanel *pTarget, PARAM1_TYPE param1, PARAM2_TYPE param2 )
{
	if ( !UIEngine()->BAnyHandlerRegisteredForEvent( T::symbol ) ) 
		return;

	UIEngine()->DispatchEventAsync( 0.0f, t.MakeEvent( pTarget ? pTarget->ClientPtr() : NULL, param1, param2 ) );
}

template < typename T, typename PARAM1_TYPE, typename PARAM2_TYPE, typename PARAM3_TYPE >
void DispatchEventAsync( T t, const IUIPanel *pTarget, PARAM1_TYPE param1, PARAM2_TYPE param2, PARAM3_TYPE param3 )
{
	if ( !UIEngine()->BAnyHandlerRegisteredForEvent( T::symbol ) ) 
		return;

	UIEngine()->DispatchEventAsync( 0.0f, t.MakeEvent( pTarget ? pTarget->ClientPtr() : NULL, param1, param2, param3 ) );
}

template < typename T, typename PARAM1_TYPE, typename PARAM2_TYPE  >
void DispatchEventAsync( float flDelay, T t, const IUIPanel *pTarget, PARAM1_TYPE param1, PARAM2_TYPE param2 )
{
	if ( !UIEngine()->BAnyHandlerRegisteredForEvent( T::symbol ) ) 
		return;

	UIEngine()->DispatchEventAsync( flDelay, t.MakeEvent( pTarget ? pTarget->ClientPtr() : NULL, param1, param2 ) );
}

template < typename T, typename PARAM1_TYPE, typename PARAM2_TYPE, typename PARAM3_TYPE >
void DispatchEventAsync( float flDelay, T t, const IUIPanel *pTarget, PARAM1_TYPE param1, PARAM2_TYPE param2, PARAM3_TYPE param3 )
{
	if ( !UIEngine()->BAnyHandlerRegisteredForEvent( T::symbol ) ) 
		return;

	UIEngine()->DispatchEventAsync( flDelay, t.MakeEvent( pTarget ? pTarget->ClientPtr() : NULL, param1, param2, param3 ) );
}

template < typename T, typename PARAM1_TYPE, typename PARAM2_TYPE, typename PARAM3_TYPE, typename PARAM4_TYPE >
void DispatchEventAsync( float flDelay, T t, const IUIPanel *pTarget, PARAM1_TYPE param1, PARAM2_TYPE param2, PARAM3_TYPE param3, PARAM4_TYPE param4 )
{
	if ( !UIEngine()->BAnyHandlerRegisteredForEvent( T::symbol ) ) 
		return;

	UIEngine()->DispatchEventAsync( flDelay, t.MakeEvent( pTarget ? pTarget->ClientPtr() : NULL, param1, param2, param3, param4 ) );
}

template < typename T, typename PARAM1_TYPE, typename PARAM2_TYPE, typename PARAM3_TYPE, typename PARAM4_TYPE, typename PARAM5_TYPE >
void DispatchEventAsync( float flDelay, T t, const IUIPanel *pTarget, PARAM1_TYPE param1, PARAM2_TYPE param2, PARAM3_TYPE param3, PARAM4_TYPE param4, PARAM5_TYPE param5 )
{
	if ( !UIEngine()->BAnyHandlerRegisteredForEvent( T::symbol ) ) 
		return;

	UIEngine()->DispatchEventAsync( flDelay, t.MakeEvent( pTarget ? pTarget->ClientPtr() : NULL, param1, param2, param3, param4, param5 ) );
}
#endif

template < typename T >
void DispatchEventAsync( T t, const IUIPanelClient *pTarget )
{
	if( !UIEngine()->BAnyHandlerRegisteredForEvent( T::symbol ) )
		return;

	UIEngine()->DispatchEventAsync( 0.0f, t.MakeEvent( pTarget ) );
}

template < typename T >
void DispatchEventAsync( float flDelay, T t, const IUIPanelClient *pTarget )
{
	if( !UIEngine()->BAnyHandlerRegisteredForEvent( T::symbol ) )
		return;

	UIEngine()->DispatchEventAsync( flDelay, t.MakeEvent( pTarget ) );
}


template < typename T, typename PARAM1_TYPE >
void DispatchEventAsync( T t, const IUIPanelClient *pTarget, PARAM1_TYPE param1 )
{
	if( !UIEngine()->BAnyHandlerRegisteredForEvent( T::symbol ) )
		return;

	UIEngine()->DispatchEventAsync( 0.0f, t.MakeEvent( pTarget, param1 ) );
}

template < typename T, typename PARAM1_TYPE >
void DispatchEventAsync( float flDelay, T t, const IUIPanelClient *pTarget, PARAM1_TYPE param1 )
{
	if( !UIEngine()->BAnyHandlerRegisteredForEvent( T::symbol ) )
		return;

	UIEngine()->DispatchEventAsync( flDelay, t.MakeEvent( pTarget, param1 ) );
}

template < typename T, typename PARAM1_TYPE, typename PARAM2_TYPE  >
void DispatchEventAsync( T t, const IUIPanelClient *pTarget, PARAM1_TYPE param1, PARAM2_TYPE param2 )
{
	if( !UIEngine()->BAnyHandlerRegisteredForEvent( T::symbol ) )
		return;

	UIEngine()->DispatchEventAsync( 0.0f, t.MakeEvent( pTarget, param1, param2 ) );
}

template < typename T, typename PARAM1_TYPE, typename PARAM2_TYPE, typename PARAM3_TYPE >
void DispatchEventAsync( T t, const IUIPanelClient *pTarget, PARAM1_TYPE param1, PARAM2_TYPE param2, PARAM3_TYPE param3 )
{
	if( !UIEngine()->BAnyHandlerRegisteredForEvent( T::symbol ) )
		return;

	UIEngine()->DispatchEventAsync( 0.0f, t.MakeEvent( pTarget, param1, param2, param3 ) );
}

template < typename T, typename PARAM1_TYPE, typename PARAM2_TYPE  >
void DispatchEventAsync( float flDelay, T t, const IUIPanelClient *pTarget, PARAM1_TYPE param1, PARAM2_TYPE param2 )
{
	if( !UIEngine()->BAnyHandlerRegisteredForEvent( T::symbol ) )
		return;

	UIEngine()->DispatchEventAsync( flDelay, t.MakeEvent( pTarget, param1, param2 ) );
}

template < typename T, typename PARAM1_TYPE, typename PARAM2_TYPE, typename PARAM3_TYPE >
void DispatchEventAsync( float flDelay, T t, const IUIPanelClient *pTarget, PARAM1_TYPE param1, PARAM2_TYPE param2, PARAM3_TYPE param3 )
{
	if( !UIEngine()->BAnyHandlerRegisteredForEvent( T::symbol ) )
		return;

	UIEngine()->DispatchEventAsync( flDelay, t.MakeEvent( pTarget, param1, param2, param3 ) );
}

template < typename T, typename PARAM1_TYPE, typename PARAM2_TYPE, typename PARAM3_TYPE, typename PARAM4_TYPE >
void DispatchEventAsync( float flDelay, T t, const IUIPanelClient *pTarget, PARAM1_TYPE param1, PARAM2_TYPE param2, PARAM3_TYPE param3, PARAM4_TYPE param4 )
{
	if( !UIEngine()->BAnyHandlerRegisteredForEvent( T::symbol ) )
		return;

	UIEngine()->DispatchEventAsync( flDelay, t.MakeEvent( pTarget, param1, param2, param3, param4 ) );
}

template < typename T, typename PARAM1_TYPE, typename PARAM2_TYPE, typename PARAM3_TYPE, typename PARAM4_TYPE, typename PARAM5_TYPE >
void DispatchEventAsync( float flDelay, T t, const IUIPanelClient *pTarget, PARAM1_TYPE param1, PARAM2_TYPE param2, PARAM3_TYPE param3, PARAM4_TYPE param4, PARAM5_TYPE param5 )
{
	if( !UIEngine()->BAnyHandlerRegisteredForEvent( T::symbol ) )
		return;

	UIEngine()->DispatchEventAsync( flDelay, t.MakeEvent( pTarget, param1, param2, param3, param4, param5 ) );
}


/*
IUIEvent *CreateEventFromSymbol( panorama::CPanoramaSymbol symEvent, IUIPanel *pPanel );
template <typename PARAM1> IUIEvent *CreateEventFromSymbol( panorama::CPanoramaSymbol symEvent, IUIPanel *pPanel, PARAM1 p1 );
template <typename PARAM1, typename PARAM2> IUIEvent *CreateEventFromSymbol( panorama::CPanoramaSymbol symEvent, IUIPanel *pPanel, PARAM1 p1, PARAM2 p2 );
template <typename PARAM1, typename PARAM2, typename PARAM3> IUIEvent *CreateEventFromSymbol( panorama::CPanoramaSymbol symEvent, IUIPanel *pPanel, PARAM1 p1, PARAM2 p2, PARAM3 p3 );
*/
} // namespace panorama 

#endif // UIEVENT_H
