//
// camera/base/BaseObject.cpp
// 
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//

#include "camera/base/BaseObject.h"

#include "atl/string.h"
#include "bank/group.h"

#include "camera/camera_channel.h"
#include "camera/system/CameraMetadata.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camBaseObject,0x3C6946AA)

#if !__NO_OUTPUT
PARAM(debugCreateCameraObjectStack, "[camera] Collects callstacks of Create Camera Objects to help tracking down the cause of our object pool filling");
#endif

camBaseObject::camBaseObject(const camBaseObjectMetadata& metadata)
: m_Metadata(metadata)
#if __BANK
, m_WidgetGroup(NULL)
#endif // __BANK
{
}

camBaseObject::~camBaseObject()
{
#if __BANK
	//Remove any widgets associated with this instance.
	if(m_WidgetGroup)
	{
		m_WidgetGroup->Destroy();
		m_WidgetGroup = NULL;
	}
#endif // __BANK

#if !__NO_OUTPUT && !__FINAL
	UnregisterObjectStack();
#endif
}

u32 camBaseObject::GetNameHash() const
{
	return m_Metadata.m_Name.GetHash();
}

#if !__NO_OUTPUT
#if !__FINAL
sysStackCollector camBaseObject::s_CameraCreateObjectStackCollector;
atString* camBaseObject::s_CurrentObjectStackString = nullptr;

void camBaseObject::DisplayStackLine(size_t addr, const char* sym, size_t displacement)
{
	if(s_CurrentObjectStackString)
	{
		char displayStackLineBuffer[256];
#if __64BIT
		formatf(displayStackLineBuffer, "0x%016" SIZETFMT "x - %s+0x%" SIZETFMT "x\n", addr, sym, displacement);
#else
		formatf(displayStackLineBuffer, "0x%08" SIZETFMT "x - %s+0x%" SIZETFMT "x\n", addr, sym, displacement);
#endif
		*s_CurrentObjectStackString += atString(displayStackLineBuffer);
	}
	else
	{
		char displayStackLineBuffer[256];
#if __64BIT
		formatf(displayStackLineBuffer, "0x%016" SIZETFMT "x - %s+0x%" SIZETFMT "x", addr, sym, displacement);
#else
		formatf(displayStackLineBuffer, "0x%08" SIZETFMT "x - %s+0x%" SIZETFMT "x", addr, sym, displacement);
#endif
		cameraDisplayf("%s", displayStackLineBuffer);
	}
}

void camBaseObject::PrintStackInfo(u32 /*tagCount*/, u32 /*stackCount*/, const size_t* stack, u32 stackSize)
{
	sysStack::PrintCapturedStackTrace(stack, stackSize, camBaseObject::DisplayStackLine);
}

void camBaseObject::CollectCreateObjectStack()
{
	if(PARAM_debugCreateCameraObjectStack.Get())
	{
		s_CameraCreateObjectStackCollector.CollectStack((u32)CalcObjectStackKey(), (u32)2, (u32)0);
	}
}

void camBaseObject::UnregisterObjectStack()
{
	if(PARAM_debugCreateCameraObjectStack.Get())
	{
		s_CameraCreateObjectStackCollector.UnregisterTag(CalcObjectStackKey());
	}
}

void camBaseObject::PrintCollectedObjectStack(atString* objectStackString) const
{
	if(PARAM_debugCreateCameraObjectStack.Get())
	{
		s_CurrentObjectStackString = objectStackString;
		s_CameraCreateObjectStackCollector.PrintInfoForTag(CalcObjectStackKey(), camBaseObject::PrintStackInfo);
		s_CurrentObjectStackString = nullptr;
	}
}
#endif // !__FINAL

//NOTE: We should not need to access the name string in !__NO_OUTPUT builds and we may wish to remove the string in these builds in the future.
const char* camBaseObject::GetName() const
{
	return SAFE_CSTRING(m_Metadata.m_Name.TryGetCStr());
}
#endif // !__NO_OUTPUT
