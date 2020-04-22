//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef LOWPASSSTREAM_H
#define LOWPASSSTREAM_H
#ifdef _WIN32
#pragma once
#endif


// ------------------------------------------------------------------ //
// Default implementations of the operators used for averaging.
// ------------------------------------------------------------------ //
template< class DataType >
inline void SetToZero( DataType &data )
{
	data = 0;
}

template< class DataType >
inline void AddSamples( DataType const &d1, DataType const &d2, DataType &out )
{
	out = d1 + d2;
}

template< class DataType >
inline DataType DivideSample( DataType const &data, int factor )
{
	return data / factor;
}


// This class does a simple low pass filter on data that you pass through it.
// You tell it the number of samples to hang onto and it will average that number
// of samples as you feed it data.
template< class DataType, int nSamples >
class CLowPassStream
{
public:
				CLowPassStream();

	// Add a sample to the list.
	void		AddSample( DataType const &data );

	// Get the current average.
	DataType	GetCurrentAverage();

private:
	
	DataType	m_Samples[nSamples];
	int			m_iOutSample;
	int			m_nSamplesGotten;	// incremented until it reaches nSamples.
									// helps avoid the startup transient.
};


template< class DataType, int nSamples >
inline CLowPassStream<DataType, nSamples>::CLowPassStream()
{
	for( int i=0; i < nSamples; i++ )
		SetToZero( m_Samples[i] );

	m_iOutSample = 0;
	m_nSamplesGotten = 0;
}

template< class DataType, int nSamples >
inline void CLowPassStream<DataType, nSamples>::AddSample( DataType const &data )
{
	m_Samples[m_iOutSample] = data;
	
	++m_nSamplesGotten;
	if( m_nSamplesGotten >= nSamples )
		m_nSamplesGotten = nSamples;

	++m_iOutSample;
	if( m_iOutSample >= nSamples )
		m_iOutSample = 0;
}

template< class DataType, int nSamples >
inline DataType CLowPassStream<DataType, nSamples>::GetCurrentAverage()
{
	DataType data;

	SetToZero( data );
	for( int i=0; i < nSamples; i++ )
		AddSamples( data, m_Samples[i], data );

	return DivideSample( data, m_nSamplesGotten );
}


#endif // LOWPASSSTREAM_H
