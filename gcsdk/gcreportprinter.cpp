//========= Copyright (c), Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================


#include "stdafx.h"
#include "gcreportprinter.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace GCSDK
{


CGCReportPrinter::Variant_t::Variant_t() :
	m_nInt( 0 ),
	m_fFloat( 0 )
{}

CGCReportPrinter::CGCReportPrinter()
{
}

CGCReportPrinter::~CGCReportPrinter()
{
	Clear();
}

bool CGCReportPrinter::AddStringColumn( const char* pszColumn )
{
	//don't allow adding columns if data is already present
	if( m_Rows.Count() > 0 )
		return false;

	int nIndex = m_Columns.AddToTail();
	Column_t& col = m_Columns[ nIndex ];
	col.m_sName = pszColumn;
	col.m_eType = eCol_String;
	col.m_eSummary = eSummary_None;
	col.m_nNumDecimals = 0;
	col.m_eIntDisplay = eIntDisplay_Normal;
	return true;
}

bool CGCReportPrinter::AddIntColumn( const char* pszColumn, ESummaryType eSummary, EIntDisplayType eIntDisplay /* = eIntDisplay_Normal */ )
{
	//don't allow adding columns if data is already present
	if( m_Rows.Count() > 0 )
		return false;

	int nIndex = m_Columns.AddToTail();
	Column_t& col = m_Columns[ nIndex ];
	col.m_sName = pszColumn;
	col.m_eType = eCol_Int;
	col.m_eSummary = eSummary;
	col.m_nNumDecimals = 0;
	col.m_eIntDisplay = eIntDisplay;
	return true;
}

bool CGCReportPrinter::AddFloatColumn( const char* pszColumn, ESummaryType eSummary, uint32 unNumDecimal /* = 2 */ )
{
	//don't allow adding columns if data is already present
	if( m_Rows.Count() > 0 )
		return false;

	int nIndex = m_Columns.AddToTail();
	Column_t& col = m_Columns[ nIndex ];
	col.m_sName = pszColumn;
	col.m_eType = eCol_Float;
	col.m_eSummary = eSummary;
	col.m_nNumDecimals = unNumDecimal;
	col.m_eIntDisplay = eIntDisplay_Normal;
	return true;
}

bool CGCReportPrinter::AddSteamIDColumn( const char* pszColumn )
{
	//don't allow adding columns if data is already present
	if( m_Rows.Count() > 0 )
		return false;

	int nIndex = m_Columns.AddToTail();
	Column_t& col = m_Columns[ nIndex ];
	col.m_sName = pszColumn;
	col.m_eType = eCol_SteamID;
	col.m_eSummary = eSummary_None;
	col.m_nNumDecimals = 0;
	col.m_eIntDisplay = eIntDisplay_Normal;
	return true;
}

void CGCReportPrinter::ClearData()
{
	m_Rows.PurgeAndDeleteElements();
}

//called to reset the entire report
void CGCReportPrinter::Clear()
{
	ClearData();
	m_Columns.Purge();
}

//called to commit the values that have been added as a new row
bool CGCReportPrinter::CommitRow()
{
	//only let full rows be committed
	if( m_RowBuilder.Count() != m_Columns.Count() )
		return false;
	if( m_Columns.IsEmpty() )
		return false;

	m_Rows.AddToTail( new TRow( m_RowBuilder ) );
	m_RowBuilder.RemoveAll();	
	return true;
}

//called to add the various data to the report, the order of this must match the columns that were added originally
bool CGCReportPrinter::StrValue( const char* pszStr, const char* pszLink )
{
	//make sure we have a following column and that the type matches
	if( ( m_RowBuilder.Count() >= m_Columns.Count() ) || ( m_Columns[ m_RowBuilder.Count() ].m_eType != eCol_String ) )
		return false;

	Variant_t& val = m_RowBuilder[ m_RowBuilder.AddToTail() ];
	val.m_sStr = pszStr;
	val.m_sLink = pszLink;
	return true;
}

bool CGCReportPrinter::IntValue( int64 nValue, const char* pszLink )
{
	//make sure we have a following column and that the type matches
	if( ( m_RowBuilder.Count() >= m_Columns.Count() ) || ( m_Columns[ m_RowBuilder.Count() ].m_eType != eCol_Int ) )
		return false;

	Variant_t& val = m_RowBuilder[ m_RowBuilder.AddToTail() ];
	val.m_nInt = nValue;
	val.m_sLink = pszLink;
	return true;
}

bool CGCReportPrinter::FloatValue( double fValue, const char* pszLink )
{
	//make sure we have a following column and that the type matches
	if( ( m_RowBuilder.Count() >= m_Columns.Count() ) || ( m_Columns[ m_RowBuilder.Count() ].m_eType != eCol_Float ) )
		return false;

	Variant_t& val = m_RowBuilder[ m_RowBuilder.AddToTail() ];
	val.m_fFloat = fValue;
	val.m_sLink = pszLink;
	return true;
}

bool CGCReportPrinter::SteamIDValue( CSteamID id, const char* pszLink )
{
	//make sure we have a following column and that the type matches
	if( ( m_RowBuilder.Count() >= m_Columns.Count() ) || ( m_Columns[ m_RowBuilder.Count() ].m_eType != eCol_SteamID ) )
		return false;

	Variant_t& val = m_RowBuilder[ m_RowBuilder.AddToTail() ];
	val.m_SteamID = id;
	val.m_sLink = pszLink;
	return true;
}

//class that implements sorting our rows based upon a provided column with ascending or descending ordering
class CReportRowSorter
{
public:

	CReportRowSorter( bool bDescending, uint32 nCol, CGCReportPrinter::EColumnType eType ) :
		m_bDescending( bDescending ), m_nCol( nCol ), m_eType( eType )
	{
	}

	bool operator()( const CGCReportPrinter::TRow* pR1, const CGCReportPrinter::TRow* pR2 )
	{
		//to implement ascending vs descending, we can just flip our inputs
		if( m_bDescending )
			std::swap( pR1, pR2 );

		const CGCReportPrinter::Variant_t& v1 = ( *pR1 )[ m_nCol ];
		const CGCReportPrinter::Variant_t& v2 = ( *pR2 )[ m_nCol ];

		switch( m_eType )
		{
		case CGCReportPrinter::eCol_String:
			return stricmp( v1.m_sStr, v2.m_sStr ) < 0;
		case CGCReportPrinter::eCol_Int:
			return v1.m_nInt < v2.m_nInt;
		case CGCReportPrinter::eCol_Float:
			return v1.m_fFloat < v2.m_fFloat;
		case CGCReportPrinter::eCol_SteamID:
			return v1.m_SteamID < v2.m_SteamID;
		}

		return false;
	}

	bool							m_bDescending;
	uint32							m_nCol;
	CGCReportPrinter::EColumnType	m_eType;
};


//sorts the report based upon the specified column name
void CGCReportPrinter::SortReport( const char* pszColumn, bool bDescending )
{
	//find our column
	FOR_EACH_VEC( m_Columns, nCol )
	{
		if( stricmp( m_Columns[ nCol ].m_sName, pszColumn ) == 0 )
		{
			CReportRowSorter sorter( bDescending, nCol, m_Columns[ nCol ].m_eType );
			std::sort( m_Rows.begin(), m_Rows.end(), sorter );
			break;
		}
	}
}

void CGCReportPrinter::SortReport( uint32 nColIndex, bool bDescending )
{
	if( nColIndex < ( uint32 )m_Columns.Count() )
	{
		CReportRowSorter sorter( bDescending, nColIndex, m_Columns[ nColIndex ].m_eType );
		std::sort( m_Rows.begin(), m_Rows.end(), sorter );
	}
}

//utility to count the number of digits on the provided integer
static uint CountDigits( int64 nInt )
{
	//the zero special case, since it would otherwise fall out of the loop too early
	if( nInt == 0 )
		return 1;

	int nDigits = 0;
	if( nInt < 0 )
	{
		//for the minus sign
		nDigits++;
	}

	while( nInt != 0 )
	{
		nInt /= 10;
		nDigits++;
	}

	return nDigits;
}

static uint CountIntWidth( int64 nValue, CGCReportPrinter::EIntDisplayType eIntDisplay )
{
	uint unDigits;
	switch ( eIntDisplay )
	{
	case CGCReportPrinter::eIntDisplay_Memory_MB:
		// "1234.56 MB"
		unDigits = CountDigits( nValue / k_nMegabyte ) + 1 + 2 + 3;
		break;

	case CGCReportPrinter::eIntDisplay_Normal:
	default:
		// 12345678
		unDigits = CountDigits( nValue );
		break;
	}
	return unDigits;
}

static const char * GetIntValueDisplay( int64 nValue, CGCReportPrinter::EIntDisplayType eIntDisplay )
{
	static CFmtStr1024 s_fmtResult;
	switch ( eIntDisplay )
	{
	case CGCReportPrinter::eIntDisplay_Memory_MB:
		// "1234.56 MB"
		s_fmtResult.sprintf( "%lld.%02u MB", ( nValue / k_nMegabyte ), (uint32)( 100.0f * ( ( abs( nValue ) % k_nMegabyte ) / (float)k_nMegabyte ) ) );
		break;

	case CGCReportPrinter::eIntDisplay_Normal:
	default:
		// 12345678
		s_fmtResult.sprintf( "%lld", nValue );
		break;
	}
	return s_fmtResult;
}

//called to print out the provided report
void CGCReportPrinter::PrintReport( CGCEmitGroup& eg, uint32 nTop )
{
	//we need to determine our totals and maximum row widths for our columns first
	CUtlVector< uint32 > vColWidths;
	vColWidths.EnsureCapacity( m_Columns.Count() );
	CUtlVector< Variant_t > vSummary;
	vSummary.EnsureCapacity( m_Columns.Count() );

	CFmtStr1024 vMsg;
	CFmtStr1024 vSeparator;

	FOR_EACH_VEC( m_Columns, nCol )
	{
		const Column_t& col = m_Columns[ nCol ];
		uint32 nColWidth = V_strlen( col.m_sName );
		Variant_t summary;

		//run through all the values to find the row widths and the summary values
		FOR_EACH_VEC( m_Rows, nRow )
		{
			//bail after the first N elements
			if( ( nTop > 0 ) && ( ( uint32 )nRow >= nTop ) )
				break;

			const Variant_t& v = ( *m_Rows[ nRow ] )[ nCol ];
			switch( col.m_eType )
			{
			case eCol_String:
				nColWidth = MAX( nColWidth, strlen( v.m_sStr ) );
				break;
			case eCol_SteamID:
				nColWidth = MAX( nColWidth, strlen( v.m_SteamID.Render() ) );
				break;
			case eCol_Float:
				nColWidth = MAX( nColWidth, CountDigits( ( int64 )v.m_fFloat ) + 1 + col.m_nNumDecimals );
				switch( col.m_eSummary )
				{
				case eSummary_Max:
					summary.m_fFloat = MAX( summary.m_fFloat, v.m_fFloat );
					break;
				case eSummary_Total:
					summary.m_fFloat += v.m_fFloat;
					break;
				}
				break;
			case eCol_Int:
				nColWidth = MAX( nColWidth, CountIntWidth( v.m_nInt, col.m_eIntDisplay ) );
				switch( col.m_eSummary )
				{
				case eSummary_Max:
					summary.m_nInt = MAX( summary.m_nInt, v.m_nInt );
					break;
				case eSummary_Total:
					summary.m_nInt += v.m_nInt;
					break;
				}
				break;
			}
		}

		//make sure the summary value contributes to the column width
		switch( col.m_eType )
		{
		case eCol_Float:
			nColWidth = MAX( nColWidth, CountDigits( ( int64 )summary.m_fFloat ) + 1 + col.m_nNumDecimals );
			break;
		case eCol_Int:
			nColWidth = MAX( nColWidth, CountIntWidth( summary.m_nInt, col.m_eIntDisplay ) );
			break;
		}

		//initialize our column sizes
		vColWidths.AddToTail( nColWidth );
		vSummary.AddToTail( summary );

		vMsg.AppendFormat( "%*s", nColWidth, col.m_sName.String() );
		vMsg.Append( ' ' );

		for( uint32 nChar = 0; nChar < nColWidth; nChar++ )
			vSeparator.Append( '-' );
		vSeparator.Append( ' ' );
	}

	//now print our header
	vMsg.Append( '\n' );
	vSeparator.Append( '\n' );

	EG_MSG( eg, "%s", vMsg.String() );
	EG_MSG( eg, "%s", vSeparator.String() );

	//buffer for compositing our value
	CFmtStr1024 vValue;

	//now print each of our columns
	FOR_EACH_VEC( m_Rows, nRow )
	{
		//bail after the first N elements
		if( ( nTop > 0 ) && ( ( uint32 )nRow >= nTop ) )
			break;

		vMsg.Clear();
		FOR_EACH_VEC( m_Columns, nCol )
		{
			const Column_t& col = m_Columns[ nCol ];
			const Variant_t& v = ( *m_Rows[ nRow ] )[ nCol ];
			const uint32 nColWidth = vColWidths[ nCol ];

			vValue.Clear();
			switch( col.m_eType )
			{
			case eCol_String:
				vValue.Append( v.m_sStr.String() );
				break;
			case eCol_SteamID:
				vValue.Append( v.m_SteamID.Render() );
				break;
			case eCol_Float:
				vValue.sprintf( "%.*f", col.m_nNumDecimals, v.m_fFloat );
				break;
			case eCol_Int:
				vValue.Append( GetIntValueDisplay( v.m_nInt, col.m_eIntDisplay ) );
				break;
			}
			
			//print out spaces before we do the link (so we don't have the whole table underlined)
			uint32 nValueLen = vValue.Length();
			uint32 nNumSpaces = nColWidth - MIN( nColWidth, nValueLen );
			for( uint32 nCurrSpace = 0; nCurrSpace < nNumSpaces; nCurrSpace++ )
				vMsg.Append( ' ' );
			
			//print out the link if one is provided
			if( !v.m_sLink.IsEmpty() )
			{
				vMsg.AppendFormat( "<link cmd=\"%s\">", v.m_sLink.String() );
				vMsg.Append( vValue );
				vMsg.Append( "</link>" );
			}
			else
			{
				//allow for steam ID special linking if no link is specified
				if( col.m_eType == eCol_SteamID )
				{
					// !FIXME! DOTAMERGE
					//vMsg.Append( v.m_SteamID.RenderLink() );
					vMsg.Append( v.m_SteamID.Render() );
				} else
					vMsg.Append( vValue );
			}

			vMsg.Append( ' ' );
		}

		vMsg.Append( '\n' );
		EG_MSG( eg, "%s", vMsg.String() );
	}

	//and finally our footer
	EG_MSG( eg, "%s", vSeparator.String() );

	//and our summary
	{
		vMsg.Clear();
		FOR_EACH_VEC( m_Columns, nCol )
		{
			const Column_t& col = m_Columns[ nCol ];
			const Variant_t& v = vSummary[ nCol ];
			const uint32 nColWidth = vColWidths[ nCol ];

			if( ( col.m_eType == eCol_String ) || ( col.m_eSummary == eSummary_None ) )
			{
				vMsg.AppendFormat( "%*s ", nColWidth, "" );
			}
			else
			{
				switch( col.m_eType )
				{
				case eCol_Float:
					vMsg.AppendFormat( "%*.*f ", nColWidth, col.m_nNumDecimals, v.m_fFloat );
					break;
				case eCol_Int:
					vMsg.AppendFormat( "%*s ", nColWidth, GetIntValueDisplay( v.m_nInt, col.m_eIntDisplay ) );
					break;
				}
			}
		}

		vMsg.Append( '\n' );
		EG_MSG( eg, "%s", vMsg.String() );
	}
}

} // namespace GCSDK
