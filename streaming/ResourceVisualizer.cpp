
#if !__FINAL

#include "ResourceVisualizer.h"
#include "atl/string.h"
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "file/stream.h"
#include "streaming/packfilemanager.h"
#include "streaming/streamingengine.h"
#include "streaming/streamingmodule.h"
#include "system/exec.h"
#include "system/param.h"
#include <time.h>

PARAM(resvisualize, "Enables Resource Visualizer");

#if __BANK
void CResourceVisualizer::InitWidgets()
{
	rage::bkBank& bank = BANKMGR.CreateBank("Resource Visualizer");
	bank.AddToggle("Enable", &m_enabled, rage::datCallback(MFA(CResourceVisualizer::Toggle), this)); 
	bank.AddButton("Dump Now", datCallback(MFA(CResourceVisualizer::Dump), this), "Dump metrics for this frame");
	bank.PopGroup();
}
#endif // __BANK

void CResourceVisualizer::Dump()
{
	if (m_enabled)
	{
		// Path
		const char* pFolder = "X:/";
		PARAM_resvisualize.Get(pFolder);

		atString path(pFolder);
		if (!path.EndsWith("/") || !path.EndsWith("\\"))
			path += '/';

		// User
		char username[32] = {0};
		if (sysGetEnv("USERNAME", username, sizeof(username)))
		{
			path += username;
			path += "_";
		}

		// Datetime		
		time_t curtime;
		time(&curtime);

		static const int LEN = 256;
		char timestamp[LEN];
		const struct tm *today = localtime(&curtime);
		::strftime(timestamp, LEN - 1, "%Y_%m_%d_%H_%M_%S", today);
		path += timestamp;

		// Extension
		path += ".rvx";

		// Dump
		DumpStreamingReport(false, path.c_str());
	}
}

void CResourceVisualizer::Toggle()
{
	m_enabled = !m_enabled;
}

s32 CResourceVisualizer::OrderByMemorySize(strIndex i, strIndex j)
{
	strStreamingInfoManager& info = strStreamingEngine::GetInfo();
	return ((info.GetObjectVirtualSize(j)+info.GetObjectPhysicalSize(j)) - (info.GetObjectVirtualSize(i)+info.GetObjectPhysicalSize(i)) < 0);
}

fiStream* CResourceVisualizer::OpenFile(bool append, const char* filename)
{
	fiStream* s = NULL;
	if (append)
	{
		s = fiStream::Open(filename, false);
		if (s != NULL)
		{
			s->Seek(s->Size());
		}
	}
	if (s == NULL)
	{
		s = fiStream::Create(filename);
	}

	return s;
}

void CResourceVisualizer::GetResourceInfo(const datResourceInfo& info, size_t (&UsedBySizeVirtual)[32], size_t (&UsedBySizePhysical)[32])
{
	size_t virtualCount = info.GetVirtualChunkCount();
	size_t physicalCount = info.GetPhysicalChunkCount();

	for (size_t i = 0; i < virtualCount; ++i)
	{
		size_t level = 0;
		size_t size = info.GetVirtualChunkSize();		

		while (size > 1)
		{
			size >>= 1;
			++level;
		}

		UsedBySizeVirtual[level]++;
	}

	if (physicalCount)
	{
		for (size_t i = virtualCount; i < (virtualCount + physicalCount); ++i)
		{
			size_t level = 0;
			size_t size = info.GetPhysicalChunkSize();
			
			while (size > 1)
			{
				size >>= 1;
				++level;
			}

			UsedBySizePhysical[level]++;
		}
	}
}

void CResourceVisualizer::DumpStreamingReport(bool append, const char* filename)
{
	// I/O operation
	fiStream* pFile = OpenFile(append, filename);

	// Buddy Heap Buckets
	/*fprintf(pFile, "=== Buddy Buckets ===\n");
	fprintf(pFile, "name, total_size, used_slots, free_slots\n");

	sysMemDistribution distribution;
	sysMemAllocator* allocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_VIRTUAL);
	allocator->GetMemoryDistribution(distribution);

	char* bucketName[] = {"4K", "8K", "16K", "32K", "64K", "128K", "256K", "512K", "1M", "2M", "4M", "8M", "16M", "32M", "64M", "128M"};
	int bucketBytes[] = {4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072};

	for (int i = 0; i < NELEM(bucketName); ++i)
	{		
		const size_t usedSlots = distribution.UsedBySize[i + 12];
		const size_t freeSlots = distribution.FreeBySize[i + 12];
		fprintf(pFile, "%s, %d, %d, %d\n", bucketName[i], bucketBytes[i], usedSlots, freeSlots);
	}

#if __PS3
	fprintf(pFile,"=== Physical Buddy Buckets ===\n");
	fprintf(pFile,"name, total_size, used_slots, free_slots\n");

	allocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_PHYSICAL);
	allocator->GetMemoryDistribution(distribution);

	for (int i = 0; i < NELEM(bucketName); ++i)
	{		
		const size_t usedSlots = distribution.UsedBySize[i + 12];
		const size_t freeSlots = distribution.FreeBySize[i + 12];
		fprintf(pFile, "%s, %d, %d, %d\n", bucketName[i], bucketBytes[i], usedSlots, freeSlots);
	}
#endif

	// Track queued streaming objects
	fprintf(pFile,"=== Queued Streams ===\n");
	fprintf(pFile,"name, virtual_size, physical_size, ref_count, virtual_needed, physical_needed, status\n");

	size_t numObjs = 0;
	strIndex requestedObjs[500];

	RequestInfoList::Iterator it(strStreamingEngine::GetInfo().GetRequestInfoList()->GetFirst());
	while (it.IsValid())
	{
		strStreamingInfo* pInfo = strStreamingEngine::GetInfo().GetStreamingInfo(it.Item()->m_Index);

		if(numObjs == 500)
			break;

		requestedObjs[numObjs++] = pInfo->GetIndex();
		it.Next();
	}

	std::sort(&requestedObjs[0], &requestedObjs[numObjs], &OrderByMemorySize2);
	
	for (size_t i = 0; i < 10 && i < numObjs; i++)
	{
		const strIndex index = requestedObjs[i];
		strStreamingInfo* pInfo = strStreamingEngine::GetInfo().GetStreamingInfo(index);
		const strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModule(index);
		const int objIndex = pModule->GetObjectIndex(index);
		
		atString virtualNeeded;
		atString physicalNeeded;
		atString status = atString("yes");
		size_t UsedBySizeVirtual[32] = {0};
		size_t UsedBySizePhysical[32] = {0};
		const bool bStreamingEmergency = (strStreamingEngine::GetLoader().GetMsTimeSinceLastEmergency() < 1000);

		if (pInfo->IsFlagSet(STRFLAG_INTERNAL_RESOURCE))
		{
			GetResourceInfo(pInfo->GetResourceInfo(), UsedBySizeVirtual, UsedBySizePhysical);

			for (int i = 12; i < 26; ++i)
			{
				size_t size = 1 << (i - 10);

				if (UsedBySizeVirtual[i] > 0)
				{
					if (0 == distribution.FreeBySize[i] && bStreamingEmergency)
						status = "no";

					char tmp[32] = {0};
					formatf(tmp, "%dx%d ", UsedBySizeVirtual[i], size);
					virtualNeeded += tmp;
				}

				if (UsedBySizePhysical[i] > 0)
				{
					if (0 == distribution.FreeBySize[i] && bStreamingEmergency)
						status = "no";

					char tmp[32] = {0};
					formatf(tmp, "%dx%d ", UsedBySizePhysical[i], size);
					physicalNeeded += tmp;
				}
			}
		}

		fprintf(pFile, "%s, %d, %d, %d, %s, %s, %s\n", pModule->GetName(objIndex), (pInfo->ComputeVirtualSize() >> 10), (pInfo->ComputePhysicalSize() >> 10), pModule->GetNumRefs(objIndex), virtualNeeded.c_str(), physicalNeeded.c_str(), status.c_str());
	}*/

	fprintf(pFile,"# Objects\n");
	size_t virtualSizeLoaded = 0;
	size_t physicalSizeLoaded = 0;
	size_t virtualSizeQueued = 0;
	size_t physicalSizeQueued = 0;
	size_t virtualSizeDiscardable= 0;
	size_t physicalSizeDiscardable= 0;

	enum {LOADED, DONT_DELETE, REQUESTED};

	RequestInfoList::Iterator reqIt(strStreamingEngine::GetInfo().GetRequestInfoList()->GetFirst());
	LoadedInfoList::Iterator loadedIt(strStreamingEngine::GetInfo().GetLoadedInfoList()->GetFirst());
	LoadedInfoMap* persistentLoadedMap = strStreamingEngine::GetInfo().GetLoadedPersistentInfoMap();
	LoadedInfoMap::Iterator persistentIt = persistentLoadedMap->CreateIterator();
	persistentIt.Start();


	for (int pass=0; pass<3; pass++)
	{
		while (true)
		{
			strStreamingInfo *pInfo = NULL;

			bool skip = false;
			strIndex index;

			switch(pass)
			{
			case LOADED:
				if (!loadedIt.IsValid())
				{
					skip = true;
				}
				else
				{
					index = loadedIt.Item()->m_Index;
					loadedIt.Next();
				}
				break;
			case DONT_DELETE:
				if (persistentIt.AtEnd())
				{
					skip = true;
				}
				else
				{
					index = persistentIt.GetData().m_Index;
					persistentIt.Next();
				}
				break;
			case REQUESTED:
				if (!reqIt.IsValid())
				{
					skip = true;
				}
				else
				{
					index = reqIt.Item()->m_Index;
					reqIt.Next();
				}
				break;
			}

			if (skip)
			{
				break;
			}

			pInfo = strStreamingEngine::GetInfo().GetStreamingInfo(index);

			if (!pInfo->IsFlagSet(STRFLAG_INTERNAL_DUMMY))
			{
				size_t vSize = pInfo->ComputeVirtualSize(index);
				size_t pSize = pInfo->ComputePhysicalSize(index);

				const char* location = 0;
				switch(pInfo->GetStatus())
				{
				case STRINFO_NOTLOADED: continue; // should never happen really
				case STRINFO_LOADED:
					virtualSizeLoaded += vSize;
					physicalSizeLoaded += pSize;
					location = "loaded";
					break;
				case STRINFO_LOADING:
				case STRINFO_LOADREQUESTED:
					virtualSizeQueued += vSize;
					physicalSizeQueued += pSize;
					location = "queued";
					break;
				default:
					continue;
				}

				strStreamingFile* pStreamingFile = strPackfileManager::GetImageFileFromHandle(pInfo->GetHandle());
				const char* imageName = pStreamingFile ? pStreamingFile->m_name.GetCStr() : "null";
				const strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModule(index);
				const char* objName = pModule->GetName(pModule->GetObjectIndex(index));
				const char* moduleName = pModule->GetModuleName();

				// Queued objects should NEVER be marked as discardable!
				if (REQUESTED != pass)
				{
					// Track discardable objects - this logic is ripped from IsObjectInUse() also make sure to check it's not marked "no delete"
					strLocalIndex objIndexInModule = strLocalIndex(index.Get() - pModule->GetStreamingIndex(strLocalIndex(0)).Get());
					if (pModule->GetNumRefs(objIndexInModule) <= 0 && pInfo->GetDependentCount() <= 0 && !pInfo->IsFlagSet(STR_DONTDELETE_MASK))
					{
						location= "discardable";
						virtualSizeDiscardable += vSize;
						physicalSizeDiscardable += pSize;
					}
				}

				fprintf(pFile, "%s, %s.%s, %s, %d, %d, %d\n", imageName, objName, moduleName, location, pSize, vSize, (vSize + pSize));
			}
		}
	}

	/*f
	// Dump one more "special" object which accounts for the memory used up in the streaming resource heap by non-streaming resources.
#if ONE_STREAMING_HEAP
	sysMemAllocator* pAllocatorRes = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_VIRTUAL);
	size_t virtualSizeNonStream = pAllocatorRes->GetMemoryUsed() - (virtualSizeLoaded + physicalSizeLoaded);
	size_t physicalSizeNonStream = 0;

	// Add Size
	size_t sizeVirtual = pAllocatorRes->GetHeapSize();
	size_t sizePhysical = 0;
#else
	sysMemAllocator* pAllocatorVirt = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_VIRTUAL);
	sysMemAllocator* pAllocatorPhys = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_PHYSICAL);
	size_t virtualSizeNonStream = pAllocatorVirt->GetMemoryUsed() - virtualSizeLoaded;
	size_t physicalSizeNonStream = pAllocatorPhys->GetMemoryUsed() - physicalSizeLoaded;

	// Add Size
	size_t sizeVirtual = pAllocatorVirt->GetHeapSize();
	size_t sizePhysical = pAllocatorPhys->GetHeapSize();

//#if __PPU && !__FINAL
//	sizeVirtual -= g_extra_devkit_memory;
//#endif

#endif

	printf(pFile,"=== Summary ===\n");
	size_t totalVirtual = virtualSizeLoaded + virtualSizeQueued + virtualSizeNonStream;
	size_t totalPhysical =  physicalSizeLoaded + physicalSizeQueued + physicalSizeNonStream;
	fprintf(pFile,"type, physical, virtual, total, size\n");
	fprintf(pFile,"Queued, %9d, %9d, %9d\n", physicalSizeQueued, virtualSizeQueued, virtualSizeQueued + physicalSizeQueued);
	fprintf(pFile,"Loaded, %9d, %9d, %9d\n", physicalSizeLoaded, virtualSizeLoaded, virtualSizeLoaded + physicalSizeLoaded);
	fprintf(pFile,"Discardable, %9d, %9d, %9d\n", physicalSizeDiscardable, virtualSizeDiscardable, virtualSizeDiscardable + physicalSizeDiscardable);
	fprintf(pFile,"NonStream, %9d, %9d, %9d\n", physicalSizeNonStream, virtualSizeNonStream, virtualSizeNonStream + physicalSizeNonStream);
	fprintf(pFile,"Totals, %9d, %9d, %9d\n", totalPhysical, totalVirtual, totalVirtual + totalPhysical);
	fprintf(pFile,"HeapSize, %9d, %9d, %9d\n", sizePhysical, sizeVirtual, sizeVirtual + sizePhysical);*/

	// I/O operation
	pFile->Close();
}

#endif // !__FINAL
