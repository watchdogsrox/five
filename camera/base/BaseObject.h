//
// camera/base/BaseObject.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef BASE_OBJECT_H
#define BASE_OBJECT_H

#include "fwtl/regdrefs.h"
#include "fwutil/rtti.h"
#include "system/stack_collector.h"

namespace rage
{
	class bkGroup;
}

class camBaseObjectMetadata;

#define SAFE_CSTRING(_CStr) (_CStr ? _CStr : "UNKNOWN")

//A base camera object class that forms the basis of all data driven camera classes.
//NOTE: We inherit from CRefAwareBase in order to use safe camera references.
class camBaseObject : public fwRefAwareBase
{
	DECLARE_RTTI_BASE_CLASS(camBaseObject);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	camBaseObject(const camBaseObjectMetadata& metadata);

public:
	virtual ~camBaseObject();

	u32				GetNameHash() const;

	const camBaseObjectMetadata& GetMetadata() const { return m_Metadata; }

#if __BANK
	virtual void	AddWidgetsForInstance() {}
#endif // __BANK

protected:
	const camBaseObjectMetadata& m_Metadata;

#if __BANK
	bkGroup*		m_WidgetGroup;	//The bank group containing instance-specific RAG widgets for this instance, or NULL.
#endif // __BANK

#if !__NO_OUTPUT
public:
	//NOTE: We should not need to access the name string in !__NO_OUTPUT builds and we may wish to remove the string in these builds in the future.
	const char*		GetName() const;
#if !__FINAL
	void PrintCollectedObjectStack(atString* outputString = nullptr) const;
	void CollectCreateObjectStack();
	void UnregisterObjectStack();

private:
	static rage::sysStackCollector s_CameraCreateObjectStackCollector;
	static rage::atString* s_CurrentObjectStackString;

	static void DisplayStackLine(size_t addr,const char* sym,size_t displacement);
	static void PrintStackInfo(u32 tagCount, u32 stackCount, const size_t* stack, u32 stackSize);

	u32 CalcObjectStackKey() const 
	{ 
		return (u32)(size_t)this; 
	}
#endif // !__FINAL
#endif //!__NO_OUTPUT

private:
	//Forbid copy construction and assignment.
	camBaseObject(const camBaseObject& other);
	camBaseObject& operator=(const camBaseObject& other);
};

#endif // BASE_OBJECT_H
