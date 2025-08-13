//
// camera/system/CameraFactory.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef CAMERA_FACTORY_H
#define CAMERA_FACTORY_H

#include "atl/hashstring.h"
#include "atl/map.h"
#include "parser/visitorwidgets.h"

#include "camera/camera_channel.h"

//A camera-specific helper macro that verifies the object pool is valid before use, returning NULL and providing useful debug TTY if it's full or has
//yet to be allocated.
#define camera_verified_pool_new(_T) !camFactory::VerifySimpleObjectPool<_T>(#_T) ? NULL : rage_new _T

namespace rage
{
	class bkGroup;
	class parMemberString;
	class parStructure;

	class fwBasePool;
}

class camBaseDirectorMetadata;
class camBaseObject;
class camBaseObjectMetadata;
class camMetadataStore;

static const int g_MaxWidgetStringProxyLength = 64;

class camFactoryHelperBase
{
public:
	virtual	~camFactoryHelperBase() {}
	virtual	camBaseObject* CreateObject(const camBaseObjectMetadata& metadata, const atHashWithStringNotFinal& classIdToVerify) = 0;

protected:
	void EnsureObjectClassIdIsValid(camBaseObject*& object, const camBaseObjectMetadata& metadata, const atHashWithStringNotFinal& classIdToVerify);
	bool VerifyObjectPool(fwBasePool* pool, const camBaseObjectMetadata& metadata, const atHashWithStringNotFinal& classId) const;
};

template <class _Type> class camFactoryHelper : public camFactoryHelperBase
{
public:
	virtual camBaseObject* CreateObject(const camBaseObjectMetadata& metadata, const atHashWithStringNotFinal& classIdToVerify)
	{
		camBaseObject* object = rage_new _Type(metadata);

		EnsureObjectClassIdIsValid(object, metadata, classIdToVerify);

		return object;
	}
};

template <class _Type> class camFactoryHelperPooled : public camFactoryHelper<_Type>
{
public:
	virtual camBaseObject* CreateObject(const camBaseObjectMetadata& metadata, const atHashWithStringNotFinal& classIdToVerify)
	{
		const bool isPoolValid	= camFactoryHelperBase::VerifyObjectPool(_Type::_ms_pPool, metadata, _Type::GetStaticClassId());
		camBaseObject* object	= isPoolValid ? camFactoryHelper<_Type>::CreateObject(metadata, classIdToVerify) : NULL;

		return object;
	}
};

class camFactory
{
public:
	static void		InitPools();
	static void		ShutdownPools();
	static void		Init();
	static void		Shutdown();

	static u32		GetMaxCameraSize();
	static u32		GetMaxFrameShakerSize();
	static u32		GetMaxSwitchHelperSize();

	template <class _Type> static const _Type* FindObjectMetadata(const char* name)
	{
		return static_cast<const _Type*>(FindObjectMetadata(atHashWithStringNotFinal(name), _Type::parser_GetStaticStructure()));
	}
	template <class _Type> static const _Type* FindObjectMetadata(const u32 nameHash)
	{
		return static_cast<const _Type*>(FindObjectMetadata(atHashWithStringNotFinal(nameHash), _Type::parser_GetStaticStructure()));
	}

	template <class _Type> static bool IsObjectMetadataOfClass(const camBaseObjectMetadata& metadata)
	{
		const parStructure* parserStructureToVerify = _Type::parser_GetStaticStructure();
		return (parserStructureToVerify && IsObjectMetadataOfClass(metadata, *parserStructureToVerify));
	}

	template <class _Type> static _Type* CreateObject(const char* name)
	{
		return static_cast<_Type*>(CreateObject(atHashWithStringNotFinal(name), _Type::GetStaticClassId()));
	}
	template <class _Type> static _Type* CreateObject(const u32 nameHash)
	{
		return static_cast<_Type*>(CreateObject(atHashWithStringNotFinal(nameHash), _Type::GetStaticClassId()));
	}
	template <class _Type> static _Type* CreateObject(const camBaseObjectMetadata& metadata)
	{
		return static_cast<_Type*>(CreateObject(metadata, _Type::GetStaticClassId()));
	}

	template <class _Type> static bool VerifySimpleObjectPool(const char* className)
	{
		return VerifySimpleObjectPool(_Type::GetPool(), className);
	}

	static const camMetadataStore* GetMetadataStore() { return ms_MetadataStore; }

#if __BANK
	static void AddWidgets(bkBank& bank);

	static void DebugDumpAllObjectPools();
	static void DebugDumpObjectPool(const fwBasePool* pool, bool isCamBaseObject = true);
	static void DebugDumpAllCreateObjectsCallStack();
	static void DebugDumpCreateObjectCallStack(const fwBasePool* pool, bool isCamBaseObject = true);
#endif // __BANK

private:
	static void	MapFactoryHelpers();

	static const camBaseObjectMetadata*	FindObjectMetadata(const atHashWithStringNotFinal& name, const parStructure* parserStructureToVerify = NULL);
	static bool							IsObjectMetadataOfClass(const camBaseObjectMetadata& metadata, const parStructure& parserStructureToVerify);
	static camBaseObject*				CreateObject(const atHashWithStringNotFinal& name, const atHashWithStringNotFinal& classIdToVerify);
 	static camBaseObject*				CreateObject(const camBaseObjectMetadata& metadata, const atHashWithStringNotFinal& classIdToVerify);
	static bool							VerifySimpleObjectPool(fwBasePool* pool, const char* className);

#if __BANK
#if PARSER_ALL_METADATA_HAS_NAMES
	static void AddWidgetsForMetadata(bkBank& bank);
	static void SaveMetadata();

	static bkGroup* ms_MetadataWidgetGroup;	//The bank group containing metadata-specific RAG widgets, or NULL.
#endif // PARSER_ALL_METADATA_HAS_NAMES
#endif // __BANK

	static camMetadataStore* ms_MetadataStore;

	static atMap<u32, camBaseObjectMetadata*> ms_MetadataMap;
	static atMap<u32, camFactoryHelperBase*> ms_FactoryHelperMap;
};

#if __BANK && PARSER_ALL_METADATA_HAS_NAMES

class camWidgetStringProxy : public datBase
{
public:
	camWidgetStringProxy(atHashString* hashString, const char* widgetString);

	void OnChange();

	atHashString* m_HashString;
	char m_WidgetString[g_MaxWidgetStringProxyLength];
};

//A camera system specific visitor that iterates over a structure and creates bank widgets for all the members within.
class camBuildWidgetsVisitor : public rage::parBuildWidgetsVisitor
{
public:
	static void ShutdownClass();

	virtual void StringMember(parPtrToMember ptrToMember, const char* ptrToString, parMemberString& metadata);

private:
	static atArray<camWidgetStringProxy*> ms_ProxyStrings;
};

#endif // __BANK && PARSER_ALL_METADATA_HAS_NAMES

#endif // CAMERA_FACTORY_H
