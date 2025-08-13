
#include "script_save_pso_file_tracker.h"


// Rage headers
#include "parser/psofile.h"

// Game headers
#include "SaveLoad/SavegameScriptData/script_save_data.h"
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_data.h"


#if SAVEGAME_USES_PSO

void CScriptSavePsoFileTracker::Shutdown()
{
	atMap<atLiteralHashValue, psoStruct>::Iterator mapIter = StructMap.CreateIterator();

	for(mapIter.Start(); !mapIter.AtEnd(); mapIter.Next())
	{
		psoFile* file = &mapIter.GetData().GetFile();

		int FileCount = CScriptSaveData::ms_aFiles.GetCount();
		savegameDisplayf("CScriptSavePsoFileTracker::Shutdown - number of entries in ms_aFiles array is %d", FileCount);
		for(int loop = 0; loop < FileCount; loop++)
		{
			if (CScriptSaveData::ms_aFiles[loop].m_File == file)
			{
				CScriptSaveData::ms_aFiles[loop].m_RefCount--;
				savegameDisplayf("CScriptSavePsoFileTracker::Shutdown - ref count of ms_aFiles entry %d has been decremented to %d", loop, CScriptSaveData::ms_aFiles[loop].m_RefCount);
				if (CScriptSaveData::ms_aFiles[loop].m_RefCount == 0)
				{
					savegameDisplayf("CScriptSavePsoFileTracker::Shutdown - ref count of ms_aFiles entry %d is now 0 so delete the work buffer and the pso file", loop);

					CSaveGameBuffer::FreeSavegameMemoryBuffer(CScriptSaveData::ms_aFiles[loop].m_WorkBuffer);

					delete CScriptSaveData::ms_aFiles[loop].m_File;
					CScriptSaveData::ms_aFiles.DeleteFast(loop);
				}
			}
		}
	}

	savegameDisplayf("CScriptSavePsoFileTracker::Shutdown - about to reset StructMap for this nested script data. It currently contains %d items", StructMap.GetNumUsed());
	StructMap.Reset();

	savegameAssertf(CScriptSaveData::ms_aFiles.GetCount() == 0, "Found %d remaining PSO files that were still referenced!", CScriptSaveData::ms_aFiles.GetCount());
	for(int loop = 0; loop < CScriptSaveData::ms_aFiles.GetCount(); loop++)
	{
		savegameDisplayf("CScriptSavePsoFileTracker::Shutdown - forcing deletion of the work buffer and the pso file for ms_aFiles entry %d", loop);

		CSaveGameBuffer::FreeSavegameMemoryBuffer(CScriptSaveData::ms_aFiles[loop].m_WorkBuffer);

		savegameAssertf(CScriptSaveData::ms_aFiles[loop].m_RefCount == 0, "PSO file had %d outstanding references", CScriptSaveData::ms_aFiles[loop].m_RefCount);
		delete CScriptSaveData::ms_aFiles[loop].m_File;
	}
	CScriptSaveData::ms_aFiles.Reset();
}


void CScriptSavePsoFileTracker::ForceCleanup()
{
	atMap<atLiteralHashValue, psoStruct>::Iterator mapIter = StructMap.CreateIterator();

	for(mapIter.Start(); !mapIter.AtEnd(); mapIter.Next())
	{
		psoFile* file = &mapIter.GetData().GetFile();

		int FileCount = CScriptSaveData::ms_aFiles.GetCount();
		savegameDisplayf("CScriptSavePsoFileTracker::ForceCleanup - number of entries in ms_aFiles array is %d", FileCount);
		for(int loop = 0; loop < FileCount; loop++)
		{
			if (CScriptSaveData::ms_aFiles[loop].m_File == file)
			{
				CScriptSaveData::ms_aFiles[loop].m_RefCount--;
				savegameDisplayf("CScriptSavePsoFileTracker::ForceCleanup - ref count of ms_aFiles entry %d has been decremented to %d", loop, CScriptSaveData::ms_aFiles[loop].m_RefCount);
				if (CScriptSaveData::ms_aFiles[loop].m_RefCount == 0)
				{
					savegameDisplayf("CScriptSavePsoFileTracker::ForceCleanup - ref count of ms_aFiles entry %d is now 0 so delete the work buffer and the pso file", loop);

					CSaveGameBuffer::FreeSavegameMemoryBuffer(CScriptSaveData::ms_aFiles[loop].m_WorkBuffer);

					delete CScriptSaveData::ms_aFiles[loop].m_File;
					CScriptSaveData::ms_aFiles.DeleteFast(loop);
				}
				break;
			}
		}
	}

	savegameDisplayf("CScriptSavePsoFileTracker::ForceCleanup - about to reset StructMap for this nested script data. It currently contains %d items", StructMap.GetNumUsed());
	StructMap.Reset();
}


void CScriptSavePsoFileTracker::DeleteStruct(atLiteralHashValue hash)
{
	psoStruct* structPtr = StructMap.Access(hash);
	if (structPtr)
	{

		psoFile* file = &structPtr->GetFile();

		int FileCount = CScriptSaveData::ms_aFiles.GetCount();
		savegameDisplayf("CScriptSavePsoFileTracker::DeleteStruct - number of entries in ms_aFiles array is %d", FileCount);
		for(int loop = 0; loop < FileCount; loop++)
		{
			if (CScriptSaveData::ms_aFiles.GetCount()>0 && CScriptSaveData::ms_aFiles[loop].m_File == file)
			{
				CScriptSaveData::ms_aFiles[loop].m_RefCount--;
				savegameDisplayf("CScriptSavePsoFileTracker::DeleteStruct - ref count of ms_aFiles entry %d has been decremented to %d", loop, CScriptSaveData::ms_aFiles[loop].m_RefCount);
				if (CScriptSaveData::ms_aFiles[loop].m_RefCount == 0)
				{
					savegameDisplayf("CScriptSavePsoFileTracker::DeleteStruct - ref count of ms_aFiles entry %d is now 0 so delete the work buffer and the pso file", loop);

					CSaveGameBuffer::FreeSavegameMemoryBuffer(CScriptSaveData::ms_aFiles[loop].m_WorkBuffer);
					if(CScriptSaveData::ms_aFiles[loop].m_File)
					delete CScriptSaveData::ms_aFiles[loop].m_File;
					CScriptSaveData::ms_aFiles.DeleteFast(loop);
				}
				break;
			}
		}
	}
	StructMap.Delete(hash);
	savegameDisplayf("CScriptSavePsoFileTracker::DeleteStruct - deleting StructMap entry for this nested script data. The deleted entry had hash key %u. StructMap now contains %d items", hash.GetHash(), StructMap.GetNumUsed());
}

void CScriptSavePsoFileTracker::Append(atLiteralHashValue hash, psoStruct& str, u8* workBuffer)
{
	if (savegameVerifyf(!StructMap.Access(hash), "StructMap already contained an element with hash 0x%x", hash.GetHash()))
	{
		StructMap[hash] = str;
		psoFile* file = &str.GetFile();

		sysStack::PrintStackTrace();
		savegameDisplayf("CScriptSavePsoFileTracker::Append - adding entry to StructMap for this nested script data. The new entry has hash %u. StructMap now contains %d items", hash.GetHash(), StructMap.GetNumUsed());

		bool bFound = false;
		int FileCount = CScriptSaveData::ms_aFiles.GetCount();
		for(int loop = 0; loop < FileCount; loop++)
		{
			if (CScriptSaveData::ms_aFiles[loop].m_File == file)
			{
				CScriptSaveData::ms_aFiles[loop].m_RefCount++;
				savegameDisplayf("CScriptSavePsoFileTracker::Append - file already exists in entry %d of the ms_aFiles array. incrementing its refcount to %d", loop, CScriptSaveData::ms_aFiles[loop].m_RefCount);
				bFound = true;
			}
		}
		if (!bFound)
		{
			CScriptSaveData::FileWithCount f;
			f.m_File = file;
			f.m_RefCount = 1;
			f.m_WorkBuffer = workBuffer;
			CScriptSaveData::ms_aFiles.PushAndGrow(f);

			savegameDisplayf("CScriptSavePsoFileTracker::Append - file doesn't already exist in the ms_aFiles array so adding a new entry and setting its refCount to 1");
		}
	}
}

psoStruct* CScriptSavePsoFileTracker::GetStruct(atLiteralHashValue hash)
{
	return StructMap.Access(hash);
}

/*
bool CScriptSavePsoFileTracker::IsEmpty()
{
	bool bIsEmpty = true;

	if (StructMap.GetNumUsed() > 0)
	{
		savegameDisplayf("CScriptSavePsoFileTracker::IsEmpty - num used slots in StructMap = %d", StructMap.GetNumUsed());
		bIsEmpty = false;
	}

	if (Files.GetCount() > 0)
	{
		savegameDisplayf("CScriptSavePsoFileTracker::IsEmpty - num used slots in Files array = %d", Files.GetCount());
		bIsEmpty = false;
	}

	return bIsEmpty;
}
*/

#endif // SAVEGAME_USES_PSO

