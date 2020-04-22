//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
// A string class that keeps all its memory on the stack and performs no
// memory allocation
//=============================================================================//

#ifndef STACKSTRING_H
#define STACKSTRING_H

#ifdef _WIN32
#pragma once
#endif


template< size_t cubSize >
class CStackString
{
public:
	class CStackString() { Reset(); }
	class CStackString( const char *pchRHS ) { Set( pchRHS ); }

	void Reset() { m_rchBuffer[0] = 0; m_pchEnd = m_rchBuffer; }
	void Set( const char *pchValue );
	const char *Get() const { return m_rchBuffer; }
	static size_t GetCubSize() { return cubSize; }
	size_t Length() const { return m_pchEnd - &m_rchBuffer[0]; }

	CStackString & operator=( const char *pchValue ) { Set( pchValue ); return *this; }
	CStackString & operator+=( const char *pchValue ) { Append( pchValue ); return *this; }
	operator const char*() const { return m_rchBuffer; }

	void Format( PRINTF_FORMAT_STRING const char *pchFormat, ... );
	void AppendFormat( PRINTF_FORMAT_STRING const char *pchFormat, ... );
	void AppendFormatV( const char *pchFormat, va_list marker );
	void Append( const char *pchValue );

	void SetIndent( uint32 unCount, char chIndent = '\t' );
private:
	char m_rchBuffer[ cubSize ];
	char *m_pchEnd;
};

typedef CStackString<256> CStackString256;
typedef CStackString<512> CStackString512;
typedef CStackString<1024> CStackString1024;
typedef CStackString<8192> CStackStringMax;

template< size_t cubSize >
void CStackString<cubSize>::Set( const char *pchValue ) 
{ 
	V_strcpy_safe( m_rchBuffer, pchValue ); 
	m_pchEnd = &m_rchBuffer[Q_strlen(m_rchBuffer)]; 
}

template< size_t cubSize >
void CStackString<cubSize>::AppendFormatV( const char *pchFormat, va_list marker )
{
	size_t cubPrinted = Q_vsnprintf( m_pchEnd, GetCubSize() - Length(), pchFormat, marker );
	m_pchEnd += cubPrinted;
}


template< size_t cubSize >
void CStackString<cubSize>::Format( PRINTF_FORMAT_STRING const char *pchFormat, ... )
{
	va_list marker;
	va_start( marker, pchFormat );
	size_t cubPrinted = Q_vsnprintf( m_rchBuffer, GetCubSize(), pchFormat, marker );
	m_pchEnd = m_rchBuffer + cubPrinted;
	va_end( marker );
}


template< size_t cubSize >
void CStackString<cubSize>::AppendFormat( PRINTF_FORMAT_STRING const char *pchFormat, ... )
{
	va_list marker;
	va_start( marker, pchFormat );
	AppendFormatV( pchFormat, marker );
	va_end( marker );
}

template< size_t cubSize >
void CStackString<cubSize>::Append( const char *pchValue )
{
	Q_strcat( m_rchBuffer, pchValue, GetCubSize() );
	m_pchEnd = m_rchBuffer + Q_strlen(m_rchBuffer);
}

template< size_t cubSize >
void CStackString<cubSize>::SetIndent( uint32 unCount, char chIndent )
{
	Reset();
	for ( uint32 x = 0; x < unCount && x < (GetCubSize()-1); x++ )
	{
		*m_pchEnd = chIndent;
		m_pchEnd++;
	}
	*m_pchEnd = '\0';
}


#endif
