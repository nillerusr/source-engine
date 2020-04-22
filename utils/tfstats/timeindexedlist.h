//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Interface AND implementation of CTimeIndexedList
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#ifndef TIMEINDEXEDLIST_H
#define TIMEINDEXEDLIST_H
#ifdef WIN32
#pragma once
#endif
#include <list>
#include "TFStatsApplication.h"
#include "util.h"
//------------------------------------------------------------------------------------------------------
// Purpose: CTimedIndexedList is a list of elements indexed by time_t's.
// the elements of the list are meant to represent a state that endures over time
// and when it is switched to another state, that is represented by an element in this
// list with the value of the new state at index t, where t is the time that the switch
// occured.
// in TFStats, this is used for player names and player classes
//------------------------------------------------------------------------------------------------------
template <class T>
class CTimeIndexedList
{
public:
	static time_t PENDING;
	typedef struct{T data;time_t start; time_t end;} listtype;
	typedef std::list<listtype>::iterator iterator;
	typedef std::map<T,time_t>::iterator accumulatorIterator;
	typedef std::map<T,time_t> accumulator;
	typedef std::list<listtype> timelist;
	
	timelist theList;
	time_t endTime;
	bool anythingAtTime(time_t);
	T atTime(time_t,T errorvalue=T());
	T favourite(T errorvalue=T());
	CTimeIndexedList();
	void add(time_t a,T b);
	bool contains(const T& t);

	accumulator accumulate();
	
	time_t howLong(const T& t);
	iterator begin();
	iterator end();
	int size();
	int numDifferent();

	void cut(time_t t); 
	void remove(T b);
};
template <class T>
time_t CTimeIndexedList<T>::PENDING=-1;

template <class T>
CTimeIndexedList<T>::CTimeIndexedList()
{
	
	endTime=0;
	
}

//------------------------------------------------------------------------------------------------------
// Function:	CTimeIndexedList<T>::accumulate
// Purpose:	
// Output:	CTimeIndexedList<T>::accumulator
//------------------------------------------------------------------------------------------------------
template<class T>
CTimeIndexedList<T>::accumulator CTimeIndexedList<T>::accumulate()
{
	CTimeIndexedList<T>::accumulator accum; //maps from a value of T, to the time that value was played
	
/*
	//iterate through for debugging purposes
	{

		CTimeIndexedList<T>::iterator it=theList.begin();
		for (it;it!=theList.end();++it)
		{
			CTimeIndexedList<T>::listtype lt=*it;
		}
	}
*/	
	CTimeIndexedList<T>::iterator it=theList.begin();	
	for (it;it!=theList.end();++it)
	{
		time_t start=it->start;
		time_t end=it->end;
		T& bucket=it->data;
		if (end==PENDING)
		{
			end=endTime;
			if (end==0)
			{
				g_pApp->fatalError("Time flow error, make sure your log file\n");
			}
		}
		accum[bucket]+=(end-start);
	}

	return accum;
}

//------------------------------------------------------------------------------------------------------
// Function:	CTimeIndexedList<T>::add
// Purpose:	
// Input:	a - 
//				b - 
//------------------------------------------------------------------------------------------------------
template<class T>
void CTimeIndexedList<T>::add(time_t a,T b)
{
	bool lastIsB=false;
	bool firstElement=false;
	listtype insertme;
	insertme.data=b;
	insertme.start=a;
	insertme.end=PENDING;

	if (!theList.empty())
	{
		//find the last element and fix up its end-time (if it's pending)
		CTimeIndexedList<T>::iterator last=theList.end();
		last--;
		listtype lt=*last;

		if (last->end==PENDING && last->data!=b)
			last->end=insertme.start;
	}
	
	//add the new element!
	theList.push_back(insertme);
	
	if (a > endTime)
		endTime=a;

	

}
//------------------------------------------------------------------------------------------------------
// Function:	CTimeIndexedList::atTime
// Purpose:	returns the element whose time index is the closest to and less than or
//	equal to the queried time
// Input:	tm - the time that is being queried
// Output:	T, the element
//------------------------------------------------------------------------------------------------------
template <class T>
T CTimeIndexedList<T>::atTime(time_t tm,T errorvalue)
{
	CTimeIndexedList<T>::iterator it;
	if (theList.empty())
		return errorvalue;

	//nothing exists here!
	if (tm < theList.begin()->start)
		return errorvalue;
	
	for (it=theList.begin();it!=theList.end();++it)
	{
		time_t mark=it->start;
		time_t markend=it->end;

		if (tm >= mark && tm < markend)
			return it->data;
		if (tm >= mark && markend == PENDING)
			return it->data; 
	}

	return errorvalue;
}

//------------------------------------------------------------------------------------------------------
// Function:	CTimeIndexedList::favourite
// Purpose:	returns the element that spans the most time
// Output:	T, the element
//------------------------------------------------------------------------------------------------------
template <class T>
T CTimeIndexedList<T>::favourite(T errorvalue)
{
	if (theList.empty())
		return errorvalue;

	CTimeIndexedList<T>::accumulator accum=accumulate();

	//now scan that intermediate map, and determine the element with the longest duration
	CTimeIndexedList<T>::accumulatorIterator accumiter=accum.begin();

	time_t maxtime=0;
	CTimeIndexedList<T>::accumulatorIterator fave=accumiter;

	T t= fave->first;
	for (accumiter;accumiter!=accum.end();++accumiter)
	{
		t= fave->first;
		time_t clicksPlayed=accumiter->second;
		
		if (clicksPlayed> maxtime)
		{
			maxtime=clicksPlayed;
			fave=accumiter;
		}
	}
	
	T ttt= fave->first;

	return ttt;
}

//------------------------------------------------------------------------------------------------------
// Function:	CTimeIndexedList::contains
// Purpose:	returns true if the element is in the list
// Input:	t, the element value that we're querying
// Output:	Returns true if the element was in the list
//------------------------------------------------------------------------------------------------------
template <class T>
bool CTimeIndexedList<T>::contains(const T& t)
{
	CTimeIndexedList<T>::iterator it=theList.begin();
	for (it=theList.begin();it!=theList.end();++it)
	{
		if (it->data==t)
			return true;
	}
	return false;
}

template <class T>
time_t CTimeIndexedList<T>::howLong(const T& t)
{
	if (theList.begin()==theList.end())
		return 0;

	CTimeIndexedList<T>::accumulator accum=accumulate(); //maps from a value of T, to the time that value was played
	
	return accum[t];
}

template <class T>
CTimeIndexedList<T>::iterator CTimeIndexedList<T>::begin()
{
	return theList.begin();
}

template <class T>
CTimeIndexedList<T>::iterator CTimeIndexedList<T>::end()
{
	return theList.end();
}

template <class T>
int CTimeIndexedList<T>::size()
{
	return theList.size();
}


template <class T>
int CTimeIndexedList<T>::numDifferent()
{
	if (theList.begin()==theList.end())
		return 0;

	 //maps from a value of T, to the time that value was played
	CTimeIndexedList<T>::accumulator accum=accumulate();

	return accum.size();
}


template<class T>
void CTimeIndexedList<T>::cut(time_t t)
{
	if (t > endTime)
		endTime=t;
	if (!theList.empty())
	{
		//find the last element and fix up its end-time (if it's pending)
		CTimeIndexedList<T>::iterator last=theList.end();
		last--;

		if (last->end==PENDING)
			last->end=t;
	}
}


template<class T>
void CTimeIndexedList<T>::remove(T b)
{
	CTimeIndexedList<T>::iterator it=theList.begin();
	
	while(it!=theList.end())
	{
		if (it->data==b)
			it=theList.erase(it);
		else
			++it;
	}
}




template <class T>
bool CTimeIndexedList<T>::anythingAtTime(time_t tm)
{
/*
	//iterate through for debugging purposes
	{

		CTimeIndexedList<T>::iterator it=theList.begin();
		for (it;it!=theList.end();++it)
		{
			CTimeIndexedList<T>::listtype lt=*it;
		}
	}
	*/
	CTimeIndexedList<T>::iterator it;
	if (theList.empty())
		return false;

	//nothing exists here!
	if (tm < theList.begin()->start)
		return false;
	
	for (it=theList.begin();it!=theList.end();++it)
	{
		time_t mark=it->start;
		time_t markend=it->end;

		if (tm >= mark && tm < markend)
			return true;
	}

	return false;
}

#endif // TIMEINDEXEDLIST_H