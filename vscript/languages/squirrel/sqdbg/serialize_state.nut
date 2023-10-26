/*
	see copyright notice in sqrdbg.h
*/
local currentscope;
if ( ::getroottable().parent )
{
	currentscope = ::getroottable();
	::setroottable( ::getroottable().parent );
}
try {
	
local objs_reg = { maxid=0 ,refs={} }

complex_types <- {
	["table"] = null,
	["array"] = null,
	["class"] = null,
	["instance"] = null,
	["weakref"] = null,
}

function build_refs(t):(objs_reg)
{
	if(t == ::getroottable())
		return;
	local otype = ::type(t);
	if(otype in complex_types)
	{
		if(!(t in objs_reg.refs)) {
			objs_reg.refs[t] <- objs_reg.maxid++;
		
		    iterateobject(t,function(o,i,val):(objs_reg)
		    {
			    build_refs(val);
			    build_refs(i);
		    })
		}
		
	}
}

function getvalue(v):(objs_reg)
{
	switch(::type(v))
	{
		case "table":
		case "array":
		case "class":
		case "instance":
			return objs_reg.refs[v].tostring();
		case "integer":
		case "float":
		    return v;
		case "bool":
		    return v.tostring();
		case "string":
			return v;
		case "null":
		    return "null";
		default:
			
			return pack_type(::type(v));
	}
}

local packed_types={
	["null"]="n",
	["string"]="s",
	["integer"]="i",
	["float"]="f",
	["userdata"]="u",
	["function"]="fn",
	["table"]="t",
	["array"]="a",
	["generator"]="g",
	["thread"]="h",
	["instance"]="x", 
	["class"]="y",  
	["bool"]="b",
	["weakref"]="w"  
}

function pack_type(type):(packed_types)
{
	if(type in packed_types)return packed_types[type]
	return type
} 

function iterateobject(obj,func)
{
	local ty = ::type(obj);
	if(ty == "instance") {
		try { //TRY TO USE _nexti
		    foreach(idx,val in obj)
		    {
				func(obj,idx,val);
		    }
		}
		catch(e) {
		   foreach(idx,val in obj.getclass())
		   {
			func(obj,idx,obj[idx]);
		   }
		}
	}
	else if(ty == "weakref") {
		func(obj,"@ref",obj.ref());
	}
	else {
		foreach(idx,val in obj)
		{
		    func(obj,idx,val);
		}
	}
			
}

function build_tree():(objs_reg)
{
	foreach(i,o in objs_reg.refs)
	{
		beginelement("o");
		attribute("type",(i==::getroottable()?"r":pack_type(::type(i))));
		local _typeof = typeof i;
		if(_typeof != ::type(i)) {
			attribute("typeof",_typeof);
		}
		attribute("ref",o.tostring());
		if(i != ::getroottable()){
			iterateobject(i,function (obj,idx,val) {
				if(::type(val) == "function")
					return;
					
				if ( ::type(idx) == "string" && idx.find( "__" ) == 0 )
					return;

				beginelement("e");	
					emitvalue("kt","kv",idx);
					emitvalue("vt","v",obj[idx]);
				endelement("e");	

			})
		}
		endelement("o");
	}
}

function evaluate_watch(locals,id,expression)
{
	local func_src="return function ("
	local params=[];
	
	params.append(locals["this"])
	local first=1;
	foreach(i,v in locals){
		if(i!="this" && i[0] != '@'){ //foreach iterators start with @
			if(!first){
				func_src=func_src+","
				
			}
			first=null
			params.append(v)
			func_src=func_src+i
		}
	}
	func_src=func_src+"){\n"
	func_src=func_src+"return ("+expression+")\n}"
	
	try {
		local func=::compilestring(func_src);
		return {status="ok" , val=func().acall(params)};
	}
	catch(e)
	{
		
		return {status="error"}
	}
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
function emitvalue(type_attrib,value_attrib,val)
{
	attribute(type_attrib,pack_type(::type(val)));
	attribute(value_attrib,getvalue(val).tostring());
}

local stack=[]
local level=3;
local si;

	//ENUMERATE THE STACK WATCHES
	while(si=::getstackinfos(level))
	{
		stack.append(si);
		level++;
	}

	//EVALUATE ALL WATCHES
	objs_reg.refs[::getroottable()] <- objs_reg.maxid++;
	foreach(i,val in stack)
	{
		if(val.src!="NATIVE") {
			if("watches" in this) {
				val.watches <- {}
				foreach(i,watch in watches)
				{
					if(val.src!="NATIVE"){
						val.watches[i] <- evaluate_watch(val.locals,i,watch);
						if(val.watches[i].status!="error")
							build_refs(val.watches[i].val);
					}
					else{
						val.watches[i] <- {status="error"}
					}
					val.watches[i].exp <- watch;
				}
				
			}
		}
		foreach(i,l in val.locals)
			build_refs(l);
	}


	beginelement("objs");
	build_tree();
	endelement("objs");

	beginelement("calls");

	foreach(i,val in stack)
	{

		beginelement("call");
		attribute("fnc",val.func);
		attribute("src",val.src);
		attribute("line",val.line.tostring());
		foreach(i,v in val.locals)
		{
			beginelement("l");
				attribute("name",getvalue(i).tostring());
				emitvalue("type","val",v);
			endelement("l");
		}
		if("watches" in val) {
			foreach(i,v in val.watches)
			{
				beginelement("w");
					attribute("id",i.tostring());
					attribute("exp",v.exp);
					attribute("status",v.status);
					if(v.status!="error") {
						emitvalue("type","val",v.val);
					}
				endelement("w");
			}
		}
		endelement("call");
		 
	}
	endelement("calls");


	objs_reg = null;
	stack = null;
	
	if("collectgarbage" in ::getroottable()) ::collectgarbage();
}catch(e)
{
	::print("ERROR"+e+"\n");
}

if ( currentscope )
{
	::setroottable( currentscope );
}
