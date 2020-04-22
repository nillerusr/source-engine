//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANORAMA_ILOCALIZE_H
#define PANORAMA_ILOCALIZE_H

#include "language.h"
#include "tier0/platform.h"
#include "tier1/interface.h"
#include "tier1/utlflags.h"
#include "tier1/utlsymbol.h"
#include "tier1/utlstring.h"
#include "tier1/utlmap.h"
#include "tier1/fileio.h"
#include "tier1/utlpriorityqueue.h"
#include "steam/steamtypes.h"
#if defined( SOURCE2_PANORAMA )
#include "currencyamount.h"
#else
#include "stime.h"
#include "constants.h"
#include "globals.h"
#include "amount.h"
#include "rtime.h"
#endif
#include "reliabletimer.h"
#include "language.h"

namespace panorama
{

//-----------------------------------------------------------------------------
// Purpose: type of variable data we are supporting
//-----------------------------------------------------------------------------
enum EPanelKeyType
{
	k_ePanelVartype_None,
	k_ePanelVartype_String,
	k_ePanelVartype_Time,
	k_ePanelVartype_Money,
	k_ePanelVartype_Number,
	k_ePanelVartype_Generic
};

// help function syntax for the generic key handler
typedef const char *( *PFNLocalizeDialogVariableHandler )( const CUtlString &sStringValue, int nIntValue, const IUIPanel *pPanel, const char *pszKey, void *pUserData );

enum EPanelKeyTimeModifiers
{
	k_ePanelKeyTimeModifiers_ShortDate = 1 << 0,
	k_ePanelKeyTimeModifiers_LongDate = 1 << 1,
	k_ePanelKeyTimeModifiers_ShortTime = 1 << 2,
	k_ePanelKeyTimeModifiers_LongTime = 1 << 3,
	k_ePanelKeyTimeModifiers_DateTime =  1 << 4,
	k_ePanelKeyTimeModifiers_Relative = 1 << 5,
	k_ePanelKeyTimeModifiers_Duration = 1 << 6,
};

enum EStringTruncationStyle
{
	k_eStringTruncationStyle_None,
	k_eStringTruncationStyle_Rear, // prevent any chars being added above max length
	k_eStringTruncationStyle_Front, // remove 
};

enum EStringTransformStyle
{
	k_eStringTransformStyle_None,
	k_eStringTransformStyle_Uppercase,
	k_eStringTransformStyle_Lowercase,
};


//-----------------------------------------------------------------------------
// Purpose: callback interface to help measuring strings
//-----------------------------------------------------------------------------
class ILocalizationStringSizeResolver
{
public:
	virtual ~ILocalizationStringSizeResolver() {}
	virtual int ResolveStringLengthInPixels( const char *pchString ) = 0;
};


const uint32 k_nLocalizeMaxChars = (uint32)~0;

class CPanelKeyValue;
class CLocalization;
class CPanel2D;

//-----------------------------------------------------------------------------
// Purpose: interface to final string data to display to users
//-----------------------------------------------------------------------------
class ILocalizationString
{
public:
	virtual ~ILocalizationString() {}

	// Get the length of the string in characters
	virtual int Length() const = 0;
	virtual bool IsEmpty() const = 0;

	virtual const char *String() const = 0;
	virtual const char *StringNoTransform() const = 0;
	virtual operator const char *() const = 0;

	// add this string on the end
	virtual bool AppendText( const char *pchText ) = 0;
	// done with this string, delete it
	virtual void Release() const = 0;

	virtual EStringTransformStyle GetTransformStyle() const = 0;
	virtual const IUIPanel *GetOwningPanel() const = 0;
	virtual uint32 GetMaxChars() const = 0;
	virtual EStringTruncationStyle GetTruncationStyle() const = 0;

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName ) = 0;
#endif
protected:
	// internal loc engine helpers, you won't call these
	friend class CLocalization;
	virtual void Recalculate( const CUtlString *pString ) = 0;
	virtual bool BContainsDialogVariable( const CPanelKeyValue &key ) = 0;
};

class CLocStringSafePointer
{
public:
	CLocStringSafePointer()
	{
		m_pString = NULL;
	}

	CLocStringSafePointer( const ILocalizationString *pLocString )
	{
		m_pString = pLocString;
	}

	CLocStringSafePointer &operator =( const ILocalizationString *pString )
	{
		if ( m_pString == pString )
			return *this;

		Clear();

		m_pString = const_cast<ILocalizationString *>( pString );

		return *this;
	}
 
	const ILocalizationString *operator ->( ) const
	{
		return Get();
	}

	const ILocalizationString *operator *( ) const
	{
		return Get();
	}

	operator const ILocalizationString*( ) const
	{
		return Get();
	}
	
	~CLocStringSafePointer()
	{
		Clear();
	}

	void Clear()
	{
		if ( m_pString )
		{
			m_pString->Release();
			m_pString = NULL;
		}
	}

	bool IsValid() { return m_pString != nullptr; }

	const ILocalizationString *Get() const
	{
		return m_pString;
	}

private: 
	const ILocalizationString *m_pString;
};


//-----------------------------------------------------------------------------
// Purpose: interface to localize strings
//-----------------------------------------------------------------------------
class IUILocalization
{
public:
	// change the language used by the loc system
	virtual bool SetLanguage( const char *pchLanguage ) = 0;
#if defined( SOURCE2_PANORAMA )
	// add a loc file to the system, in the form of <prefix>_<language>.txt , i.e dota_french.txt, the files will be loaded from the panorama/localization folder of your mod
	virtual bool BLoadLocalizationFile( const char *pchFilePrefix ) = 0;
#else
	virtual ELanguage CurrentLanguage() = 0;
#endif

	virtual void InstallCustomDialogVariableHandler( const char *pchCustomHandlerName, PFNLocalizeDialogVariableHandler pfnLocalizeFunc, void *pUserData = NULL ) = 0;
	virtual void RemoveCustomDialogVariableHandler( const char *pchCustomHandlerName ) = 0;

	// find the string corresponding to this localization token, or if we don't find it then just return back the string wrapped in a loc object
	virtual const ILocalizationString *PchFindToken( const IUIPanel *pPanel, const char *pchToken, const uint32 ccMax , EStringTruncationStyle eTrunkStyle, EStringTransformStyle eTransformStyle, bool bAllowDialogVariable = false ) = 0;

	// give me a localize string wrapper around this string, don't try and apply token localizing on it though, but do optionally allow it to have dialog variables that we parse in it
	// be careful allowing dialog variable parsing, you want to sanitize any user input before allowing it
	virtual const ILocalizationString *PchSetString( const IUIPanel *pPanel, const char *pchText, const uint32 ccMax, EStringTruncationStyle eTrunkStyle, EStringTransformStyle eTransformStyle, bool bAllowDialogVariable, bool bStringAlreadyFullyParsed ) = 0;

	virtual const ILocalizationString *ChangeTransformStyleAndRelease( const ILocalizationString *pLocalizationString, EStringTransformStyle eTranformStyle ) = 0;

	// copy an existing loc string without altering the ref count on the current
	virtual ILocalizationString *CloneString( const IUIPanel *pPanel, const ILocalizationString *pLocToken ) = 0;

	// return the raw, un-parsed, value for this loc token, returns NULL if we didn't have this token in a loc file from disk
	virtual const char *PchFindRawString( const char *pchToken ) = 0;

	virtual bool SetDialogVariable( const IUIPanel *pPanel, const char *pchKey, const char *pchValue ) = 0;
#if defined( SOURCE2_PANORAMA )
	virtual bool SetDialogVariable( const IUIPanel *pPanel, const char *pchKey, time_t timeVal ) = 0;
	virtual bool SetDialogVariable( const IUIPanel *pPanel, const char *pchKey, CCurrencyAmount amount ) = 0;
#else
	virtual bool SetDialogVariable( const IUIPanel *pPanel, const char *pchKey, CRTime timeVal ) = 0;
	virtual bool SetDialogVariable( const IUIPanel *pPanel, const char *pchKey, CAmount amount ) = 0;
#endif

	virtual bool SetDialogVariable( const IUIPanel *pPanel, const char *pchKey, int nVal ) = 0;

	// copy all the dialog vars to a new panel
	virtual void CloneDialogVariables( const IUIPanel *pPanelFrom, IUIPanel *pPanelTo ) = 0;

	// force a re-evaluation of a specific dialog variable
	virtual void DirtyDialogVariable( const IUIPanel *pPanel, const char *pchKey ) = 0;


	// given this loc string find the longest string in any language that we could display here and update to use it
	virtual void SetLongestStringForToken( const ILocalizationString *pLocalizationString, ILocalizationStringSizeResolver *pResolver ) = 0;
};


//-----------------------------------------------------------------------------
// Purpose: Wrapper around ILocalizationString that will take care of making
// a copy if it ends up needing to be mutable.
//-----------------------------------------------------------------------------
class CMutableLocalizationString
{
public:
	CMutableLocalizationString()
		: m_bMutable( false )
		, m_pString( NULL )
	{
	}

	CMutableLocalizationString( ILocalizationString *pString )
		: m_bMutable( true )
		, m_pString( pString )
	{
	}

	CMutableLocalizationString( const ILocalizationString *pString )
		: m_bMutable( false )
		, m_pString( const_cast< ILocalizationString * >( pString ) )
	{
	}

	CMutableLocalizationString( const CMutableLocalizationString &other )
	{
		m_bMutable = true;
		if ( other.m_pString )
		{
			m_pString = UILocalize()->CloneString( other.m_pString->GetOwningPanel(), other.m_pString );
		}
		else
		{
			m_pString = NULL;
		}
	}

	~CMutableLocalizationString()
	{
		Clear();
	}

	const ILocalizationString *Get() const
	{
		return m_pString;
	}

	ILocalizationString *GetMutable()
	{
		if ( m_bMutable || !m_pString )
			return m_pString;

		ILocalizationString *pOldString = m_pString;

		m_pString = UILocalize()->CloneString( pOldString->GetOwningPanel(), pOldString );
		m_bMutable = true;

		pOldString->Release();

		return m_pString;
	}

	void Clear()
	{
		if ( m_pString )
		{
			m_pString->Release();
			m_pString = NULL;
		}
	}

	const ILocalizationString *Extract()
	{
		const ILocalizationString *pString = m_pString;
		m_pString = NULL;
		return pString;
	}
	ILocalizationString *ExtractMutable()
	{
		ILocalizationString *pString = GetMutable();
		m_pString = NULL;
		return pString;
	}

	// The -> operator is only overloaded to return a const string. If you need a mutable version, you must call GetMutable directly. 
	const ILocalizationString *operator ->() const
	{
		return Get();
	}
	
	
	explicit operator bool() const
	{
		return Get() != NULL;
	}
	bool operator ==( const CMutableLocalizationString &other ) const
	{
		return m_pString == other.m_pString;
	}
	bool operator !=( const CMutableLocalizationString &other ) const
	{
		return !( *this == other );
	}

	bool operator ==( const ILocalizationString *pString ) const
	{
		return m_pString == pString;
	}
	bool operator !=( const ILocalizationString *pString ) const
	{
		return !( *this == pString );
	}

	CMutableLocalizationString &operator =( const CMutableLocalizationString &other )
	{
		if ( other.m_pString == m_pString )
			return *this;

		Clear();
		
		m_bMutable = true;
		if ( other.m_pString )
		{
			m_pString = UILocalize()->CloneString( other.m_pString->GetOwningPanel(), other.m_pString );
		}
		else
		{
			m_pString = NULL;
		}

		return *this;
	}

	CMutableLocalizationString &operator =( const ILocalizationString *pString )
	{
		if ( m_pString == pString )
			return *this;

		Clear();

		m_bMutable = false;
		m_pString = const_cast< ILocalizationString * >( pString );

		return *this;
	}

private:
	bool m_bMutable;
	ILocalizationString *m_pString;;
};


} // namespace panorama

#endif // PANORAMA_ILOCALIZE_H
