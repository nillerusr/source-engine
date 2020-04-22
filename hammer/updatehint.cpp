//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: An object that is used, when modifying the state of the document,
//			to collect information about what objects changed and how they changed.
//			This aggregate info is then passed to CMapDoc::UpdateObjects which performs
//			post processing and view updates.
//
// $NoKeywords: $
//=============================================================================//


//-----------------------------------------------------------------------------
// Purpose: Iterates the list of updated objects.
//-----------------------------------------------------------------------------
POSITION CUpdateHint::GetHeadPosition(int nIndex)
{
	return(m_NotifyList[nIndex].Objects.GetHeadPosition());
}


//-----------------------------------------------------------------------------
// Purpose: Iterates the list of updated objects.
//-----------------------------------------------------------------------------
CMapClass *CUpdateHint::GetNext(POSITION &pos)
{
	return(m_NotifyList[nIndex].Objects.GetNext(pos));
}


//-----------------------------------------------------------------------------
// Purpose: Returns the notification code for this update.
//-----------------------------------------------------------------------------
int CUpdateHint::GetNotifyCode(void)
{
	return(m_NotifyList[nIndex].nCode);
}


//-----------------------------------------------------------------------------
// Purpose: Returns the current update region.
//-----------------------------------------------------------------------------
BoundBox const &CUpdateHint::GetUpdateRegion(void)
{
	return(m_UpdateRegion);
}


//-----------------------------------------------------------------------------
// Purpose: Prepares to update an object.
// Input  : pObject - Object that will be updated.
//-----------------------------------------------------------------------------
void CUpdateHint::PreUpdateObject(CMapClass *pObject)
{
	if (pObject != NULL)
	{
		CMapObjectList TempList;
		TempList.AddTail(pObject);
		PreUpdateObjects(&TempList);
	}
	else
	{
		PreUpdateObjects(NULL);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Prepares to update the list of objects.
// Input  : pObjects - List of objects, NULL if none.
//-----------------------------------------------------------------------------
void CUpdateHint::PreUpdateObjects(CMapObjectList *pObjects)
{
	if (pObjects != NULL)
	{
		POSITION pos = pObjects->GetHeadPosition();
		while (pos != NULL)
		{
			CMapClass *pObject = pObjects->GetNext(pos);
			if (pObject != NULL)
			{
				m_UpdateRegion.UpdateBounds(pObject);
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Prepares to update an object.
// Input  : pObject - Object that will be updated.
//-----------------------------------------------------------------------------
void CUpdateHint::PostUpdateObject(CMapClass *pObject, int nNotifyCode)
{
	if (pObject != NULL)
	{
		CMapObjectList TempList;
		TempList.AddTail(pObject);
		PostUpdateObjects(&TempList, nNotifyCode);
	}
	else
	{
		PostUpdateObjects(NULL, nNotifyCode);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Prepares to update the list of objects.
// Input  : pObjects - List of objects, NULL if none.
//-----------------------------------------------------------------------------
void CUpdateHint::PostUpdateObjects(CMapObjectList *pObjects, int nNotifyCode)
{
	int nIndex = 0;
	bool bFound = false;
		
	while (!bFound && (nIndex < m_ListEntries))
	{
		if (m_NotifyList[nIndex].nCode == nNotifyCode)
		{
			bFound = true;
		}
		else
		{
			nIndex++;
		}
	}

	if ((!bFound && (nIndex < MAX_NOTIFY_CODES))
	{
		if (nIndex < MAX_NOTIFY_CODES)
		{
			m_ListEntries++;
		}
	}
	else
	{
		ASSERT(nIndex < MAX_NOTIFY_CODES);
		return;
	}

	m_NotifyList[nIndex].Objects.AddTail(pObjects);

	if (pObjects != NULL)
	{
		POSITION pos = pObjects->GetHeadPosition();
		while (pos != NULL)
		{
			CMapClass *pObject = pObjects->GetNext(pos);
			if (pObject != NULL)
			{
				m_UpdateRegion.UpdateBounds(pObject);
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CUpdateHint::Reset(void)
{
	m_Objects.RemoveAll();
	m_UpdateRegion.ResetBounds();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CUpdateHint::UpdateBounds(BoundBox &bbox)
{
	m_UpdateRegion.UpdateBounds(&bbox);
}
