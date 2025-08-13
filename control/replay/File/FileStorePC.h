#ifndef FILE_STORE_PC_H
#define FILE_STORE_PC_H 

#include "../ReplaySettings.h"

#if GTA_REPLAY

// rage
#include "file/remote.h"

// game
#include "frontend/ProfileSettings.h"
#include "system/SettingsManager.h"
#include "device_replay.h"

#if RSG_PC

class FileStorePC : public fiDeviceReplay
{
public:
	 virtual const char* GetDirectory() const { return "user:/"; }

	 virtual u64 GetMaxMemoryAvailable() const
	 {
		 eReplayMemoryLimit size = (eReplayMemoryLimit)CProfileSettings::GetInstance().GetInt(CProfileSettings::REPLAY_MEM_LIMIT);

		 return MemLimits[size].AsBytes();
	 }

	 virtual u64 GetFreeSpaceAvailable(bool force)
	 {
		 if( !m_ShouldUpdateDirectorySize && m_freeSpace != 0 && !force )
		 {
			 return m_freeSpace;
		 }

		 u64 freeBytesAvailable;
		 char buffer[RAGE_MAX_PATH] = {0};
		 m_pDevice->FixRelativeName(buffer, RAGE_MAX_PATH, GetDirectory());

		 USES_CONVERSION;
		 if (GetDiskFreeSpaceExW(reinterpret_cast<const wchar_t*>(UTF8_TO_WIDE(buffer)), (PULARGE_INTEGER)&freeBytesAvailable, NULL, NULL))
		 {
			m_freeSpace = freeBytesAvailable;
		 }

		 return m_freeSpace;
	 }

	 virtual u64 GetFreeSpaceAvailableForClips()
	 {
		u64 freeSpace = fiDeviceReplay::GetFreeSpaceAvailableForClips();
			 
		//return the minimum disk space as the actual free space could be less than our estimate.
		return MIN(freeSpace, m_freeSpace);
	 }

	 virtual u64 GetFreeSpaceAvailableForMontages()
	 {
		 u64 freeSpace = fiDeviceReplay::GetFreeSpaceAvailableForMontages();

		 //return the minimum disk space as the actual free space could be less than our estimate.
		 return MIN(freeSpace, m_freeSpace);
	 }

	 virtual u64 GetFreeSpaceAvailableForVideos()
	 {
		u64 freeSpace = GetFreeSpaceAvailable(false);
		return freeSpace;
	 }

	 virtual bool CanFullfillUserSpaceRequirement() 
	 {
		eReplayMemoryLimit size = (eReplayMemoryLimit)CProfileSettings::GetInstance().GetInt(CProfileSettings::REPLAY_MEM_LIMIT);

		 return CanFullfillSpaceRequirement(size); 
	 }

private:
	u64 m_freeSpace;
};

#endif // RSG_PC

#endif

#endif