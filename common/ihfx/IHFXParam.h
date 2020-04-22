#ifndef IHFXPARAM_H_
#define IHFXPARAM_H_
#include ".\\HFXConfig.h"
 
class IHapticEffect; 
typedef IHapticEffect IHFXEffect; 
#ifdef HFX_64BIT
#define HFX_NULL_HANDLE ((IHFXEffect*)(unsigned long)0x000000000000001) 
#else
#define HFX_NULL_HANDLE ((IHFXEffect*)(unsigned long)0x00000001) 
#endif
static IHFXEffect *hfxNoHandle=HFX_NULL_HANDLE; 
 
typedef unsigned __int32 HFXParamID; 
typedef unsigned __int32 HFXEffectID; 
 
// if a invalid char is added it will show up as '`'
// if a '?' it will be the same as a '_'
// any upper case chars will be converted to lowercase.
 
 
#define HFX_PARAM_ENCODE_CHAR_LOWER(lowercase_alpha_or_underscore) \
	(lowercase_alpha_or_underscore ? ( ( lowercase_alpha_or_underscore < ('~'+1) ) ? \
	( (lowercase_alpha_or_underscore)-('^') ) : 2 ) : 0) 
 
#define HFX_PARAM_ENCODE_CHAR_UPPER(upper_alpha) \
	(upper_alpha!=0 ? ( (upper_alpha>'>') ? \
		((upper_alpha)-('>')) : ( ((upper_alpha>'/')&&(upper_alpha<':')) ? \
			((upper_alpha + 32)-('>')) : (2) ) \
			) : 0) 
 
#define HFX_PARAM_ENCODE_CHAR(alpha_or_underscore) \
	(alpha_or_underscore ?(alpha_or_underscore < '_') ? \
	HFX_PARAM_ENCODE_CHAR_UPPER(alpha_or_underscore) : \
	HFX_PARAM_ENCODE_CHAR_LOWER(alpha_or_underscore):0 ) 
 
#define HFX_PARAM_PACK_CHAR(char, index) \
	( char ? ((char) << (index * 5)) : 0 ) 
 
#define HFX_PARAM_ENCODE_STRING_CHAR(char) ( (char < '_') ? (HFX_PARAM_ENCODE_CHAR_UPPER(char)) : (HFX_PARAM_ENCODE_CHAR_LOWER(char)) ) 
 
#define HFX_PARAM_ENCODE_AND_PACK_CHAR(char, index) \
	HFX_PARAM_PACK_CHAR(HFX_PARAM_ENCODE_CHAR(char), index) 
 
 
#define TC unsigned int
 
#define HFX_PARAM_ENCODE_STRING(ConstCharPtr7) \
	( ConstCharPtr7 ?  \
		(ConstCharPtr7[0]!=0 ? \
			((HFX_PARAM_ENCODE_STRING_CHAR((ConstCharPtr7)[0])) << 0) | \
				(ConstCharPtr7[1]!=0 ? \
				((HFX_PARAM_ENCODE_STRING_CHAR((ConstCharPtr7)[1])) << 5) |  \
					(ConstCharPtr7[2]!=0 ? \
					((HFX_PARAM_ENCODE_STRING_CHAR((ConstCharPtr7)[2])) << 10) | \
						(ConstCharPtr7[3]!=0 ? \
						((HFX_PARAM_ENCODE_STRING_CHAR((ConstCharPtr7)[3])) << 15) | \
							(ConstCharPtr7[4]!=0 ? \
							((HFX_PARAM_ENCODE_STRING_CHAR((ConstCharPtr7)[4])) << 20) | \
								(ConstCharPtr7[5]!=0 ? \
								((HFX_PARAM_ENCODE_STRING_CHAR((ConstCharPtr7)[5])) << 25) \
								: (unsigned)0) \
							: (unsigned)0) \
						: (unsigned)0) \
					: (unsigned)0) \
				: (unsigned)0) \
			: (unsigned)0) \
		: (unsigned)0) 
 
#define HFX_PARAM_ENCODE_6(c1,c2,c3,c4,c5,c6) \
	((HFXParamID)\
	( HFX_PARAM_ENCODE_AND_PACK_CHAR(c1,0) | HFX_PARAM_ENCODE_AND_PACK_CHAR(c2,1) | HFX_PARAM_ENCODE_AND_PACK_CHAR(c3,2) \
	| HFX_PARAM_ENCODE_AND_PACK_CHAR(c4,3) | HFX_PARAM_ENCODE_AND_PACK_CHAR(c5,4) | HFX_PARAM_ENCODE_AND_PACK_CHAR(c6,5) )) 
 
#define HFX_PARAM_ENCODE_5(c1,c2,c3,c4,c5) \
	HFX_PARAM_ENCODE_6(c1,c2,c3,c4,c5,0) 
 
#define HFX_PARAM_ENCODE_4(c1,c2,c3,c4) \
	HFX_PARAM_ENCODE_6(c1,c2,c3,c4,0,0) 
 
#define HFX_PARAM_ENCODE_3(c1,c2,c3) \
	HFX_PARAM_ENCODE_6(c1,c2,c3,0,0,0) 
 
#define HFX_PARAM_ENCODE_2(c1,c2) \
	HFX_PARAM_ENCODE_6(c1,c2,0,0,0,0) 
 
#define HFX_PARAM_ENCODE_1(c1) \
	HFX_PARAM_ENCODE_6(c1,0,0,0,0,0) 
 
#define HFX_PARAM_DECODE_TO(char7, hfxparamid) \
	(char7)[0] = HFX_CHAR_ID(hfxparamid, 0); \
	(char7)[1] = (char7)[0] ? HFX_CHAR_ID(hfxparamid, 1) : 0; \
	(char7)[2] = (char7)[1] ? HFX_CHAR_ID(hfxparamid, 2) : 0; \
	(char7)[3] = (char7)[2] ? HFX_CHAR_ID(hfxparamid, 3) : 0; \
	(char7)[4] = (char7)[3] ? HFX_CHAR_ID(hfxparamid, 4) : 0; \
	(char7)[5] = (char7)[4] ? HFX_CHAR_ID(hfxparamid, 5) : 0; \
	(char7)[6] = 0  
 
HFX_INLINE HFXParamID HFX_PARAM_ENCODE_VA(char c1, char c2=0, char c3=0, char c4=0, char c5=0, char c6=0) 
{
	return HFX_PARAM_ENCODE_6(c1,c2,c3,c4,c5,c6); 
}
 
#if HFX_MSVC > 7
//max of 6!
#define HFX_PARAM_ENCODE(...) HFX_PARAM_ENCODE_VA(__VA_ARGS__) 
#else
#define HFX_PARAM_ENCODE HFX_PARAM_ENCODE_VA
#endif
 
 
#define HFX_PARAM_ENCODE_4(c1,c2,c3,c4) HFX_PARAM_ENCODE_6(c1,c2,c3,c4,0,0) 
 
#define HFX_5BIT_MASK ((1<<0) | (1<<1) | (1<<2) | (1<<3) | (1<<4)) 
 
#define HFX_MASK_ID(paramid, index)((paramid>>(index*5)) & (HFX_5BIT_MASK)) 
 
#define HFX_CHAR_ID(paramid, index) \
	(HFX_MASK_ID(paramid,index)?HFX_MASK_ID(paramid,index)+'^':0) 
 
// usage : char hfxParamText[] = HFX_PARAMETER_DECODE(hfxParamID);
#define HFX_PARAM_DECODE(hfxparam) { \
	HFX_CHAR_ID(hfxparam,0), \
	HFX_CHAR_ID(hfxparam,1), \
	HFX_CHAR_ID(hfxparam,2), \
	HFX_CHAR_ID(hfxparam,3), \
	HFX_CHAR_ID(hfxparam,4), \
	HFX_CHAR_ID(hfxparam,5),0}
 
 
enum HFXParamType
{
	HFXPARAM_NULL			= 0, 
	HFXPARAM_INT			= 'i', 
	HFXPARAM_FLOAT			= 'f', 
	HFXPARAM_DOUBLE			= 'd', 
	HFXPARAM_BOOL			= 'b', 
	HFXPARAM_INT_ARRAY		= 'I', 
	HFXPARAM_FLOAT_ARRAY	= 'F', 
	HFXPARAM_DOUBLE_ARRAY	= 'D', 
	HFXPARAM_BOOL_ARRAY		= 'B', 
}; 
 
typedef unsigned char HFXPARAM; 
 
enum HFXArray
{
	HFXARRAY_NOTARRAY=0, 
	HFXARRAY_COPY=1,//
	HFXARRAY_INDEX=2, 
}; 
typedef unsigned char HFXARRAY; 
typedef void *HFXVarValue; 
struct HFX_PURE_INTERFACE IHapticEffectParamGroup
{
 
#ifndef HFX_NO_PARAMETER_CLASS_MACROS
// declare this inside user implementated parametergroup class definitions.
#define HFX_PARAMETER_GROUP(classname, baseclass, defaultEffect) \
	typedef classname ParameterGroup; \
	typedef baseclass SuperGroup; \
	typedef defaultEffect EffectClass; \
	using SuperGroup::SetVar; \
	using SuperGroup::GetVar; \
	using SuperGroup::SetVarFloat; \
	using SuperGroup::SetVarInt; \
	using SuperGroup::SetVarDouble; \
	using SuperGroup::GetVarBool; \
	using SuperGroup::GetVarFloat; \
	using SuperGroup::GetVarInt; \
	using SuperGroup::GetVarDouble; 
 
#define HFX_PARAMETER_ID(name, type) \
	static const HFXParamID ID_##name; \
	static const char * const ID_##name##_String; 
// declare this inside user implementated parametergroup class definitions
// for each variable you want to be accessible through
// parameter filetypes or set/get functions. These macros will make the variable
// you specify.
// specify a seperate membername for readability.
// REMEMBER: name should be no longer than 6 letters, membername can be as long as you want.
#define HFX_PARAMETER_FLOAT(name, membername) \
	HFX_PARAMETER_ID(float_##name); \
	union{float name; float m_fl##name; float membername;}; \
	static const float ID_float_##name##_Default; 
 
#define HFX_PARAMETER_DOUBLE(name, membername) \
	HFX_PARAMETER_ID(double_##name); \
	union{double name; double m_dbl##name; double membername;}; \
	static const bool ID_double_##name##_Default
 
#define HFX_PARAMETER_BOOL(name, membername) \
	HFX_PARAMETER_ID(bool_##name); \
	union{bool name:1; bool m_b##name:1; bool membername:1;}; \
	static const bool ID_bool_##name##_Default; 
 
#define HFX_PARAMETER_INT(name, membername) \
	HFX_PARAMETER_ID(int_##name); \
	union{int name; int m_i##name; int membername;}; \
	static const int ID_int_##name##_Default; 
 
#define HFX_PARAMETER_FLOAT_ARRAY(size, name, membername) \
	HFX_PARAMETER_ID(float##size##_##name); \
	union{float name; float m_fl##size##name; float membername;}; \
	static const float ID_float##size##_##name##_Default[size]; 
 
#define HFX_PARAMETER_DOUBLE_ARRAY(size, name, membername) \
	HFX_PARAMETER_ID(double##size##_##name); \
	union{double name[size]; double m_dbl##size##name[size]; double membername[size];}; \
	static const bool ID_double##size##_##name##_Default[size]; 
 
#define HFX_PARAMETER_BOOL_ARRAY(size, name, membername) \
	HFX_PARAMETER_ID(bool##size##_##name); \
	union{bool name[size]; bool m_b##size##name[size]; bool membername[size];}; \
	static const bool ID_bool##size##_##name##_Default[size]; 
 
#define HFX_PARAMETER_INT_ARRAY(size, name, membername) \
	HFX_PARAMETER_ID(int##size##_##name); \
	union{int name[size]; int m_i##name[size]; int membername[size];}; \
	static const int ID_int##size##_##name##_Default; 
#endif
	//virtual bool RunSetCommand( char *cmd ) = 0;
	//virtual HFXEffectID StandardEffectName() const = 0;
 
	// ACTUAL OVERRIDES!
	virtual bool SetVar(const HFXParamID id, void *value, HFXParamType t,  
		HFXARRAY settype=HFXARRAY_NOTARRAY, unsigned int indexORcount=0, bool allowMake=false) = 0; 
 
	virtual bool GetVar(const HFXParamID id, void *value, HFXParamType t, 
		HFXARRAY gettype=HFXARRAY_NOTARRAY, unsigned int indexORcount=0 )const = 0; 
 
#ifndef HFX_NO_OVERLOADED_PARAMETER
 
	HFX_INLINE bool SetVarByName(const char *name, HFXVarValue value, HFXParamType t,  
		bool allowMake ) 
		{return SetVar(HFX_PARAM_ENCODE_STRING(name), value, t, HFXARRAY_NOTARRAY, 0, allowMake);}
 
	HFX_INLINE bool SetVar(const HFXParamID name, HFXVarValue value, HFXParamType t,  
		bool allowMake ) 
		{return SetVar(name, value, t, HFXARRAY_NOTARRAY, 0, allowMake);}
 
	HFX_INLINE bool SetVarByName(const char *name, HFXVarValue value, HFXParamType t,  
		HFXARRAY settype, unsigned int indexORcount, bool allowMake ) 
		{return SetVar(HFX_PARAM_ENCODE_STRING(name), value, t, settype, indexORcount, allowMake);}
 
	HFX_INLINE bool SetVarByName(const char *name, const double value, bool allowMake=false) 
		{return SetVarByName(name, (HFXVarValue)&value, HFXPARAM_DOUBLE, allowMake);}
	HFX_INLINE bool SetVar(const HFXParamID name, const double value, bool allowMake=false) 
		{return SetVar(name, (HFXVarValue)&value, HFXPARAM_DOUBLE, allowMake);}
	HFX_INLINE bool SetVarByName(const char *name, const bool value, bool allowMake=false) 
		{return SetVarByName(name, (HFXVarValue)&value, HFXPARAM_BOOL, allowMake);}
	HFX_INLINE bool SetVar(const HFXParamID name, const bool value, bool allowMake=false) 
		{return SetVar(name, (HFXVarValue)&value, HFXPARAM_BOOL, allowMake);}
	HFX_INLINE bool SetVarByName(const char *name, const float value, bool allowMake=false) 
		{return SetVarByName(name, (HFXVarValue)&value, HFXPARAM_FLOAT, allowMake);}
	HFX_INLINE bool SetVar(const HFXParamID name, const float value, bool allowMake=false) 
		{return SetVar(name, (HFXVarValue)&value, HFXPARAM_FLOAT, allowMake);}
	HFX_INLINE bool SetVarByName(const char *name, const int value, bool allowMake=false) 
		{return SetVarByName(name, (HFXVarValue)&value, HFXPARAM_INT, allowMake);}
	HFX_INLINE bool SetVar(const HFXParamID name, const int value, bool allowMake=false) 
		{return SetVar(name, (HFXVarValue)&value, HFXPARAM_INT, allowMake);}
 
	HFX_INLINE bool SetVarArrayByName(const char *name, unsigned int arraySize, const double *value, bool allowMake=false) 
		{return SetVarByName(name, (HFXVarValue)&value, HFXPARAM_DOUBLE_ARRAY, HFXARRAY_COPY, arraySize, allowMake);}
	HFX_INLINE bool SetVarArray(const HFXParamID name, unsigned int arraySize, const double *value, bool allowMake=false) 
		{return SetVar(name, (HFXVarValue)&value, HFXPARAM_DOUBLE_ARRAY, HFXARRAY_COPY, arraySize, allowMake);}
	HFX_INLINE bool SetVarArrayByName(const char *name, unsigned int arraySize, const bool *value, bool allowMake=false) 
		{return SetVarByName(name, (HFXVarValue)&value, HFXPARAM_BOOL_ARRAY, HFXARRAY_COPY, arraySize, allowMake);}
	HFX_INLINE bool SetVarArray(const HFXParamID name, unsigned int arraySize, const bool *value, bool allowMake=false) 
		{return SetVar(name, (HFXVarValue)&value, HFXPARAM_BOOL_ARRAY, HFXARRAY_COPY, arraySize, allowMake);}
	HFX_INLINE bool SetVarArrayByName(const char *name, unsigned int arraySize, const float *value, bool allowMake=false) 
		{return SetVarByName(name, (HFXVarValue)&value, HFXPARAM_FLOAT_ARRAY, HFXARRAY_COPY, arraySize, allowMake);}
	HFX_INLINE bool SetVarArray(const HFXParamID name, unsigned int arraySize, const float *value, bool allowMake=false) 
		{return SetVar(name, (HFXVarValue)&value, HFXPARAM_FLOAT_ARRAY, HFXARRAY_COPY, arraySize, allowMake);}
	HFX_INLINE bool SetVarArrayByName(const char *name, unsigned int arraySize, const int *value, bool allowMake=false) 
		{return SetVarByName(name, (HFXVarValue)&value, HFXPARAM_INT_ARRAY, HFXARRAY_COPY, arraySize, allowMake);}
	HFX_INLINE bool SetVarArray(const HFXParamID name, unsigned int arraySize, const int *value, bool allowMake=false) 
		{return SetVar(name, (HFXVarValue)&value, HFXPARAM_INT_ARRAY, HFXARRAY_COPY, arraySize, allowMake);}
 
	template<typename T, int S> 
	HFX_INLINE bool SetVarTypeArrayByName(const char *name, const T (value)[S], bool allowMake = false) 
		{return SetVarArrayByName(name, S, (const T*)value, allowMake);}
 
	template<typename T, int S> 
	HFX_INLINE bool SetVarTypeArray(const HFXParamID name, const T (value)[S], bool allowMake = false) 
		{return SetVarArray(name, S, (const T*)value, allowMake);}
 
	HFX_INLINE bool SetVarElementByName(const char *name, unsigned int arrayIndex, const double value) 
		{return SetVarByName(name, (HFXVarValue)&value, HFXPARAM_DOUBLE_ARRAY, HFXARRAY_INDEX, arrayIndex, false);}
	HFX_INLINE bool SetVarElement(const HFXParamID name, unsigned int arrayIndex, const double value) 
		{return SetVar(name, (HFXVarValue)&value, HFXPARAM_DOUBLE_ARRAY, HFXARRAY_INDEX, arrayIndex, false);}
	HFX_INLINE bool SetVarElementByName(const char *name, unsigned int arrayIndex, const bool value) 
		{return SetVarByName(name, (HFXVarValue)&value, HFXPARAM_BOOL_ARRAY, HFXARRAY_INDEX, arrayIndex, false);}
	HFX_INLINE bool SetVarElement(const HFXParamID name, unsigned int arrayIndex, const bool value) 
		{return SetVar(name, (HFXVarValue)&value, HFXPARAM_BOOL_ARRAY, HFXARRAY_INDEX, arrayIndex, false);}
	HFX_INLINE bool SetVarElementByName(const char *name, unsigned int arrayIndex, const float value) 
		{return SetVarByName(name, (HFXVarValue)&value, HFXPARAM_FLOAT_ARRAY, HFXARRAY_INDEX, arrayIndex, false);}
	HFX_INLINE bool SetVarElement(const HFXParamID name, unsigned int arrayIndex, const float value) 
		{return SetVar(name, (HFXVarValue)&value, HFXPARAM_FLOAT_ARRAY, HFXARRAY_INDEX, arrayIndex, false);}
	HFX_INLINE bool SetVarElementByName(const char *name, unsigned int arrayIndex, const int value) 
		{return SetVarByName(name, (HFXVarValue)&value, HFXPARAM_INT_ARRAY, HFXARRAY_INDEX, arrayIndex, false);}
	HFX_INLINE bool SetVarElement(const HFXParamID name, unsigned int arrayIndex, const int value) 
		{return SetVar(name, (HFXVarValue)&value, HFXPARAM_INT_ARRAY, HFXARRAY_INDEX, arrayIndex, false);}
 
	HFX_INLINE bool GetVarByName(const char *name, void *value, HFXParamType t, 
		HFXARRAY gettype=HFXARRAY_NOTARRAY, unsigned int indexORcount=0 )const
		{return GetVar(HFX_PARAM_ENCODE_STRING(name), value, t, gettype, indexORcount);}
 
	HFX_INLINE bool GetVarByName(const char *name, float &value )const
		{return GetVarByName(name, (void*)&value, HFXPARAM_FLOAT);}
	HFX_INLINE bool GetVar( const HFXParamID name, float &value )const
		{return GetVar(name, (void*)&value, HFXPARAM_FLOAT);}
	HFX_INLINE bool GetVarByName(const char *name, double &value )const
		{return GetVarByName(name, (void*)&value, HFXPARAM_DOUBLE);}
	HFX_INLINE bool GetVar( const HFXParamID name, double &value )const
		{return GetVar(name, (void*)&value, HFXPARAM_DOUBLE);}
	HFX_INLINE bool GetVarByName(const char *name, int &value )const
		{return GetVarByName(name, (void*)&value, HFXPARAM_INT);}
	HFX_INLINE bool GetVar( const HFXParamID name, int &value )const
		{return GetVar(name, (void*)&value, HFXPARAM_INT);}
	HFX_INLINE bool GetVarByName(const char *name, bool &value )const
		{return GetVarByName(name, (void*)&value, HFXPARAM_BOOL);}
	HFX_INLINE bool GetVar( const HFXParamID name, bool &value )const
		{return GetVar(name, (void*)&value, HFXPARAM_BOOL);}
 
	HFX_INLINE bool GetVarElementByName(const char *name, unsigned int arrayIndex, float &value)const
		{return GetVarByName(name, (HFXVarValue)&value, HFXPARAM_FLOAT_ARRAY, HFXARRAY_INDEX, arrayIndex);}
	HFX_INLINE bool GetVarElement(const HFXParamID name, unsigned int arrayIndex, float &value)const
		{return GetVar(name, (HFXVarValue)&value, HFXPARAM_FLOAT_ARRAY, HFXARRAY_INDEX, arrayIndex);}
	HFX_INLINE bool GetVarElementByName(const char *name, unsigned int arrayIndex, double &value)const
		{return GetVarByName(name, (HFXVarValue)&value, HFXPARAM_DOUBLE_ARRAY, HFXARRAY_INDEX, arrayIndex);}
	HFX_INLINE bool GetVarElement(const HFXParamID name, unsigned int arrayIndex, double &value)const
		{return GetVar(name, (HFXVarValue)&value, HFXPARAM_DOUBLE_ARRAY, HFXARRAY_INDEX, arrayIndex);}
	HFX_INLINE bool GetVarElementByName(const char *name, unsigned int arrayIndex, bool &value)const
		{return GetVarByName(name, (HFXVarValue)&value, HFXPARAM_BOOL_ARRAY, HFXARRAY_INDEX, arrayIndex);}
	HFX_INLINE bool GetVarElement(const HFXParamID name, unsigned int arrayIndex, bool &value)const
		{return GetVar(name, (HFXVarValue)&value, HFXPARAM_BOOL_ARRAY, HFXARRAY_INDEX, arrayIndex);}
	HFX_INLINE bool GetVarElementByName(const char *name, unsigned int arrayIndex, int &value)const
		{return GetVarByName(name, (HFXVarValue)&value, HFXPARAM_INT_ARRAY, HFXARRAY_INDEX, arrayIndex);}
	HFX_INLINE bool GetVarElement(const HFXParamID name, unsigned int arrayIndex, int &value)const
		{return GetVar(name, (HFXVarValue)&value, HFXPARAM_INT_ARRAY, HFXARRAY_INDEX, arrayIndex);}
 
 
	HFX_INLINE bool GetVarArrayByName(const char *name, unsigned int arraySize, int *&value)const
		{return GetVarByName(name, (HFXVarValue)&value, HFXPARAM_INT_ARRAY, HFXARRAY_COPY, arraySize);}
	HFX_INLINE bool GetVarArrayByName(const char *name, unsigned int arraySize, float *&value)const
		{return GetVarByName(name, (HFXVarValue)&value, HFXPARAM_FLOAT_ARRAY, HFXARRAY_COPY, arraySize);}
	HFX_INLINE bool GetVarArrayByName(const char *name, unsigned int arraySize, double *&value)const
		{return GetVarByName(name, (HFXVarValue)&value, HFXPARAM_DOUBLE_ARRAY, HFXARRAY_COPY, arraySize);}
	HFX_INLINE bool GetVarArrayByName(const char *name, unsigned int arraySize, bool *&value)const
		{return GetVarByName(name, (HFXVarValue)&value, HFXPARAM_BOOL_ARRAY, HFXARRAY_COPY, arraySize);}
 
	HFX_INLINE bool GetVarArray(const HFXParamID name, unsigned int arraySize, float *&value)const
		{return GetVar(name, (HFXVarValue)&value, HFXPARAM_FLOAT_ARRAY, HFXARRAY_COPY, arraySize);}
	HFX_INLINE bool GetVarArray(const HFXParamID name, unsigned int arraySize, double *&value)const
		{return GetVar(name, (HFXVarValue)&value, HFXPARAM_DOUBLE_ARRAY, HFXARRAY_COPY, arraySize);}
	HFX_INLINE bool GetVarArray(const HFXParamID name, unsigned int arraySize, bool *&value)const
		{return GetVar(name, (HFXVarValue)&value, HFXPARAM_BOOL_ARRAY, HFXARRAY_COPY, arraySize);}
	HFX_INLINE bool GetVarArray(const HFXParamID name, unsigned int arraySize, int *&value)const
		{return GetVar(name, (HFXVarValue)&value, HFXPARAM_INT_ARRAY, HFXARRAY_COPY, arraySize);}
 
	template<typename T, int S> 
	HFX_INLINE bool GetVarTypeArrayByName(const char *name, T (value)[S])const
		{return GetVarArrayByName(name, S, (T*&)value);}
 
	template<typename T, int S> 
	HFX_INLINE bool GetVarTypeArray(const HFXParamID name, T (value)[S])const
		{return GetVarArray(name, S, (T*&)value);}
 
	//strict access start. (ensures type is set) use these for typing or parsing.
	HFX_INLINE bool SetVarFloatByName(const char *name, const float value, bool allowMake=false) 
		{ return SetVarByName(name, value, allowMake); }
	HFX_INLINE bool SetVarFloat(const HFXParamID name, const float value, bool allowMake=false) 
		{ return SetVar(name, value, allowMake); }
	HFX_INLINE bool SetVarDoubleByName(const char *name, const double value, bool allowMake=false) 
		{ return SetVarByName(name, value, allowMake); }
	HFX_INLINE bool SetVarDouble(const HFXParamID name, const double value, bool allowMake=false) 
		{ return SetVar(name, value, allowMake); }
	HFX_INLINE bool SetVarIntByName(const char *name, const int value, bool allowMake=false) 
		{ return SetVarByName(name, value, allowMake); }
	HFX_INLINE bool SetVarInt(const HFXParamID name, const int value, bool allowMake=false) 
		{ return SetVar(name, value, allowMake); }
	HFX_INLINE bool SetVarBoolByName(const char *name, const bool value, bool allowMake=false) 
		{ return SetVarByName(name, value, allowMake); }
	HFX_INLINE bool SetVarBool(const HFXParamID name, const bool value, bool allowMake=false) 
		{ return SetVar(name, value, allowMake); }
 
 
	HFX_INLINE bool SetVarDoubleArrayByName(const char * name, unsigned int size, const double *value, bool allowMake=false) 
		{ return SetVarArrayByName(name, size, value, allowMake); }
	HFX_INLINE bool SetVarDoubleArray(const HFXParamID name, unsigned int size, const double *value, bool allowMake=false) 
		{ return SetVarArray(name, size, value, allowMake); }
	HFX_INLINE bool SetVarFloatArrayByName(const char * name, unsigned int size, const float *value, bool allowMake=false) 
		{ return SetVarArrayByName(name, size, value, allowMake); }
	HFX_INLINE bool SetVarFloatArray(const HFXParamID name, unsigned int size, const float *value, bool allowMake=false) 
		{ return SetVarArray(name, size, value, allowMake); }
	HFX_INLINE bool SetVarIntArrayByName(const char * name, unsigned int size, const int *value, bool allowMake=false) 
		{ return SetVarArrayByName(name, size, value, allowMake); }
	HFX_INLINE bool SetVarIntArray(const HFXParamID name, unsigned int size, const int *value, bool allowMake=false) 
		{ return SetVarArray(name, size, value, allowMake); }
	HFX_INLINE bool SetVarBoolArrayByName(const char * name, unsigned int size, const bool *value, bool allowMake=false) 
		{ return SetVarArrayByName(name, size, value, allowMake); }
	HFX_INLINE bool SetVarBoolArray(const HFXParamID name, unsigned int size, const bool *value, bool allowMake=false) 
		{ return SetVarArray(name, size, value, allowMake); }
 
	HFX_INLINE bool SetVarFloatElementByName(const char * name, unsigned int index, const float value) 
		{ return SetVarElementByName(name, index, value); }
	HFX_INLINE bool SetVarFloatElement(const HFXParamID name, unsigned int index, const float value) 
		{ return SetVarElement(name, index, value); }
	HFX_INLINE bool SetVarIntElementByName(const char * name, unsigned int index, const int value) 
		{ return SetVarElementByName(name, index, value); }
	HFX_INLINE bool SetVarIntElement(const HFXParamID name, unsigned int index, const int value) 
		{ return SetVarElement(name, index, value); }
	HFX_INLINE bool SetVarBoolElementByName(const char * name, unsigned int index, const bool value) 
		{ return SetVarElementByName(name, index, value); }
	HFX_INLINE bool SetVarBoolElement(const HFXParamID name, unsigned int index, const bool value) 
		{ return SetVarElement(name, index, value); }
	HFX_INLINE bool SetVarDoubleElementByName(const char * name, unsigned int index, const double value) 
		{ return SetVarElementByName(name, index, value); }
	HFX_INLINE bool SetVarDoubleElement(const HFXParamID name, unsigned int index, const double value) 
		{ return SetVarElement(name, index, value); }
 
	HFX_INLINE bool SetVarDoubleArrayByFloatArray(const HFXParamID name, unsigned int size, const float *value, bool allowMake=false) 
	{
		double *__d_ = new double[size]; 
		for(unsigned int i=0;i!=size;i++) 
			__d_[i]=(double)value[i]; 
		bool retval; 
		retval = SetVarDoubleArray(name, size, __d_, allowMake); 
		delete [] __d_; 
		return retval; 
	}
	HFX_INLINE bool SetVarDoubleArrayByFloatArrayByName(const char *name, unsigned int size, const float *value, bool allowMake=false) 
		{ return SetVarDoubleArrayByFloatArray(HFX_PARAM_ENCODE_STRING(name),size,value,allowMake); }
 
	HFX_INLINE bool GetVarFloatByName(const char *name, float &value)const
		{ return GetVarByName(name, value); }
	HFX_INLINE bool GetVarDoubleByName(const char *name, double &value)const
		{ return GetVarByName(name, value); }
	HFX_INLINE bool GetVarIntByName(const char *name, int &value)const
		{ return GetVarByName(name, value); }
	HFX_INLINE bool GetVarBoolByName(const char *name, bool &value)const
		{ return GetVarByName(name, value); }
 
	HFX_INLINE bool GetVarFloatElementByName(const char *name, unsigned int index, float &value)const
		{ return GetVarElementByName(name, index, value); }
	HFX_INLINE bool GetVarDoubleElementByName(const char *name, unsigned int index, double &value)const
		{ return GetVarElementByName(name, index, value); }
	HFX_INLINE bool GetVarIntElementByName(const char *name, unsigned int index, int &value)const
		{ return GetVarElementByName(name, index, value); }
	HFX_INLINE bool GetVarBoolElementByName(const char *name, unsigned int index, bool &value)const
		{ return GetVarElementByName(name, index, value); }
 
	HFX_INLINE bool GetVarFloatArrayByName(const char *name, unsigned int size, float *&value)const
		{ return GetVarArrayByName(name, size, value); }
	HFX_INLINE bool GetVarDoubleArrayByName(const char *name, unsigned int size, double *&value)const
		{ return GetVarArrayByName(name, size, value); }
	HFX_INLINE bool GetVarIntArrayByName(const char *name, unsigned int size, int *&value)const
		{ return GetVarArrayByName(name, size, value); }
	HFX_INLINE bool GetVarBoolArrayByName(const char *name, unsigned int size, bool *&value)const
		{ return GetVarArrayByName(name, size, value); }
 
	HFX_INLINE bool GetVarFloat(const HFXParamID name, float &value)const
		{ return GetVar(name, value); }
	HFX_INLINE bool GetVarDouble(const HFXParamID name, double &value)const
		{ return GetVar(name, value); }
	HFX_INLINE bool GetVarInt(const HFXParamID name, int &value)const
		{ return GetVar(name, value); }
	HFX_INLINE bool GetVarBool(const HFXParamID name, bool &value)const
		{ return GetVar(name, value); }
 
	HFX_INLINE bool GetVarFloatElement(const HFXParamID name, unsigned int index, float &value)const
		{ return GetVarElement(name, index, value); }
	HFX_INLINE bool GetVarDoubleElement(const HFXParamID name, unsigned int index, double &value)const
		{ return GetVarElement(name, index, value); }
	HFX_INLINE bool GetVarIntElement(const HFXParamID name, unsigned int index, int &value)const
		{ return GetVarElement(name, index, value); }
	HFX_INLINE bool GetVarBoolElement(const HFXParamID name, unsigned int index, bool &value)const
		{ return GetVarElement(name, index, value); }
 
	HFX_INLINE bool GetVarFloatArray(const HFXParamID name, unsigned int size, float *&value)const
		{ return GetVarArray(name, size, value); }
	HFX_INLINE bool GetVarDoubleArray(const HFXParamID name, unsigned int size, double *&value)const
		{ return GetVarArray(name, size, value); }
	HFX_INLINE bool GetVarIntArray(const HFXParamID name, unsigned int size, int *&value)const
		{ return GetVarArray(name, size, value); }
	HFX_INLINE bool GetVarBoolArray(const HFXParamID name, unsigned int size, bool *&value)const
		{ return GetVarArray(name, size, value); }
 
	HFX_INLINE bool GetVarDoubleArrayByFloatArray(const HFXParamID name, unsigned int size, float *&value) const
	{
		double *__d_ = new double[size]; 
		for(unsigned int i=0;i!=size;i++) 
			__d_[i]=(double)value[i]; 
		bool retval; 
		if(retval = GetVarDoubleArray(name, size, __d_)) 
			for(unsigned int i = 0;i!=size;i++) 
				value[i]=(float)__d_[i]; 
		delete [] __d_; 
		return retval; 
	}
	HFX_INLINE bool GetVarDoubleArrayByFloatArrayByName(const char *name, unsigned int size, float *&value) const
		{ return GetVarDoubleArrayByFloatArray(HFX_PARAM_ENCODE_STRING(name),size,value); }
 
	// util helpers
	// Vec3 and Vec3f
	HFX_INLINE bool SetVarVec3ByName(const char *name, const double *hfxVect3, bool bAllowMake=false) 
		{return SetVarDoubleArrayByName(name, 3, hfxVect3, bAllowMake);}
	HFX_INLINE bool SetVarVec3(const HFXParamID name, const double *hfxVect3, bool bAllowMake=false) 
		{return SetVarDoubleArray(name, 3, hfxVect3, bAllowMake);}
	HFX_INLINE bool GetVarVec3ByName(const char *name, double *&hfxVect3) const
		{return GetVarDoubleArrayByName(name, 3, hfxVect3);}
	HFX_INLINE bool GetVarVec3(const HFXParamID name, double *&hfxVect3) const
		{return GetVarDoubleArray(name, 3, hfxVect3);}
 
	HFX_INLINE bool SetVarVec3fByName(const char *name, const float *hfxVect3f, bool bAllowMake=false) 
		{return SetVarFloatArrayByName(name, 3, hfxVect3f, bAllowMake);}
	HFX_INLINE bool SetVarVec3f(const HFXParamID name, const float *hfxVect3f, bool bAllowMake=false) 
		{return SetVarFloatArray(name, 3, hfxVect3f, bAllowMake);}
	HFX_INLINE bool GetVarVec3fByName(const char *name, float *&hfxVect3f) const
		{return GetVarFloatArrayByName(name, 3, hfxVect3f);}
	HFX_INLINE bool GetVarVec3f(const HFXParamID name, float *&hfxVect3f) const
		{return GetVarFloatArray(name, 3, hfxVect3f);}
	//strict access end.
#endif
/*
	virtual bool SetVar(HFXParamID id, const char *value, HFXParamType t, 
		HFXARRAY settype=HFXARRAY_NOTARRAY, unsigned int indexORcount=0) = 0;

	virtual bool GetVar(HFXParamID id, const char *value, HFXParamType t,
		HFXARRAY gettype=HFXARRAY_NOTARRAY, unsigned int indexORcount=0 )const = 0;
*/
	virtual class IStack *GetDefaultStack() const=0; 
	virtual void SetDefaultStack(class IStack *stack)=0; 
 
	virtual bool Copy(IHapticEffectParamGroup *&group) const=0; 
 
	virtual HFXEffectID StandardEffectID() const=0; 
	virtual void ChangeStandardEffectID(const HFXEffectID type)=0; 
	// creation
	virtual bool CreateStandardEffect( IHFXEffect *&pHandle, class IStack *createOnStack=0,  const char *customName=0 )=0; 
	HFX_INLINE bool CreateStandardEffect( IStack *createOnStack ) 
	{
		return CreateStandardEffect(hfxNoHandle, createOnStack, 0); 
	}
	template<typename T> 
	HFX_INLINE bool CreateStandardEffect( T *&pHandle, class IStack *createOnStack=0, const char *customName=0) 
	{
		return CreateStandardEffect((IHFXEffect *&)pHandle, createOnStack, customName); 
	}
	HFX_INLINE HFXParamType GetVarTypeByName(const char *param, unsigned int *arraysize=0) const{ return GetVarType(HFX_PARAM_ENCODE_STRING(param), arraysize); }
	virtual HFXParamType GetVarType(const HFXParamID param, unsigned int *arraysize=0) const=0; 
 
}; 
typedef IHapticEffectParamGroup IHFXParamGroup; 
 
#define HFX_PARAMETER_INIT(classname, type, name, defaultvalue) \
	const char * const classname::ID_##type##name##_String = #name; \
	const HFXParamID classname::ID_##type##name = HFX_PARAM_ENCODE_STRING(classname::ID_##type##name##_String); \
	const type classname::ID_##type##name##_Default = defaultvalue;  
 
typedef void (*HFXPGCreate_t)(IHFXParamGroup*&ptr); 
typedef void (*HFXPGDestroy_t)(IHFXParamGroup*&ptr); 
 
template<typename T> 
void HFXDefaultAllocateParameterGroup(IHFXParamGroup *&ptr){ptr = new T;}; 
 
template<typename T> 
void HFXDefaultDeallocateParameterGroup(IHFXParamGroup *&ptr){if(!ptr)return; delete ((T*)ptr); ptr=0;}; 
 
#endif
 