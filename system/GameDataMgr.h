#ifndef GAME_DATA_MGR_H
#define GAME_DATA_MGR_H

// Rage headers
#include "atl/array.h"
#include "audioengine/metadatamanager.h"

//////////////////////////////////////////////////////////////////////////
// GameDataMgr.h
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// CGameDataMgr
//////////////////////////////////////////////////////////////////////////

class CGameDataMgr
{
public:

	// Initialise
	void Init(const char* metadataType, const char* metadataPath, u32 uSchemaVersion)
	{
		AssertVerify(m_metadataMgr.Init(metadataType, metadataPath, !__FINAL, uSchemaVersion));
	}

	// Shutdown
	void Shutdown()
	{
		m_metadataMgr.Shutdown();
	}

	// Access the rave data directly
	const audMetadataManager& GetDataMgr() const { return m_metadataMgr; }

	// Get rave structures directly from the metadata manager
	template<typename _Data>
	const _Data* GetData(u32 uNameHash) const
	{
		return m_metadataMgr.GetObject<_Data>(uNameHash);
	}

	// Get untyped rave structures directly from the metadata manager
	// Could be unsafe if you are not sure of the objects type
	template<typename _Data>
	const _Data* GetDataUntyped(u32 uNameHash) const
	{
		return static_cast<const _Data*>(m_metadataMgr.GetObjectMetadataPtr(uNameHash));
	}

	// Get game data from the metadata manager and return it as the specified wrapper class
	// The wrapper class must be derived off the automatically generated game data, preferably using private inheritance
	// The wrapper class must not implement any member variables or this will crash
	template<typename _Wrapper, typename _Data>
	const _Wrapper* GetData(u32 uNameHash) const
	{
#if __ASSERT
		s32 iWrapperSize = sizeof(_Wrapper);
		s32 iDataSize = sizeof(_Data);
		FastAssert(iWrapperSize == iDataSize);
#endif // __ASSERT
		return reinterpret_cast<const _Wrapper*>(m_metadataMgr.GetObject<_Data>(uNameHash));
	}

private:

	//
	// Members
	//

	// Rave metadata manager
	audMetadataManager m_metadataMgr;
};

//////////////////////////////////////////////////////////////////////////
// CGameDataWrapper
// Class for wrapping Rave data with a game side class
//////////////////////////////////////////////////////////////////////////

template<typename _Wrapper, typename _Data>
class CGameDataWrapper
{
public:

	typedef _Wrapper* (*CGameDataWrapperCreateFn)(const _Data*, u32);

	CGameDataWrapper(s32 iMaxWrappers)
		: m_wrappers(0, iMaxWrappers)
	{
	}

	~CGameDataWrapper()
	{
		Free();
	}

	void Free()
	{
		for(s32 i = 0; i < m_wrappers.GetCount(); i++)
		{
			if(m_wrappers[i].pWrapper)
			{
				delete m_wrappers[i].pWrapper;
				m_wrappers[i].pWrapper = NULL;
			}
		}

		m_wrappers.ResetCount();
	}

	_Wrapper* GetData(u32 uNameHash, const CGameDataMgr& dataMgr, CGameDataWrapperCreateFn createFn = NULL)
	{
		s32 iIndex = m_wrappers.BinarySearch(sWrapperData(uNameHash));
		if(iIndex != -1)
		{
			return m_wrappers[iIndex].pWrapper;
		}

		// Does not exist in current data - attempt to look it up from metadata
		const _Data* pData = dataMgr.GetDataUntyped<_Data>(uNameHash);
		if(pData)
		{
			if(m_wrappers.GetCount() == m_wrappers.GetCapacity())
			{
				Assertf(0, "CGameDataWrapper: m_wrappers storage is full - capacity is [%d]\n", m_wrappers.GetCapacity());
				return NULL;
			}

			_Wrapper* pWrapper = NULL;

			if(createFn)
			{
				pWrapper = createFn(pData, uNameHash);
			}
			else
			{
				pWrapper = rage_new _Wrapper(pData);
			}

			// Add the new wrapper object to the storage
			m_wrappers.Push(sWrapperData(uNameHash, pWrapper));

			// Sort the array for binary search
			m_wrappers.QSort(0, m_wrappers.GetCount(), PairSort);

			// Return a pointer to the data
			return pWrapper;
		}

		// No entry found
		Assertf(0, "CGameDataWrapper: No data found for hash key [%d]\n", uNameHash);
		return NULL;
	}

private:

	struct sWrapperData
	{
		sWrapperData() : key(0), pWrapper(NULL) {}
		sWrapperData(u32 key, _Wrapper* pWrapper = NULL) : key(key), pWrapper(pWrapper) {}
		sWrapperData(const sWrapperData& other) : key(other.key), pWrapper(other.pWrapper) {}

		//
		// Members
		//

		// Key
		u32 key;

		// Wrapper
		_Wrapper* pWrapper;

		// Operators for binary search
		bool operator< (const sWrapperData& other) const { return key < other.key; }
		bool operator==(const sWrapperData& other) const { return key == other.key; }
		void operator= (const sWrapperData& other) { key = other.key; pWrapper = other.pWrapper; }
	};

	// Function to sort the array so we can use a binary search
	static s32 PairSort(const sWrapperData* a, const sWrapperData* b) { return (a->key < b->key ? -1 : 1); }

	// Data storage
	atArray<sWrapperData> m_wrappers;
};

#endif // GAME_DATA_MGR_H
