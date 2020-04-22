//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef IVOICERECORD_H
#define IVOICERECORD_H
#pragma once


// This is the voice recording interface. It provides 16-bit signed mono data from 
// a mic at some sample rate.
abstract_class IVoiceRecord
{
protected:

	virtual				~IVoiceRecord() {}


public:
	
	// Use this to delete the object.
	virtual void		Release()=0;

	// Start/stop capturing.
	virtual bool		RecordStart() = 0;
	virtual void		RecordStop() = 0;

	// Idle processing.
	virtual void		Idle()=0;

	// Get the most recent N samples. If nSamplesWanted is less than the number of
	// available samples, it discards the first samples and gives you the last ones.
	virtual int			GetRecordedData(short *pOut, int nSamplesWanted)=0;
};


#endif // IVOICERECORD_H
