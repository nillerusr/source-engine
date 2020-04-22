
//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose:
//=============================================================================//

#ifndef UIJSREGISTRATION_H
#define UIJSREGISTRATION_H
#pragma once

#include "../panorama/uiengine.h"
#if defined( OSX ) && !defined( SOURCE2_PANORAMA )
	#include <tr1/tuple>
	#include <tr1/utility>
	#define STDTR1 std::tr1
#else
	#define STDTR1 std
#endif

#if _GNUC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuninitialized"
#endif // _GNUC

#ifdef None
#undef None // X11 defines None breaking the ability to use v8::None down below, so remove the preprocessor macro here
#endif

#pragma warning(push)
//warning C4700 : uninitialized local variable 'getcaster' used -- we'll do a lot of this, but its ok
#pragma warning( disable : 4700 )

namespace panorama
{

	template<typename T> struct RegisterJSTypeDeducer_t { static RegisterJSType_t Type() { return k_ERegisterJSTypeUnknown; } };
#define PANORAMA_DECLARE_DEDUCE_JSTYPE( _JSType, _T ) template<> struct RegisterJSTypeDeducer_t<_T> { static RegisterJSType_t Type() { return k_ERegisterJSType##_JSType; } };
	PANORAMA_DECLARE_DEDUCE_JSTYPE( Void, void )
	PANORAMA_DECLARE_DEDUCE_JSTYPE( Bool, bool )
	PANORAMA_DECLARE_DEDUCE_JSTYPE( Int8, int8 )
	PANORAMA_DECLARE_DEDUCE_JSTYPE( Uint8, uint8 )
	PANORAMA_DECLARE_DEDUCE_JSTYPE( Int16, int16 )
	PANORAMA_DECLARE_DEDUCE_JSTYPE( Uint16, uint16 )
	PANORAMA_DECLARE_DEDUCE_JSTYPE( Int32, int32 )
	PANORAMA_DECLARE_DEDUCE_JSTYPE( Uint32, uint32 )
	PANORAMA_DECLARE_DEDUCE_JSTYPE( Int64, int64 )
	PANORAMA_DECLARE_DEDUCE_JSTYPE( Uint64, uint64 )
	PANORAMA_DECLARE_DEDUCE_JSTYPE( Float, float )
	PANORAMA_DECLARE_DEDUCE_JSTYPE( Double, double )
	PANORAMA_DECLARE_DEDUCE_JSTYPE( ConstString, const char * )
	PANORAMA_DECLARE_DEDUCE_JSTYPE( PanoramaSymbol, CPanoramaSymbol )
	PANORAMA_DECLARE_DEDUCE_JSTYPE( RawV8Args, const v8::FunctionCallbackInfo<v8::Value>& )

	inline void JSCheckObjectValidity( const v8::FunctionCallbackInfo<v8::Value>& args )
	{
		// Don't use helper for getting panel, as it will throw exception, which we don't want in just this function
		v8::Local<v8::Object> obj = args.Holder();
		if( obj->InternalFieldCount() != 1 )
		{
			v8::Handle< v8::Boolean > value = v8::Boolean::New( args.GetIsolate(), false );
			args.GetReturnValue().Set( value );
		}

		v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast( obj->GetInternalField( 0 ) );
		void *pPanel = (void*)wrap->Value();
		if( !pPanel )
		{
			v8::Handle< v8::Boolean > value = v8::Boolean::New( args.GetIsolate(), false );
			args.GetReturnValue().Set( value );
		}
		else
		{
			v8::Handle< v8::Boolean > value = v8::Boolean::New( args.GetIsolate(), true );
			args.GetReturnValue().Set( value );
		}
	}

	// Register special IsValid(), which makes the JS obj like a safe handle where you can use this
	// to check if the obj has been deleted in C++ and is gone.  You must still track all objects that are
	// created and set their internal value to 0 so they point at nothing to make this work.
	inline void RegisterJSIsValid()
	{
		v8::Isolate::Scope isolate_scope( GetV8Isolate() );
		v8::HandleScope handle_scope( GetV8Isolate() );

		v8::Handle<v8::ObjectTemplate> objTemplate = UIEngine()->GetCurrentV8ObjectTemplateToSetup();
		objTemplate->Set( v8::String::NewFromUtf8( GetV8Isolate(), "IsValid" ), v8::FunctionTemplate::New( GetV8Isolate(), &JSCheckObjectValidity ) );
	}

	// Register a member to expose to JavaScript as read/write
	template< typename ObjType, typename MemberType >
	void RegisterJSAccessor( const char *jsMemberName, MemberType( ObjType::*pGetFunc )() const, void(ObjType::*pSetFunc)(MemberType), const char *pDesc = NULL );

	// Register a read-only member to expose to JavaScript
	template< typename ObjType, typename MemberType >
	void RegisterJSAccessorReadOnly( const char *jsMemberName, MemberType( ObjType::*pGetFunc )() const, const char *pDesc = NULL );

	template < typename ObjType, typename RetType, typename ...Args>
	void RegisterJSMethod( const char *pchMethodName, RetType( ObjType::*mf )(Args...) const, const char *pDesc = NULL );

	template <typename T>
	void RegisterJSConstantValue( const char *pchJSMemberName, T value, const char *pDesc = NULL );

	template < typename RetType, typename ...Args>
	void RegisterJSGlobalFunction( const char *pchJSFunctionName, RetType(*pFunc)(Args...), bool bTrueGlobal = false, const char *pDesc = NULL );


	//
	// Everything below here is internals, and should be ignored if you aren't adding additional type
	// support inside panorama itself.
	//

	template< typename ObjType> ObjType* GetThisPtrForJSCall( v8::Local<v8::Object> self )
	{
		v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast( self->GetInternalField( 0 ) );
		ObjType *pPanel = (ObjType*)wrap->Value();

		if( pPanel && panorama_is_base_of< IUIPanelClient, ObjType >::value )
		{
			IUIPanel *pUIPanel = (IUIPanel*)(pPanel);
			pPanel = (ObjType*)(pUIPanel->ClientPtr());
		}

		if( !pPanel )
		{
			GetV8Isolate()->ThrowException( v8::String::NewFromUtf8( GetV8Isolate(), "Underlying object is deleted!" ) );
			return NULL;
		}
		return pPanel;
	}


	template < typename T > void GetPtrToCallbackArray( T pGetFunc, v8::Handle<v8::Array> &callbackArray );
	template < typename T > void SetPtrToCallbackArray( T pSetFunc, v8::Handle<v8::Array> &callbackArray );

	struct funcbytes
	{
		void * v1;
		void * v2;
		void * v3;
	};

	template< typename T> union HACKY_FUNC_PTR_CASTER
	{
		T funcPtr;
		funcbytes funcBytes;
	};

	template< typename T> void RestoreFuncPtr( HACKY_FUNC_PTR_CASTER< T > &caster, v8::Local<v8::Array>  &callbackArray, int iOffset )
	{
		Assert( (int)callbackArray->Length() >= iOffset + 3 );
		caster.funcBytes.v1 = v8::Local<v8::External>::Cast( callbackArray->Get( iOffset++ ) )->Value();
		caster.funcBytes.v2 = v8::Local<v8::External>::Cast( callbackArray->Get( iOffset++ ) )->Value();
		caster.funcBytes.v3 = v8::Local<v8::External>::Cast( callbackArray->Get( iOffset++ ) )->Value();
	}

	template <typename T>
	void RegisterJSConstantValue( const char *pchJSMemberName, T value, const char *pDesc )
	{
		v8::Isolate::Scope isolate_scope( GetV8Isolate() );
		v8::HandleScope handle_scope( GetV8Isolate() );

		v8::Handle<v8::ObjectTemplate> objTemplate = UIEngine()->GetCurrentV8ObjectTemplateToSetup();
		v8::Handle<v8::Value> val;

		PanoramaTypeToV8Param( value, &val );

		objTemplate->Set( v8::String::NewFromUtf8( GetV8Isolate(), pchJSMemberName ), val, v8::PropertyAttribute( v8::ReadOnly | v8::DontDelete ) );

		UIEngine()->NewRegisterJSEntry( pchJSMemberName, RegisterJSEntryInfo_t::k_EConstantValue, pDesc, RegisterJSTypeDeducer_t<T>::Type() );
	}


	//-----------------------------------------------------------------------------
	// Purpose: More heavily templated helper for registering accessors of various types.  We'll
	// expose explicit more strongly typed versions, but this avoids duplicating setup code in each.
	//-----------------------------------------------------------------------------
	template <typename JSGetterCallback, typename JSSetterCallback, typename ObjGetMethod, typename ObjSetMethod>
	void RegisterJSAccessorInternal( const char *pchJSMemberName, ObjGetMethod pGetFunc, ObjSetMethod pSetFunc, JSGetterCallback pGetCallback, JSSetterCallback pSetCallback, const char *pDesc = NULL, RegisterJSType_t eDataType = k_ERegisterJSTypeUnknown )
	{
		v8::Isolate::Scope isolate_scope( GetV8Isolate() );
		v8::HandleScope handle_scope( GetV8Isolate() );

		v8::Handle<v8::Array> callbackArray = v8::Array::New( GetV8Isolate(), 6 );
		GetPtrToCallbackArray( pGetFunc, callbackArray );
		SetPtrToCallbackArray( pSetFunc, callbackArray );

		v8::Handle<v8::ObjectTemplate> objTemplate = UIEngine()->GetCurrentV8ObjectTemplateToSetup();
		if( pSetFunc )
			objTemplate->SetAccessor( v8::String::NewFromUtf8( GetV8Isolate(), pchJSMemberName ), pGetCallback,  pSetCallback, callbackArray, v8::DEFAULT, v8::None );
		else
			objTemplate->SetAccessor( v8::String::NewFromUtf8( GetV8Isolate(), pchJSMemberName ), pGetCallback, 0, callbackArray, v8::DEFAULT, v8::ReadOnly );

		UIEngine()->NewRegisterJSEntry( pchJSMemberName, pSetFunc ? RegisterJSEntryInfo_t::k_EAccessor : RegisterJSEntryInfo_t::k_EAccessorReadOnly, pDesc, eDataType );
	}


	template < class ObjType, class T > class CJSPropGetter
	{
	public:

		static void GetProp( v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info )
		{
			v8::Isolate::Scope isolate_scope( GetV8Isolate() );
			v8::HandleScope handle_scope( GetV8Isolate() );
			ObjType *pPanel = GetThisPtrForJSCall<ObjType>( info.Holder() );
			if( !pPanel )
				return;

			v8::Local<v8::Array> callbackArray = v8::Local<v8::Array>::Cast( info.Data() );

			HACKY_FUNC_PTR_CASTER< T( ObjType::* )() const > caster;
			RestoreFuncPtr( caster, callbackArray, 0 );

			v8::Handle< v8::Value > val;
			T nativeval = (pPanel->*caster.funcPtr)();
			PanoramaTypeToV8Param( nativeval, &val );
			info.GetReturnValue().Set( val );
		}

		static void SetProp( v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info )
		{
			v8::Isolate::Scope isolate_scope( GetV8Isolate() );
			v8::HandleScope handle_scope( GetV8Isolate() );
			ObjType *pPanel = GetThisPtrForJSCall<ObjType>( info.Holder() );
			if( !pPanel )
				return;

			v8::Local<v8::Array> callbackArray = v8::Local<v8::Array>::Cast( info.Data() );

			HACKY_FUNC_PTR_CASTER< void (ObjType::*)(T) > caster;
			RestoreFuncPtr( caster, callbackArray, 3 );

			T val;
			V8ParamToPanoramaType( value, &val );
			(pPanel->*caster.funcPtr)(val);

			FreeConvertedParam( val );
		}
	};


	//-----------------------------------------------------------------------------
	// Purpose: Register a float member to expose to JavaScript, can pass NULL for pSetFunc if this is only able to be read not written
	//-----------------------------------------------------------------------------
	template< typename ObjType, typename MemberType >
	void RegisterJSAccessor( const char *jsMemberName, MemberType( ObjType::*pGetFunc )() const, void(ObjType::*pSetFunc)(MemberType), const char *pDesc )
	{
		RegisterJSAccessorInternal( jsMemberName, pGetFunc, pSetFunc, &CJSPropGetter<ObjType, MemberType>::GetProp, &CJSPropGetter<ObjType, MemberType>::SetProp, pDesc, RegisterJSTypeDeducer_t<MemberType>::Type() );
	}

	// Register a read-only member to expose to JavaScript
	template< typename ObjType, typename MemberType >
	void RegisterJSAccessorReadOnly( const char *jsMemberName, MemberType( ObjType::*pGetFunc )() const, const char *pDesc )
	{
		RegisterJSAccessorInternal( jsMemberName, pGetFunc, 0, &CJSPropGetter<ObjType, MemberType>::GetProp, &CJSPropGetter<ObjType, MemberType>::SetProp, pDesc, RegisterJSTypeDeducer_t<MemberType>::Type() );
	}


	//-----------------------------------------------------------------------------
	// Purpose: Convert a v8 argument to native panorama type
	//-----------------------------------------------------------------------------
	template< typename T> bool BConvertV8ArgToPanorama( const v8::Handle<v8::Value> &val, T *pNativeVal )
	{
		v8::TryCatch try_catch;
		V8ParamToPanoramaType( val, pNativeVal );
		if( try_catch.HasCaught() )
		{
			UIEngine()->OutputJSExceptionToConsole( try_catch, UIEngine()->GetPanelForJavaScriptContext( *(GetV8Isolate()->GetCurrentContext()) ) );
			return false;
		}

		return true;
	}

	void JSCheckObjectValidity( const v8::FunctionCallbackInfo<v8::Value>& args );

	//-----------------------------------------------------------------------------
	// Purpose: Helper for passing function pointers into js data callback
	//-----------------------------------------------------------------------------
	template < typename T > void GetPtrToCallbackArray( T pGetFunc, v8::Handle<v8::Array> &callbackArray )
	{
		COMPILE_TIME_ASSERT( sizeof( T ) <= sizeof( funcbytes ) );

		Assert( callbackArray->Length() >= 3 );
		HACKY_FUNC_PTR_CASTER< T > getcaster;
		// single a legit GCC warning when our func pointers are 2 bytes, not 3
		getcaster.funcBytes.v3 = NULL;
		getcaster.funcPtr = pGetFunc;

		callbackArray->Set( 0, v8::External::New( GetV8Isolate(), getcaster.funcBytes.v1 ) );
		callbackArray->Set( 1, v8::External::New( GetV8Isolate(), getcaster.funcBytes.v2 ) );
		callbackArray->Set( 2, v8::External::New( GetV8Isolate(), getcaster.funcBytes.v3 ) );
	}


	//-----------------------------------------------------------------------------
	// Purpose: Helper for passing function pointers into js data callback
	//-----------------------------------------------------------------------------
	template < typename T > void SetPtrToCallbackArray( T pSetFunc, v8::Handle<v8::Array> &callbackArray )
	{
		COMPILE_TIME_ASSERT( sizeof( T ) <= sizeof( funcbytes ) );

		Assert( callbackArray->Length() >= 6 );
		if( pSetFunc )
		{
			HACKY_FUNC_PTR_CASTER< T > setcaster;
			// single a legit GCC warning when our func pointers are 2 bytes, not 3
			setcaster.funcBytes.v3 = NULL;
			setcaster.funcPtr = pSetFunc;

			callbackArray->Set( 3, v8::External::New( GetV8Isolate(), setcaster.funcBytes.v1 ) );
			callbackArray->Set( 4, v8::External::New( GetV8Isolate(), setcaster.funcBytes.v2 ) );
			callbackArray->Set( 5, v8::External::New( GetV8Isolate(), setcaster.funcBytes.v3 ) );
		}
	}


	//-----------------------------------------------------------------------------
	// Purpose: Handler for JS method call callbacks
	//-----------------------------------------------------------------------------

	// two JSMethodCallbackWrappers to handle void callbacks & callbacks with return values
	template< typename RetType, typename ObjType, typename ...Args >
	struct JSMethodCallbackWrapper
	{
		static void Call( ObjType *pObject, RetType( ObjType::*mf )(Args...), const v8::FunctionCallbackInfo<v8::Value>& v8Args, Args... args )
		{
			RetType ret = (pObject->*mf)(panorama_forward< Args >( args )...);

			v8::Handle< v8::Value > value;
			PanoramaTypeToV8Param( ret, &value );

			v8Args.GetReturnValue().Set( value );
		}

	};

	template< typename ObjType, typename ...Args >
	struct JSMethodCallbackWrapper< void, ObjType, Args... >
	{
		static void Call( ObjType *pObject, void(ObjType::*mf)(Args...), const v8::FunctionCallbackInfo<v8::Value>& v8Args, Args... args )
		{
			(pObject->*mf)(panorama_forward< Args >( args )...);
		}
	};

	template< typename RetType, typename ...Args >
	struct JSFunctionCallbackWrapper
	{
		static void Call( RetType( *mf )(Args...), const v8::FunctionCallbackInfo<v8::Value>& v8Args, Args... args )
		{
			RetType ret = mf(panorama_forward< Args >( args )... );

			v8::Handle< v8::Value > value;
			PanoramaTypeToV8Param( ret, &value );

			v8Args.GetReturnValue().Set( value );
		}

	};

	template< typename ...Args >
	struct JSFunctionCallbackWrapper < void, Args... >
	{
		static void Call( void(*mf)(Args...), const v8::FunctionCallbackInfo<v8::Value>& v8Args, Args... args )
		{
			mf(panorama_forward< Args >( args )...);
		}
	};


	// helper to generate a sequences of index to iterate
	template<int...> struct index_tuple {};

	template<int I, typename IndexTuple, typename... Types>
	struct make_indexes_impl;

	template<int I, int... Indexes, typename T, typename ... Types>
	struct make_indexes_impl < I, index_tuple<Indexes...>, T, Types... >
	{
		typedef typename make_indexes_impl<I + 1, index_tuple<Indexes..., I>, Types...>::type type;
	};

	template<int I, int... Indexes>
	struct make_indexes_impl < I, index_tuple<Indexes...> >
	{
		typedef index_tuple<Indexes...> type;
	};

	template<typename ... Types>
	struct make_indexes : make_indexes_impl < 0, index_tuple<>, Types... >
	{
	};


	// Expands values in tuple into a single function call
	template< typename ObjType, typename RetType, typename TupleType, class... Args, int... Indexes >
	void JSMethodCallTuple_Helper( ObjType *pPanel, RetType( ObjType::*mf )(Args...), const v8::FunctionCallbackInfo<v8::Value> &args, index_tuple< Indexes... >, TupleType &tup )
	{
		JSMethodCallbackWrapper< RetType, ObjType, Args... >::Call( pPanel, mf, args, panorama_forward<Args>( STDTR1::get<Indexes>( tup ) )... );
	}

	// Expands values in tuple into a single function call
	template< typename RetType, typename TupleType, class... Args, int... Indexes >
	void JSFunctionCallTuple_Helper( RetType( *pFunc )(Args...), const v8::FunctionCallbackInfo<v8::Value> &args, index_tuple< Indexes... >, TupleType &tup )
	{
		JSFunctionCallbackWrapper< RetType, Args... >::Call( pFunc, args, panorama_forward<Args>( STDTR1::get<Indexes>( tup ) )... );
	}

	// converts all v8 args into a tuple
	template< int Index, typename T >
	struct ConvertV8ArgsToTuple
	{
		static void Call( T &tup, const v8::FunctionCallbackInfo<v8::Value> &args, bool *pbSucceeded )
		{
			bool bSucceeded = BConvertV8ArgToPanorama( args[Index - 1], &STDTR1::get< Index - 1>( tup ) );
			if ( pbSucceeded )
				pbSucceeded[Index - 1] = bSucceeded;

			if ( !bSucceeded )
				return;

			ConvertV8ArgsToTuple<Index - 1, T >::Call( tup, args, pbSucceeded );
		}
	};

	template< typename T >
	struct ConvertV8ArgsToTuple< 0, T >
	{
		static void Call( T &tup, const v8::FunctionCallbackInfo<v8::Value> &args, bool *pbSucceeded )
		{
		}
	};


	// frees all args in tuple
	template< int Index, typename T >
	struct FreeV8ArgsToTuple
	{
		static void Call( T &tup, bool *pbSucceeded )
		{
			if ( pbSucceeded && pbSucceeded[Index - 1] )
				FreeConvertedParam( STDTR1::get< Index - 1 >( tup ) );

			FreeV8ArgsToTuple<Index - 1, T >::Call( tup, pbSucceeded );
		}
	};

	template< typename T >
	struct FreeV8ArgsToTuple < 0, T >
	{
		static void Call( T &tup, bool *pbSucceeded )
		{
		}
	};

	template< typename ObjType, typename RetType, typename ...Args >
	void JSMethodCallTuple( const v8::FunctionCallbackInfo<v8::Value>& args )
	{
		v8::Isolate::Scope isolate_scope( args.GetIsolate() );
		v8::HandleScope handle_scope( args.GetIsolate() );
		v8::Persistent<v8::Context> &perContext = UIEngine()->GetV8GlobalContext();
		v8::Handle<v8::Context> context = v8::Local<v8::Context>::New( GetV8Isolate(), perContext );
		v8::Context::Scope context_scope( context );

		if( args.Length() < sizeof...(Args) )
		{
			v8::String::Utf8Value str( args.Callee()->GetName()->ToString() );
			GetV8Isolate()->ThrowException( v8::String::NewFromUtf8( GetV8Isolate(), CFmtStr1024( "%s requires %d arguments; only %d given.", *str, (int)sizeof...(Args), (int)args.Length() ).String() ) );
			return;
		}

		ObjType *pPanel = GetThisPtrForJSCall<ObjType>( args.Holder() );
		if ( !pPanel )
			return;

		v8::Local<v8::Array> callbackArray = v8::Local<v8::Array>::Cast( args.Data() );

		HACKY_FUNC_PTR_CASTER< RetType( ObjType::* )(Args...) > caster;
		RestoreFuncPtr( caster, callbackArray, 0 );

		bool *pbSucceeded = NULL;
		if ( sizeof...(Args) > 0 )
		{
			pbSucceeded = new bool[sizeof...(Args)];
			V_memset( pbSucceeded, 0, sizeof...(Args)* sizeof( bool ) );
		}

		STDTR1::tuple< Args... > tupleArgs;
		ConvertV8ArgsToTuple< sizeof...(Args), STDTR1::tuple< Args... > >::Call( tupleArgs, args, pbSucceeded );

		bool bSucceeded = true;
		for ( int i = 0; i < sizeof...(Args); i++ )
		{
			if ( !pbSucceeded[i] )
				bSucceeded = false;
		}


		if ( bSucceeded )
			JSMethodCallTuple_Helper( pPanel, caster.funcPtr, args, typename make_indexes<Args...>::type(), tupleArgs );

		FreeV8ArgsToTuple< sizeof...(Args), STDTR1::tuple< Args... > >::Call( tupleArgs, pbSucceeded );
		delete[] pbSucceeded;
	}

	template< typename ObjType >
	void JSMethodCallTupleRaw( const v8::FunctionCallbackInfo<v8::Value>& args )
	{
		v8::Isolate::Scope isolate_scope( args.GetIsolate() );
		v8::HandleScope handle_scope( args.GetIsolate() );
		v8::Persistent<v8::Context> &perContext = UIEngine()->GetV8GlobalContext();
		v8::Handle<v8::Context> context = v8::Local<v8::Context>::New( GetV8Isolate(), perContext );
		v8::Context::Scope context_scope( context );

		ObjType *pPanel = GetThisPtrForJSCall<ObjType>( args.Holder() );
		if ( !pPanel )
			return;

		v8::Local<v8::Array> callbackArray = v8::Local<v8::Array>::Cast( args.Data() );

		HACKY_FUNC_PTR_CASTER< void ( ObjType::* )( const v8::FunctionCallbackInfo<v8::Value>& ) > caster;
		RestoreFuncPtr( caster, callbackArray, 0 );

		(pPanel->*caster.funcPtr)( args );
	}

	template< typename RetType, typename ...Args >
	void JSFunctionCallTuple( const v8::FunctionCallbackInfo<v8::Value>& args )
	{
		v8::Isolate::Scope isolate_scope( args.GetIsolate() );
		v8::HandleScope handle_scope( args.GetIsolate() );
		v8::Persistent<v8::Context> &perContext = UIEngine()->GetV8GlobalContext();
		v8::Handle<v8::Context> context = v8::Local<v8::Context>::New( GetV8Isolate(), perContext );
		v8::Context::Scope context_scope( context );

		if( args.Length() < sizeof...(Args) )
		{
			v8::String::Utf8Value str( args.Callee()->GetName()->ToString() );
			GetV8Isolate()->ThrowException( v8::String::NewFromUtf8( GetV8Isolate(), CFmtStr1024( "%s requires %d arguments; only %d given.", *str, (int)sizeof...(Args), (int)args.Length() ).String() ) );
			return;
		}

		v8::Local<v8::Array> callbackArray = v8::Local<v8::Array>::Cast( args.Data() );

		HACKY_FUNC_PTR_CASTER< RetType( * )(Args...) > caster;
		RestoreFuncPtr( caster, callbackArray, 0 );

		bool *pbSucceeded = NULL;
		if( sizeof...(Args) > 0 )
		{
			pbSucceeded = new bool[sizeof...(Args)];
			V_memset( pbSucceeded, 0, sizeof...(Args)* sizeof( bool ) );
		}

		STDTR1::tuple< Args... > tupleArgs;
		ConvertV8ArgsToTuple< sizeof...(Args), STDTR1::tuple< Args... > >::Call( tupleArgs, args, pbSucceeded );

		bool bSucceeded = true;
		for( int i = 0; i < sizeof...(Args); i++ )
		{
			if( !pbSucceeded[i] )
				bSucceeded = false;
		}

		if( bSucceeded )
			JSFunctionCallTuple_Helper( caster.funcPtr, args, typename make_indexes<Args...>::type(), tupleArgs );

		FreeV8ArgsToTuple< sizeof...(Args), STDTR1::tuple< Args... > >::Call( tupleArgs, pbSucceeded );
		delete[] pbSucceeded;
	}


	// Infer types for all arguments.
	template< uint8 unIndex, typename Type1, typename ...Types >
	struct RegisterJSTypesDeducer
	{
		static void Call( RegisterJSType_t *pTypes, uint8 unMaxIndex )
		{
			Assert( unIndex <= RegisterJSEntryInfo_t::k_unMaxParams );
			if ( unIndex <= RegisterJSEntryInfo_t::k_unMaxParams )
			{
				pTypes[unMaxIndex - unIndex] = RegisterJSTypeDeducer_t<Type1>::Type();
#if defined( SOURCE2_PANORAMA )
				RegisterJSTypesDeducer< unIndex - 1, Types... >::Call( pTypes, unMaxIndex );
#endif
			}
		}
	};

	template< typename Type1 >
	struct RegisterJSTypesDeducer< 0, Type1 >
	{
		static void Call( RegisterJSType_t *pTypes, uint8 unMaxIndex )
		{
			// This is the termination placeholder.
		}
	};


	// two RegisterJSMethods, one for const function pointers, one for mutable
	template < typename ObjType, typename RetType, typename ...Args>
	void RegisterJSMethod( const char *pchMethodName, RetType( ObjType::*mf )(Args...), const char *pDesc = NULL )
	{
		v8::Isolate::Scope isolate_scope( GetV8Isolate() );
		v8::HandleScope handle_scope( GetV8Isolate() );

		v8::Handle<v8::Array> callbackArray = v8::Array::New( GetV8Isolate(), 3 );
		GetPtrToCallbackArray( mf, callbackArray );

		v8::Handle<v8::ObjectTemplate> objTemplate = UIEngine()->GetCurrentV8ObjectTemplateToSetup();
		objTemplate->Set( v8::String::NewFromUtf8( GetV8Isolate(), pchMethodName ), v8::FunctionTemplate::New( GetV8Isolate(), &JSMethodCallTuple<ObjType, RetType, Args...>, callbackArray ) );

		int nEntry = UIEngine()->NewRegisterJSEntry( pchMethodName, RegisterJSEntryInfo_t::k_EMethod, pDesc, RegisterJSTypeDeducer_t<RetType>::Type() );
#if defined( SOURCE2_PANORAMA )
		RegisterJSType_t pParamTypes[RegisterJSEntryInfo_t::k_unMaxParams];
		RegisterJSTypesDeducer< sizeof...(Args), Args..., void >::Call( pParamTypes, sizeof...(Args) );
		UIEngine()->SetRegisterJSEntryParams( nEntry, sizeof...(Args), pParamTypes );
#else
		REFERENCE( nEntry );
#endif
	}

	template < typename ObjType, typename RetType, typename ...Args>
	void RegisterJSMethod( const char *pchMethodName, RetType( ObjType::*mf )(Args...) const, const char *pDesc )
	{
		v8::Isolate::Scope isolate_scope( GetV8Isolate() );
		v8::HandleScope handle_scope( GetV8Isolate() );

		v8::Handle<v8::Array> callbackArray = v8::Array::New( GetV8Isolate(), 3 );
		GetPtrToCallbackArray( mf, callbackArray );

		v8::Handle<v8::ObjectTemplate> objTemplate = UIEngine()->GetCurrentV8ObjectTemplateToSetup();
		objTemplate->Set( v8::String::NewFromUtf8( GetV8Isolate(), pchMethodName ), v8::FunctionTemplate::New( GetV8Isolate(), &JSMethodCallTuple<ObjType, RetType, Args...>, callbackArray ) );

		int nEntry = UIEngine()->NewRegisterJSEntry( pchMethodName, RegisterJSEntryInfo_t::k_EMethod, pDesc, RegisterJSTypeDeducer_t<RetType>::Type() );
#if defined( SOURCE2_PANORAMA )
		RegisterJSType_t pParamTypes[RegisterJSEntryInfo_t::k_unMaxParams];
		RegisterJSTypesDeducer< sizeof...(Args), Args..., void >::Call( pParamTypes, sizeof...(Args) );
		UIEngine()->SetRegisterJSEntryParams( nEntry, sizeof...(Args), pParamTypes );
#else
		REFERENCE( nEntry );
#endif
	}

	template < typename RetType, typename ...Args>
	void RegisterJSGlobalFunction( const char *pchJSFunctionName, RetType( *pFunc )(Args...), bool bTrueGlobal, const char *pDesc )
	{
		v8::Isolate::Scope isolate_scope( GetV8Isolate() );
		v8::HandleScope handle_scope( GetV8Isolate() );
		v8::Persistent<v8::Context> &perContext = UIEngine()->GetV8GlobalContext();
		v8::Handle<v8::Context> context = v8::Local<v8::Context>::New( GetV8Isolate(), perContext );
		v8::Context::Scope context_scope( context );


		v8::Handle<v8::Array> callbackArray = v8::Array::New( GetV8Isolate(), 3 );
		GetPtrToCallbackArray( pFunc, callbackArray );

		v8::Handle< v8::FunctionTemplate > funcTempl = v8::FunctionTemplate::New( GetV8Isolate(), &JSFunctionCallTuple< RetType, Args...>, callbackArray );
		UIEngine()->AddGlobalV8FunctionTemplate( pchJSFunctionName, &funcTempl, bTrueGlobal );

		int nEntry = UIEngine()->NewRegisterJSEntry( pchJSFunctionName, RegisterJSEntryInfo_t::k_EGlobalFunction, pDesc, RegisterJSTypeDeducer_t<RetType>::Type() );
#if defined( SOURCE2_PANORAMA )
		RegisterJSType_t pParamTypes[RegisterJSEntryInfo_t::k_unMaxParams];
		RegisterJSTypesDeducer< sizeof...(Args), Args..., void >::Call( pParamTypes, sizeof...(Args) );
		UIEngine()->SetRegisterJSEntryParams( nEntry, sizeof...(Args), pParamTypes );
#else
		REFERENCE( nEntry );
#endif
	}

	inline void RegisterJSGlobalFunctionRaw( const char *pchJSFunctionName, void (*pCallbackFunc)( const v8::FunctionCallbackInfo< v8::Value > &callbackInfo ), bool bTrueGlobal, const char *pDesc = NULL )
	{
		v8::Isolate::Scope isolate_scope( GetV8Isolate() );
		v8::HandleScope handle_scope( GetV8Isolate() );
		v8::Persistent<v8::Context> &perContext = UIEngine()->GetV8GlobalContext();
		v8::Handle<v8::Context> context = v8::Local<v8::Context>::New( GetV8Isolate(), perContext );
		v8::Context::Scope context_scope( context );

		v8::Handle< v8::FunctionTemplate > funcTempl = v8::FunctionTemplate::New( GetV8Isolate(), pCallbackFunc );
		UIEngine()->AddGlobalV8FunctionTemplate( pchJSFunctionName, &funcTempl, bTrueGlobal );

		int nEntry = UIEngine()->NewRegisterJSEntry( pchJSFunctionName, RegisterJSEntryInfo_t::k_EGlobalFunction, pDesc, k_ERegisterJSTypeVoid );
		RegisterJSType_t pParamTypes[1] = { k_ERegisterJSTypeRawV8Args };
		UIEngine()->SetRegisterJSEntryParams( nEntry, 1, pParamTypes );
	}

	template < typename ObjType >
	inline void RegisterJSMethodRaw( const char *pchMethodName, void ( ObjType::*mf )( const v8::FunctionCallbackInfo< v8::Value > &callbackInfo ), const char *pDesc = NULL )
	{
		v8::Isolate::Scope isolate_scope( GetV8Isolate() );
		v8::HandleScope handle_scope( GetV8Isolate() );

		v8::Handle<v8::Array> callbackArray = v8::Array::New( GetV8Isolate(), 3 );
		GetPtrToCallbackArray( mf, callbackArray );

		v8::Handle<v8::ObjectTemplate> objTemplate = UIEngine()->GetCurrentV8ObjectTemplateToSetup();
		objTemplate->Set( v8::String::NewFromUtf8( GetV8Isolate(), pchMethodName ), v8::FunctionTemplate::New( GetV8Isolate(), &JSMethodCallTupleRaw<ObjType>, callbackArray ) );

		int nEntry = UIEngine()->NewRegisterJSEntry( pchMethodName, RegisterJSEntryInfo_t::k_EMethod, pDesc, k_ERegisterJSTypeVoid );
		RegisterJSType_t pParamTypes[1] = { k_ERegisterJSTypeRawV8Args };
		UIEngine()->SetRegisterJSEntryParams( nEntry, 1, pParamTypes );
	}

	inline v8::Local< v8::String > JSObjectToJSON( v8::Isolate *pIsolate, v8::Local< v8::Value > object )
	{
		v8::Local< v8::Context > context = pIsolate->GetCurrentContext();
		v8::Local< v8::Object > global = context->Global();
		v8::Local< v8::Object > global_JSON = v8::Handle< v8::Object >::Cast( global->Get( v8::String::NewFromUtf8( pIsolate, "JSON" ) ) );
		v8::Local< v8::Function > global_JSON_stringify = v8::Handle< v8::Function >::Cast( global_JSON->Get( v8::String::NewFromUtf8( pIsolate, "stringify" ) ) );

		v8::Local< v8::Value > args[] = { object };
		return v8::Local< v8::String >::Cast( global_JSON_stringify->Call( global_JSON, 1, args ) );
	}
}

#pragma warning(pop)

#if _GNUC
#pragma GCC diagnostic pop
#endif

#endif // _WIN32
