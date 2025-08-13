//
// weapons/iteminfo.h
//
// Copyright (C) 1999-2014 Rockstar North.  All Rights Reserved. 
//

#ifndef ITEM_INFO_H
#define ITEM_INFO_H

// Rage headers
#include "parser/macros.h"

// Framework headers
#include "fwtl/pool.h"
#include "fwtl/regdrefs.h"
#include "fwutil/rtti.h"

// Forward declarations
class CBaseModelInfo;

////////////////////////////////////////////////////////////////////////////////

class CItemInfo : public fwRefAwareBase
{
	DECLARE_RTTI_BASE_CLASS(CItemInfo);
public:

	// Construction
	CItemInfo();
	CItemInfo(u32 uNameHash);

	// Destruction
	virtual ~CItemInfo();

	//
	// Accessors
	//

	// Get the item hash
	u32 GetHash() const { return m_Name.GetHash(); }

	// Get the model hash
	virtual u32 GetModelHash() const { return m_Model.GetHash(); }

	// Get the audio hash
	u32 GetAudioHash() const { return m_Audio.GetHash(); }

	// Get the slot hash
	u32 GetSlot() const { return m_Slot.GetHash(); }

	//
	// Model querying
	//

	// Get the model index from the model hash
	u32 GetModelIndex() const;
	u32 GetModelIndex();

	// Get the model info from the model hash
	const CBaseModelInfo* GetModelInfo() const;
	CBaseModelInfo* GetModelInfo();

	// Query the streaming system to check if the model is in memory yet
	bool GetIsModelStreamedIn() const;

	// Function to sort the array so we can use a binary search
	static s32 PairSort(CItemInfo* const* a, CItemInfo* const* b) { return ((*a)->m_Name.GetHash() < (*b)->m_Name.GetHash() ? -1 : 1); }

#if !__FINAL
	// Validate the data structure after loading
	virtual bool Validate() const;
#endif

#if !__NO_OUTPUT
	// Get the name
	const char* GetName() const { return m_Name.TryGetCStr(); }

	// Get the model name
	const char* GetModelName() const { return m_Model.TryGetCStr(); }
#endif

private:

	// Called after the data has been loaded
	void OnPostLoad();

	//
	// Members
	//

	// The hash of the name
	atHashWithStringNotFinal m_Name;

	// The model hash
	atHashWithStringNotFinal m_Model;

	// The audio hash
	atHashWithStringNotFinal m_Audio;

	// The slot hash
	atHashWithStringNotFinal m_Slot;

	PAR_PARSABLE;

private:
	// Not allowed to copy construct or assign
	CItemInfo(const CItemInfo& other);
	CItemInfo& operator=(const CItemInfo& other);
};

// Regd pointer typedef
typedef	fwRegdRef<const CItemInfo> RegdItemInfo;

#endif // ITEM_INFO_H
