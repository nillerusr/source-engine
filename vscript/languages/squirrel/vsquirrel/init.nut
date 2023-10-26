//===	======= Copyright © 2008, Valve Corporation, All rights reserved. ========
//
// Purpose: Script initially run after squirrel VM is initialized
//
//=============================================================================

//-----------------------------------------------------------------------------
// General
//-----------------------------------------------------------------------------

function printl( text )
{
	return print( text + "\n" );
}

function Msg( text )
{
	return print( text );
}

function Assert( b, msg = null )
{
	if ( b )
		return;
		
	if ( msg != null )
	{
		throw "Assertion failed: " + msg;
	}
	else
	{
		throw "Assertion failed";
	}
}

//-----------------------------------------------------------------------------
// Documentation table
//-----------------------------------------------------------------------------

if ( developer() > 0 )
{
	Documentation <-
	{
		classes = {}
		functions = {}
		instances = {}
	}


	function RetrieveNativeSignature( nativeFunction )
	{
		if ( nativeFunction in NativeFunctionSignatures )
		{
			return NativeFunctionSignatures[nativeFunction]
		}
		return "<unnamed>"
	}
	
	function RegisterFunctionDocumentation( func, name, signature, description )
	{
		if ( description.len() )
		{
			local b = ( description[0] == '#' );
			if ( description[0] == '#' )
			{
				local colon = description.find( ":" );
				if ( colon == null )
				{
					colon = description.len();
				}
				local alias = description.slice( 1, colon );
				description = description.slice( colon + 1 );
				name = alias;
				signature = "#";
			}
		}
		Documentation.functions[name] <- [ signature, description ]
	}

	function Document( symbolOrTable, itemIfSymbol = null, descriptionIfSymbol = null )
	{
		if ( typeof( symbolOrTable ) == "table" )
		{
			foreach( symbol, itemDescription in symbolOrTable )
			{
				Assert( typeof(symbol) == "string" )
				
				Document( symbol, itemDescription[0], itemDescription[1] );
			}
		}
		else
		{
			printl( symbolOrTable + ":" + itemIfSymbol.tostring() + "/" + descriptionIfSymbol );
		}
	}
	
	function PrintHelp( string = "*", exact = false )
	{
		local matches = []
		
		if ( string == "*" || !exact )
		{
			foreach( name, documentation in Documentation.functions )
			{
				if ( string != "*" && name.tolower().find( string.tolower() ) == null )
				{
					continue;
				}
				
				matches.append( name ); 
			}
		} 
		else if ( exact )
		{
			if ( string in Documentation.functions )
				matches.append( string )
		}
		
		if ( matches.len() == 0 )
		{
			printl( "Symbol " + string + " not found" );
			return;
		}
		
		matches.sort();
		
		foreach( name in matches )
		{
			local result = name;
			local documentation = Documentation.functions[name];
			
			printl( "Function:    " + name );
			local signature;
			if ( documentation[0] != "#" )
			{
				signature = documentation[0];
			}
			else
			{
				signature = GetFunctionSignature( this[name], name );
			}
			
			printl( "Signature:   " + signature );
			if ( documentation[1].len() )
				printl( "Description: " + documentation[1] );
			print( "\n" ); 
		}
	}
}
else
{
	function RetrieveNativeSignature( nativeFunction ) { return "<unnamed>"; }
	function Document( symbolOrTable, itemIfSymbol = null, descriptionIfSymbol = null ) {}
	function PrintHelp( string = "*", exact = false ) {}
}

//-----------------------------------------------------------------------------
// VSquirrel support functions
//-----------------------------------------------------------------------------

function VSquirrel_OnCreateScope( name, outer )
{
	local result;
	if ( !(name in outer) )
	{
		result = outer[name] <- { __vname=name, __vrefs = 1 };
		delegate outer : result;
	}
	else
	{
		result = outer[name];
		result.__vrefs += 1;
	}
	return result;
}

function VSquirrel_OnReleaseScope( scope )
{
	scope.__vrefs -= 1;
	if ( scope.__vrefs < 0 )
	{
		throw "Bad reference counting on scope " + scope.__vname;
	}
	else if ( scope.__vrefs == 0 )
	{
		delete scope.parent[scope.__vname];
		scope.__vname = null;
		delegate null : scope;
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CCallChainer
{
	constructor( prefixString, scopeForThis = null )
	{
		prefix = prefixString;
		if ( scopeForThis != null )
			scope = scopeForThis;
		else
			scope = ::getroottable();
		chains = {};
		
		// Expose a bound global function to dispatch to this object
		scope[ "Dispatch" + prefixString ] <- Call.bindenv( this );
	}
	
	function PostScriptExecute() 
	{
		foreach( key, value in scope )
		{
			if ( typeof( value ) == "function" ) 
			{
				if ( key.find( prefix ) == 0 )
				{
					key = key.slice( prefix.len() );
					
					if ( !(key in chains) )
					{
						//::print( "Creating new call chain " + key + "\n");
						chains[key] <- [];
					}
					
					local chain = chains[key];
					
					if ( !chain.len() || chain.top() != value )
					{
						chain.push( value );
						//::print( "Added " + value + " to call chain " + key + "\n" );
					}
				}
			}
		}
	}
	
	function Call( event, ... )
	{
		if ( event in chains )
		{
			local chain = chains[event];
			if ( chain.len() )
			{
				local i;
				local args = [];
				if ( vargc > 0 )
				{
					args.push( scope );
					for ( i = 0; i < vargc; i++ )
					{
						args.push( vargv[i] );
					}
				}
				for ( i = chain.len() - 1; i >= 0; i -= 1 )
				{
					local func = chain[i];
					local result;
					if ( !args.len() )
					{
						result = func();
					}
					else
					{
						result = func.acall( args ); 
					}
					if ( result != null && !result )
						return false;
				}
			}
		}
		
		return true;
	}
	
	scope = null;
	prefix = null;
	chains = null;
};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CSimpleCallChainer
{
	constructor( prefixString, scopeForThis = null, exactNameMatch = false )
	{
		prefix = prefixString;
		if ( scopeForThis != null )
			scope = scopeForThis;
		else
			scope = ::getroottable();
		chain = [];
		
		// Expose a bound global function to dispatch to this object
		scope[ "Dispatch" + prefixString ] <- Call.bindenv( this );
		
		exactMatch = exactNameMatch
	}
	
	function PostScriptExecute() 
	{
		foreach( key, value in scope )
		{
			if ( typeof( value ) == "function" ) 
			{
				local foundMatch = false;
				if ( exactMatch )
				{
					foundMatch = ( prefix == key );
				}
				else
				{
					foundMatch = ( key.find( prefix ) == 0 )
				}
						
				if ( foundMatch )
				{
					if ( !exactMatch )
						key = key.slice( prefix.len() );
					
					if ( !(chain) )
					{
						//::print( "Creating new call simple chain\n");
						chain <- [];
					}
					
					if ( !chain.len() || chain != value )
					{
						chain.push( value );
						//::print( "Added " + value + " to call chain.\n" );
					}
				}
			}
		}
	}
	
	function Call( ... )
	{
		if ( chain.len() )
		{
			local i;
			local args = [];
			if ( vargc > 0 )
			{
				args.push( scope );
				for ( i = 0; i < vargc; i++ )
				{
					args.push( vargv[i] );
				}
			}
			for ( i = chain.len() - 1; i >= 0; i -= 1 )
			{
				local func = chain[i];
				local result;
				if ( !args.len() )
				{
					result = func.pcall( scope );
				}
				else
				{
					result = func.pacall( scope, args ); 
				}
				if ( result != null && !result )
					return false;
			}
		}
		
		return true;
	}
	
	exactMatch = false
	scope = null;
	prefix = null;
	chain = null;
};

//-----------------------------------------------------------------------------
// Late binding: allows a table to refer to parts of itself, it's children,
// it's owner, and then have the references fixed up after it's fully parsed
//
// Usage:
//    lateBinder <- LateBinder();
//    lateBinder.Begin( this );
//    
//    Test1 <-
//    {   
// 	   Foo=1
//    }   
//    
//    Test2 <-
//    {   
// 	   FooFoo = "I'm foo foo"
// 	   BarBar="@Test1.Foo"
// 	   SubTable = { boo=[bah, "@Test2.FooFoo", "@Test1.Foo"], booboo2={one=bah, two="@Test2.FooFoo", three="@Test1.Foo"} }
// 	   booboo=[bah, "@Test2.FooFoo", "@Test1.Foo"]
// 	   booboo2={one=bah, two="@Test2.FooFoo", three="@Test1.Foo"}
// 	   bah=wha
//    }   
//    
//    lateBinder.End();
//    delete lateBinder;
//
// When End() is called, all of the unresolved symbols in the tables and arrays will be resolved,
// any left unresolved will become a string prepended with '~', which later code can deal with
//-----------------------------------------------------------------------------

class LateBinder
{
	// public:
	function Begin( target, log = false )
	{
		m_log = log;
		
		HookRootMetamethod( "_get", function( key ) { return "^" + key; } );
		HookRootMetamethod( "_newslot", function( key, value ) { if ( typeof value == "table" ) { m_fixupSet.push( [ key, value ] ); this.rawset( key, value ); };  }.bindenv(this) );
		m_targetTable = target;
		
		Log( "Begin late bind on table " + m_targetTable );
	}
	
	function End()
	{
		UnhookRootMetamethod( "_get" );
		UnhookRootMetamethod( "_newslot" );

		Log( "End late bind on table " + m_targetTable );
		
		foreach( subTablePair in m_fixupSet )
		{
			EstablishDelegation( m_targetTable, subTablePair[1] );
		}

		Log( "Begin resolution... " )
		m_logIndent++;
		
		local found = true;
		
		while ( found )
		{
			foreach( subTablePair in m_fixupSet )
			{
				Log( subTablePair[0] + " = " );
				Log( "{" );
				if ( !Resolve( subTablePair[1], subTablePair[1], false ) )
				{
					found = false;
				}
				Log( "}" );
			}
		}
			
		m_logIndent--;
		
		foreach( subTablePair in m_fixupSet )
		{
			RemoveDelegation( subTablePair[1] );
		}
		
		Log( "...end resolution" );
	}
		
	// private:
	function HookRootMetamethod( name, value )
	{
		local saved = null;
		local roottable = getroottable();
		if ( name in roottable )
		{
			saved = roottable[name];
		}
		roottable[name] <- value;
		roottable["__saved" + name] <- saved;
	}

	function UnhookRootMetamethod( name )
	{
		local saveSlot = "__saved" + name;
		local roottable = getroottable();
		local saved = roottable[saveSlot];
		if ( saved != null )
		{
			roottable[name] = saved;
		}
		else
		{
			delete roottable[name];
		}
		delete roottable[saveSlot];
	}

	function EstablishDelegation( parentTable, childTable )
	{
		delegate parentTable : childTable;
		
		foreach( key, value in childTable )
		{
			local type = typeof value;
			if ( type == "table" )
			{
				EstablishDelegation( childTable, value );
			}
		}
	}
	
	function RemoveDelegation( childTable )
	{
		delegate null : childTable;
		
		foreach( key, value in childTable )
		{
			local type = typeof value;
			if ( type == "table" )
			{
				RemoveDelegation( value );
			}
		}
	}

	function Resolve( lookupTable, subTableOrArray, throwException = false )
	{
		m_logIndent++;
		local found = false;
	
		foreach( key, value in subTableOrArray )
		{
			local type = typeof value;
			if ( type == "string" )
			{
				if ( value.len() )
				{
					local unresolvedId = null;
					local controlChar = value[0]
					if ( controlChar == '^' )
					{
						found = true;
						value = value.slice( 1 );
						if ( value in lookupTable )
						{
							subTableOrArray[key] = lookupTable[value];
							Log( key + " = " + lookupTable[value] + " <-- " + value );
						}
						else
						{
							subTableOrArray[key] = "~" + value;
							unresolvedId = value;
							Log( key + " = \"" + "~" + value + "\" (unresolved)" );
						}
					}
					else if ( controlChar == '@' )
					{
						found = true;
						local identifiers = [];
						local iLast = 1;
						local iNext;
						while ( ( iNext = value.find( ".", iLast ) ) != null )
						{
							identifiers.push( value.slice( iLast, iNext ) );
							iLast = iNext + 1;
						}
						identifiers.push( value.slice( iLast ) );
						
						local depthSuccess = 0;
						local result = lookupTable;
						foreach( identifier in identifiers )
						{
							if ( identifier in result )
							{
								depthSuccess++;
								result = result[identifier];
							}
							else
							{
								break;
							}
						}
						if ( depthSuccess == identifiers.len() )
						{
							subTableOrArray[key] = result;
							Log( key + " = " + result + " <-- " + value );
						}
						else
						{
							subTableOrArray[key] = "~" + value.slice( 1 );
							unresolvedId = value;
							Log( key + " = \"" + "~" + value + "\" (unresolved)" );
						}
					}
					
					if ( unresolvedId != null )
					{
						if ( throwException )
						{
							local exception = "Unresolved symbol: " + bind + " in ";
							foreach ( entry in m_bindNamesStack )
							{
								exception += entry;
								exception += "."
							}
							exception += unresolvedId;
							
							throw exception; 
						}
					}
				}
			}
		}

		foreach( key, value in subTableOrArray )
		{
			local type = typeof value;
			local isTable = ( type == "table" );
			local isArray = ( type == "array" )
			if ( isTable || isArray )
			{
				Log( key + " =" );
				Log( isTable ? "{" : "[" );
				
				m_bindNamesStack.push( key );
				if ( Resolve( ( isTable ) ? value : lookupTable, value, throwException ) )
				{
					found = true;
				}
				m_bindNamesStack.pop();
				
				Log( isTable ? "}" : "]" );
			}
		}
		m_logIndent--;
		return found;
	}
	
	function Log( string )
	{
		if ( m_log )
		{
			for ( local i = 0; i < m_logIndent; i++ )
			{
				print( "  " );
			}
			
			printl( string );
		}
	}

	m_targetTable = null;
	m_fixupSet = [];
	m_bindNamesStack = [];
	m_log = false;
	m_logIndent = 0;
}


