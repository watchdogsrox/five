
// name:		LinkList.h
// description:	Template link list
// written by:	Adam Fowler
//
#ifndef INC_LINKLIST_H_
#define INC_LINKLIST_H_

#include "debug\debug.h"

template<class T> class CLinkList;

//
// name:		CLink
// description:	Template class describing a link in a link list
//
template<class T>
class CLink
{
	friend class CLinkList<T>;
public:
	void Insert(CLink<T>* pLink);
	void Remove();
	
	CLink* GetPrevious() const {return m_pPrev;}
	CLink* GetNext() const {return m_pNext;}

	T item;
	
private:
	CLink* m_pPrev;
	CLink* m_pNext;
};

//
// name:		CLinkList
// description:	Template class describing a link list with a maximum number
//				of elements
//
template<class T>
class CLinkList
{
public:
	CLinkList() {m_pStore = NULL; m_bLock = false;}
	void Init(s32 size);
	void Shutdown();
	void Clear();
	
	CLink<T>* Insert();
	CLink<T>* Insert(const T& item);
	CLink<T>* InsertSorted(const T& item);
	void Remove(CLink<T>* pLink);
	CLink<T>* GetFirst() {return &m_firstLink;}
	CLink<T>* GetLast() {return &m_lastLink;}
	const CLink<T>* GetFirst() const {return &m_firstLink;}
	const CLink<T>* GetLast() const {return &m_lastLink;}
	
	s32 GetNumFree() const;
	s32 GetNumUsed() const;

	void Lock() {m_bLock = true;}
	void Unlock() {m_bLock = false;}

#if __DEV
	void CheckIntegrity();
#endif

private:
	CLink<T> m_firstLink;
	CLink<T> m_lastLink;
	CLink<T> m_firstFreeLink;
	CLink<T> m_lastFreeLink;
	
	CLink<T>* m_pStore;

	bool	m_bLock;
};

//
// name:		CLink::Insert
// description:	Insert a link
//
template<class T>
void CLink<T>::Insert(CLink<T>* pLink)
{
	pLink->m_pNext = m_pNext;
	m_pNext->m_pPrev = pLink;
	pLink->m_pPrev = this;
	m_pNext = pLink;
}
//
// name:		CLink::Remove
// description:	Remove a link
//
template<class T>
void CLink<T>::Remove()
{
	m_pNext->m_pPrev = m_pPrev;
	m_pPrev->m_pNext = m_pNext;
}


//
// name:		CLinkList::Init
// description:	Initialises link list
template<class T> void CLinkList<T>::Init(s32 size)
{
	Assert(m_pStore == NULL);
	m_pStore = rage_new CLink<T>[size];
	m_firstLink.m_pNext = &m_lastLink;
	m_lastLink.m_pPrev = &m_firstLink;
	m_firstFreeLink.m_pNext = &m_lastFreeLink;
	m_lastFreeLink.m_pPrev = &m_firstFreeLink;
	
	while(size--)
	{
		m_firstFreeLink.Insert(m_pStore+size);
	}
}

//
// name:		CLinkList::Shutdown
// description:	Remove linklist
template<class T> void CLinkList<T>::Shutdown()
{
	Assert(m_bLock == false);

	delete[] m_pStore;
	m_pStore = NULL;
}

//
// name:		CLinkList<T>::Clear
// description:	Free all the links in the link list
template<class T> void CLinkList<T>::Clear()
{
	Assert(m_bLock == false);

	CLink<T>* pLink = m_firstLink.m_pNext;
	while(pLink != &m_lastLink)
	{
		Remove(m_firstLink.m_pNext);
		pLink = m_firstLink.m_pNext;
	}
}

//
// name:		CLinkList<T>::Insert
// description:	Insert a new link into the link list without setting its contents
template<class T> CLink<T>* CLinkList<T>::Insert()
{
	//insert link in here
	CLink<T>* pNewLink = m_firstFreeLink.m_pNext;
	if(pNewLink != &m_lastFreeLink)
	{
		pNewLink->Remove();
		m_firstLink.Insert(pNewLink);
		return pNewLink;
	}
	return NULL;	
}


//
// name:		CLinkList<T>::Insert
// description:	Insert a new item into the link list
template<class T> CLink<T>* CLinkList<T>::Insert(const T& item)
{
	Assert(m_bLock == false);

	//insert link in here
	CLink<T>* pNewLink = m_firstFreeLink.m_pNext;
	if(pNewLink != &m_lastFreeLink)
	{
		pNewLink->item = item;
		pNewLink->Remove();
		m_firstLink.Insert(pNewLink);
		return pNewLink;
	}
	return NULL;	
}


//
// name:		CLinkList<T>::InsertSorted
// description:	Insert a new item into a sorted list
template<class T> CLink<T>* CLinkList<T>::InsertSorted(const T& item)
{
	Assert(m_bLock == false);

	CLink<T>* pLink = m_firstLink.m_pNext;
	
	while(pLink != &m_lastLink)
	{
		if(pLink->item >= item)
		{
			break;
		}
		pLink = pLink->m_pNext;
	}	

	//insert link in here
	CLink<T>* pNewLink = m_firstFreeLink.m_pNext;
	if(pNewLink != &m_lastFreeLink)
	{
		pNewLink->item = item;
		pNewLink->Remove();
		pLink->m_pPrev->Insert(pNewLink);
		return pNewLink;
	}
	return NULL;
}


//
// name:		CLinkList<T>::Remove
// description:	Remove a link from the link list
template<class T> void CLinkList<T>::Remove(CLink<T>* pLink)
{
	Assert(m_bLock == false);

	//remove link from allocated list and add back into free list
	pLink->Remove();
	m_firstFreeLink.Insert(pLink);
}

//
// name:		CLinkList<T>::GetNumFree
// description:	Return how many links are free
template<class T> s32 CLinkList<T>::GetNumFree() const
{
	CLink<T>* pLink = m_firstFreeLink.m_pNext;
	s32 count = 0;
	
	while(pLink != &m_lastFreeLink)
	{
		pLink = pLink->m_pNext;
		count++;
	}	
	return count;	
}

//
// name:		CLinkList<T>::GetNumUsed
// description:	Return how many links are used
template<class T> s32 CLinkList<T>::GetNumUsed() const
{
	CLink<T>* pLink = m_firstLink.m_pNext;
	s32 count = 0;
	
	while(pLink != &m_lastLink)
	{
		pLink = pLink->m_pNext;
		count++;
	}	
	return count;	
}

//
// name:		CLinkList<T>::CheckIntegrity
// description:	Check that link list is still doubly linked
#if __DEV
template<class T> void CLinkList<T>::CheckIntegrity()
{
	CLink<T>* pLink = m_firstLink.m_pNext;
	
	while(pLink != &m_lastLink)
	{
		Assert(pLink->m_pPrev->m_pNext == pLink);
		Assert(pLink->m_pNext->m_pPrev == pLink);
		pLink = pLink->m_pNext;
	}	
}
#endif //__DEV...

#endif //INC_LINKLIST_H_
