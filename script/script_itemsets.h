// 
// script/script_itemsets.h 
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved. 
// 

#ifndef SCRIPT_SCRIPT_ITEMSETS_H
#define SCRIPT_SCRIPT_ITEMSETS_H

#include "entity/extensiblebase.h"
#include "scene/RegdRefTypes.h"
#include "script/handlers/GameScriptHandler.h"
#include "script/handlers/GameScriptIds.h"

//-----------------------------------------------------------------------------

class CItemSet;
typedef	fwRegdRef<fwExtensibleBase> CItemSetRef;

//-----------------------------------------------------------------------------

class CItemSetManager
{
public:
	static void InitClass(int bufferSizeRefs);
	static void ShutdownClass();

	static CItemSetManager& GetInstance()
	{	Assert(sm_Instance);
		return *sm_Instance;
	}

	// PURPOSE:	Compact the buffer shared between the CItemSet objects to
	//			eliminate any fragmentation.
	// PARAMS:	removeUnusedElements	- True to shrink all arrays to their current use.
	// NOTES:	Primarily intended to be called internally by Allocate(), but
	//			could also be called directly if desired.
	void DefragAll(bool removeUnusedElements);

	bool Allocate(CItemSet& set, int numRefs, bool allowDefrag = true);
	void Free(CItemSet& set);

	int GetBufferSize() const
	{	return m_BufferSizeRefs;	}

protected:
	explicit CItemSetManager(int bufferSizeRefs);
	~CItemSetManager();

	// PURPOSE:	Array of pointers to all CItemSet objects that have allocated
	//			memory from our buffer. This is used both for finding free space
	//			in the buffer for new allocations, and for defragmenting the array
	//			if needed. The CItemSet objects in this array are sorted in the
	//			order in which they use memory in the buffer.
	atArray<CItemSet*>			m_ItemSetsSortedByAlloc;

	// PURPOSE:	Buffer used by all CItemSet objects for storing references to
	//			the set members.
	CItemSetRef*				m_Buffer;

	// PURPOSE:	The number of CItemSetRef objects in m_Buffer.
	unsigned int				m_BufferSizeRefs;

	// PURPOSE:	Instance pointer.
	static CItemSetManager*	sm_Instance;
};

//-----------------------------------------------------------------------------

class CItemSet : public fwExtensibleBase
{
	DECLARE_RTTI_DERIVED_CLASS(CItemSet, fwExtensibleBase);

public:
	FW_REGISTER_CLASS_POOL(CItemSet);

	explicit CItemSet(bool autoClean);
	virtual ~CItemSet();

	bool AddEntityToSet(fwExtensibleBase& obj);
	bool RemoveEntityFromSet(fwExtensibleBase& obj);

	void Clean();

	int GetSetSize(bool bAutoClean = true)
	{
		if(bAutoClean && m_AutoClean)
		{
			Clean();
		}
		return m_NumEntities;
	}

	fwExtensibleBase* GetEntity(int index) const
	{	FastAssert(index >= 0 && index < m_NumEntities);
		return m_EntityArray[index];
	}

	bool IsEntityInSet(const fwExtensibleBase& obj) const
	{	return FindInSet(obj) >= 0;	}

protected:
	friend class CItemSetManager;

	// Not allowed to copy construct or assign
	CItemSet(const CItemSet&);
	const CItemSet& operator=(const CItemSet&);

	int FindInSet(const fwExtensibleBase& obj) const;

	// Only used for temporary space, so should be cheap to increase if needed.
	static const int kMaxRefsPerSet = 256;

	CItemSetRef*			m_EntityArray;
	int						m_MaxEntities;
	int						m_NumEntities;
	bool					m_AutoClean;
};

//-----------------------------------------------------------------------------

#endif // SCRIPT_SCRIPT_ITEMSETS_H

// End of file script/script_itemsets.h
