#ifndef PCH_PRECOMPILED

//
//
//    Filename: Store.h
//     Creator: Adam Fowler
//     $Author: $
//       $Date: $
//   $Revision: $
// Description: Template class for array of objects
//
//
#ifndef INC_STATIC_STORE_H_
#define INC_STATIC_STORE_H_

#include "debug\debug.h"

//
// Template class that stores an array of objects. Objects can only be allocated. All objects are deallocated when 
// Init() is called. The template parameters are the class in the Store and 
// the number of elements in the store e.g. CStore<float, 24> is an array of 24 floats
//
template<class _T> class CStaticStore
{
public:
	CStaticStore(s32 size) : m_size(size), m_storeArray(NULL) {}

	// initialise class
	void Init() {Assert(m_storeArray == NULL); m_nextItem = 0; m_storeArray = rage_new _T[m_size];}
	void Shutdown() { delete[] m_storeArray; m_storeArray = NULL;}

	// Get size allocated
	s32 GetSize() {return m_size;}
	// allocate one item
	_T* GetNextItem() {
#if !__FINAL	
		if (!(m_nextItem < m_size)) {
			Quitf("Size of Store:%d needs increasing\n", m_size);
		}
#endif		
		return &m_storeArray[m_nextItem++];
	}
	// get allocated item from an index
	inline _T& operator[](s32 i) {Assert(i < m_nextItem); return m_storeArray[i];}
	inline const _T& operator[](s32 i) const {Assert(i < m_nextItem); return m_storeArray[i];}
	// get number of items allocated
	s32 GetItemsUsed() const {return m_nextItem;}
	void SetItemsUsed(s32 num) {m_nextItem = num;}

	// call a function for all items in store that are used
	void ForAllItemsUsed(void (_T::*func)());
	// call a function for all items in the store
	void ForAllItems(void (_T::*func)());
	// Returns the pointer to the first element
	_T *FirstElement(void) {return m_storeArray;}
	// return index of element
	s32 GetElementIndex(_T* pElement) {s32 index = pElement-m_storeArray; Assertf(index >=0 && index < m_nextItem, "Try to access element outside store"); return index;}

private:
	s32 m_size;
	s32 m_nextItem;
	_T* m_storeArray;
};

template<class _T> void CStaticStore<_T>::ForAllItemsUsed(void (_T::*func)()) 
{
	for(s32 i=0; i<m_nextItem; i++) 
		(m_storeArray[i].*func)();
}

template<class _T> void CStaticStore<_T>::ForAllItems(void (_T::*func)()) 
{
	for(s32 i=0; i<m_size; i++) 
		(m_storeArray[i].*func)();
}

template<class _T> class CStaticStore<_T*>
{
public:
	CStaticStore(s32 size) : m_size(size), m_storeArray(NULL) {}

	// initialise class
	void Init() {Assert(m_storeArray == NULL); m_nextItem = 0; m_storeArray = rage_new _T*[m_size];}
	void Shutdown() { delete[] m_storeArray; m_storeArray = NULL;}

	// Get size allocated
	s32 GetSize() {return m_size;}
	// allocate one item
	_T** GetNextItem() {
#if !__FINAL	
		if (!(m_nextItem < m_size)) {
			Quitf("Size of Store:%d needs increasing\n", m_size);
		}
#endif		
		return &m_storeArray[m_nextItem++];
	}
	// get allocated item from an index
	inline _T*& operator[](s32 i) {Assert(i < m_nextItem); return m_storeArray[i];}
	inline const _T*& operator[](s32 i) const {Assert(i < m_nextItem); return m_storeArray[i];}
	// get number of items allocated
	s32 GetItemsUsed() const {return m_nextItem;}
	void SetItemsUsed(s32 num) {m_nextItem = num;}

	// call a function for all items in store that are used
	void ForAllItemsUsed(void (_T::*func)());
	// call a function for all items in the store
	void ForAllItems(void (_T::*func)());
	// Returns the pointer to the first element
	_T **FirstElement(void) {return m_storeArray;}


private:
	s32 m_size;
	s32 m_nextItem;
	_T** m_storeArray;
};

template<class _T> void CStaticStore<_T*>::ForAllItemsUsed(void (_T::*func)()) 
{
	for(s32 i=0; i<m_nextItem; i++) 
		((*m_storeArray[i]).*func)();
}

template<class _T> void CStaticStore<_T*>::ForAllItems(void (_T::*func)()) 
{
	for(s32 i=0; i<m_size; i++) 
		((*m_storeArray[i]).*func)();
}

#endif // INC_STORE_H_

#endif
