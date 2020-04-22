//========= Copyright (c), Valve LLC, All rights reserved. ============
//
// Purpose: A utility for printing out reports on the GC in an ordered manner
//
//=============================================================================

#ifndef GCREPORTPRINTER_H
#define GCREPORTPRINTER_H

#pragma once

#include "gclogger.h"

namespace GCSDK
{
	//-----------------------------------------------------------------------------------------------------
	// CGCReportPrinter - A class that will handle formatting a report for outputting to a console or other 
	// data sources. Just create an instance of the class, add the columns, then add the fields, and print. Below is an example:
	//
	// CGCReportPrinter rp;
	// rp.AddStringColumn( "Names" );
	// rp.AddFloatColumn( "SomeValue", 2, CGCReportPrinter::eSummary_Total );
	// FOR_EACH_VALUE( v )
	//		rp.StrValue( v.Name );
	//		rp.FloatValue( v.Float );
	//		rp.CommitRow();
	// rp.SortReport( "SomeValue" );
	// rp.PrintReport( SPEW_CONSOLE );
	//
	//-----------------------------------------------------------------------------------------------------

	class CGCReportPrinter
	{
	public:

		CGCReportPrinter();
		~CGCReportPrinter();

		//what type of summary to report at the end of the report for the column (not used for string columns)
		enum ESummaryType
		{
			eSummary_None,
			eSummary_Total,
			eSummary_Max
		};

		//how to format memory values
		enum EIntDisplayType
		{
			eIntDisplay_Normal,
			eIntDisplay_Memory_MB,
		};

		//called to handle inserting columns into the report of various data types. These must be called before any data has been added
		//to the report, and will fail if there is outstanding data
		bool AddStringColumn( const char* pszColumn );
		bool AddSteamIDColumn( const char* pszColumn );
		bool AddIntColumn( const char* pszColumn, ESummaryType eSummary, EIntDisplayType eIntDisplay = eIntDisplay_Normal );
		bool AddFloatColumn( const char* pszColumn, ESummaryType eSummary, uint32 unNumDecimal = 2 );

		//called to reset all report data
		void ClearData();
		//called to reset the entire report
		void Clear();

		//called to add the various data to the report, the order of this must match the columns that were added originally
		bool StrValue( const char* pszStr, const char* pszLink = NULL );
		bool IntValue( int64 nValue, const char* pszLink = NULL );
		bool FloatValue( double fValue, const char* pszLink = NULL );
		bool SteamIDValue( CSteamID id, const char* pszLink = NULL );
		//called to commit the values that have been added as a new row
		bool CommitRow();

		//sorts the report based upon the specified column name
		void SortReport( const char* pszColumn, bool bDescending = true );
		//same as the above, but sorts based upon the specified column index
		void SortReport( uint32 nColIndex, bool bDescending = true );

		//called to print out the provided report
		void PrintReport( CGCEmitGroup& eg, uint32 nTop = 0 );

	private:

		friend class CReportRowSorter;

		//the type of each column
		enum EColumnType
		{
			eCol_String,
			eCol_Int,
			eCol_Float,
			eCol_SteamID,
		};

		//our list of columns
		struct Column_t
		{
			CUtlString		m_sName;
			EColumnType		m_eType;
			ESummaryType	m_eSummary;
			uint8			m_nNumDecimals;	//for floats only
			EIntDisplayType	m_eIntDisplay; // for ints only
		};
		CUtlVector< Column_t >		m_Columns;

		//a variant that holds onto the column field data
		struct Variant_t
		{
			Variant_t();
			CUtlString	m_sStr;
			CUtlString	m_sLink;		//optional link to put around the value
			int64		m_nInt;
			double		m_fFloat;
			CSteamID	m_SteamID;
		};

		//our data block
		typedef CCopyableUtlVector< Variant_t > TRow;
		CUtlVector< TRow* > m_Rows;

		//a row that isn't quite in the table, but consists of the row being built to
		//avoid issues with partial rows
		TRow m_RowBuilder;
	};	

} // namespace GCSDK


#endif // GCLOGGER_H
