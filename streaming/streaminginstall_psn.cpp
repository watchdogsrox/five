//
// streaming/streaminginstall_psn.cpp
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

STREAMING_OPTIMISATIONS()

#if __PPU
#include <sys/memory.h>

#include "streaminginstall_psn.h"
#include "file/savegame.h"
#include "file/device.h"
#include "file/asset.h"
#include "math/amath.h"
#include "system/timer.h"
#include "system/nelem.h"
#include "system/system.h"
#include "system/param.h"
// #include "text/text.h"

#if !__FINAL
PARAM(update, "Development update folder");
#endif
// :TODO: use correct SFO info..
static const char* SAMPLE_PARAMSFO_VERSION = "01.00";

fiPsnUpdateDevice fiPsnUpdateDevice::m_device;

/////////////////////////////////////
//start of psn update device
/////////////////////////////////////

fiPsnUpdateDevice& fiPsnUpdateDevice::Instance()
{
	static bool instanced = false;

	if(!instanced)
	{
		instanced = true;

#if !__FINAL
		const char* pUpdateFolder;
		if(PARAM_update.Get(pUpdateFolder))
		{
			static fiDeviceRelative gUpdateDevice;
			gUpdateDevice.Init(pUpdateFolder, false);
			gUpdateDevice.MountAs("update:/");
			Printf("Mount %s as update:", pUpdateFolder);
		}
		else
#endif
		{
			u32 result = cellGameDataCheckCreate2(
				CELL_GAMEDATA_VERSION_CURRENT, 
				XEX_TITLE_ID, 
				CELL_GAMEDATA_ERRDIALOG_ALWAYS, 
				&fiPsnUpdateDevice::GameDataStatCallback,
				SYS_MEMORY_CONTAINER_ID_INVALID);

			if(result != CELL_OK)
				streamErrorf("fiPsnUpdateDevice::Failed to retrieve the gamedata directory (%d)", result);
		}
	}

	return m_device;
}

void fiPsnUpdateDevice::GameDataStatCallback(CellGameDataCBResult* result, CellGameDataStatGet* get, CellGameDataStatSet* UNUSED_PARAM(set))
{
	m_device.Init(get->gameDataPath, false);
	m_device.MountAs("update:/");

	result->result = CELL_GAMEDATA_CBRESULT_OK_CANCEL ;
	result->reserved = NULL;
}

/////////////////////////////////////
//end of psn update device
/////////////////////////////////////

fiPsnInstallerDevice::fiPsnInstallerDevice(bool forceReinstall)
:	m_StartedInstalling(false)
,	m_FinishedInstalling(false)
,   m_InstallFailed(false)
,   m_NotEnoughSpace(false)
,	m_ForceReinstall(forceReinstall)
,	m_BytesCopied(0)
,	m_InstallSize(0)
{	
}

fiHandle fiPsnInstallerDevice::InstallFile(const char* path, bool readOnly, const char* installedname, bool isContentInfo)
{
	streamAssertf(!m_StartedInstalling, "Can't add an installable file after game installation");
	fiHandle handle = (fiHandle)m_Files.GetCount();
	fiInstalledFile& f = m_Files.Grow();	
	safecpy(f.m_SrcFilename, path, NELEM(f.m_SrcFilename));
	if (!installedname || !installedname[0])
		installedname = ASSET.FileName(path);
	safecpy(f.m_InstallName, installedname, NELEM(f.m_InstallName));	
	f.m_Handle = fiHandleInvalid;
	f.m_ReadOnly = readOnly;
	f.m_IsContentInfo = isContentInfo;
	f.m_SeekPos = 0;
	f.m_Installed = false;
	return handle;
}

fiHandle fiPsnInstallerDevice::LocalHandle(fiHandle handle) const
{
	fiInstalledFile& f = m_Files[(int)handle];
	streamAssertf(f.m_Installed, "Trying to access a file that hasn't finished installing");
	if (f.m_Handle == fiHandleInvalid)
	{
		const fiDevice& localdev = fiDeviceLocal::GetInstance();
		if (m_OpenFiles.IsFull())
		{
			// out of file handles, close the oldest one
			fiInstalledFile* fileToClose = m_OpenFiles.Pop();
			streamAssert(fileToClose->m_Handle != fiHandleInvalid);
			fileToClose->m_SeekPos = localdev.Seek(fileToClose->m_Handle, 0, seekCur);
			localdev.Close(fileToClose->m_Handle);
			fileToClose->m_Handle = fiHandleInvalid;
		}
		char destname[2048];
		GetInstalledPath(destname, NELEM(destname), f);
		const_cast<fiInstalledFile&>(f).m_Handle = localdev.Open(destname, f.m_ReadOnly);
		streamAssertf(f.m_Handle != fiHandleInvalid, 
			"Error opening installed file %s (installed from %s)", destname, f.m_SrcFilename);
		localdev.Seek(f.m_Handle, f.m_SeekPos, seekSet);		
		m_OpenFiles.Push(&f);
	}
	return f.m_Handle;
}

int fiPsnInstallerDevice::Close(fiHandle handle) const
{
	fiInstalledFile& f = m_Files[(int)handle];
	if (f.m_Handle != fiHandleInvalid)
	{
		fiDeviceLocal::GetInstance().Close(f.m_Handle);
		f.m_Handle = fiHandleInvalid;
		int i = 0;
		streamVerify(m_OpenFiles.Find(&f, &i));
		m_OpenFiles.Delete(i);
	}
	return 0;
}

static fiPsnInstallerDevice* s_InstallerDevice = 0;

bool fiPsnInstallerDevice::InitInstall()
{
	m_StartedInstalling = true;
	s_InstallerDevice = this;
	// create gamedata directory or find existing one
	u32 result = cellGameDataCheckCreate2(
		CELL_GAMEDATA_VERSION_CURRENT, 
		XEX_TITLE_ID, 
		CELL_GAMEDATA_ERRDIALOG_ALWAYS, 
		&fiPsnInstallerDevice::GameDataStatCallbackStub,
		SYS_MEMORY_CONTAINER_ID_INVALID);

	if(result != CELL_OK)
	{
		streamErrorf("fiPsnInstallerDevice::InstallOpenedFiles Failed to create the gamedata directory (%d)", result);
		m_InstallFailed = true;
		if (m_progressCallback)
			m_progressCallback(InstallStatus(), InstallProgress());
		return false;
	}
	return true;
}

void fiPsnInstallerDevice::InstallOpenedFiles()
{
	// start the installer thread
	if(InstallStatus() == INSTALL_OK)
		m_Thread = sysIpcCreateThread(&fiPsnInstallerDevice::InstallerThreadStub, this, sysIpcMinThreadStackSize, PRIO_BELOW_NORMAL, "Installer");
}

bool fiPsnInstallerDevice::IsInstalled(fiHandle handle)
{
	return m_Files[(int)handle].m_Installed;
}

bool fiPsnInstallerDevice::IsEverythingInstalled()
{
	// go through list of files and check if they are all installed
	for(atArray<fiInstalledFile>::iterator it = m_Files.begin(); it != m_Files.end(); ++it)
	{
		fiInstalledFile& f = *it;
		if (!f.m_Installed)
			return false;
	}
	return true;
}

bool fiPsnInstallerDevice::FinishInstallation()
{
	streamAssert(m_StartedInstalling);
	sysIpcWaitThreadExit(m_Thread);
	return !m_InstallFailed;
}

void fiPsnInstallerDevice::InstallerThreadStub(void* ptr)
{
	static_cast<fiPsnInstallerDevice*>(ptr)->InstallerThread();
}

void fiPsnInstallerDevice::InstallerThread()
{
	// install any missing files
	streamDisplayf("INSTALLING..");
	sysTimer timer;
    float nextProgressReport = 0.0f;
	m_InstallFailed = false;
	const fiDevice& localdev = fiDeviceLocal::GetInstance();
	for(atArray<fiInstalledFile>::iterator it = m_Files.begin(); it != m_Files.end(); ++it)
	{
		fiInstalledFile& f = *it;
		if (!f.m_Installed)
		{
			char destname[2048];
			GetInstalledPath(destname, NELEM(destname), f);

			streamDisplayf("INSTALLING %s to %s", f.m_SrcFilename, destname);

			const fiDevice* srcdevice = fiDevice::GetDevice(f.m_SrcFilename, true);
			fiHandle src = srcdevice->Open(f.m_SrcFilename, true);
			streamAssertf(src != fiHandleInvalid, "Error opening %s for read", f.m_SrcFilename);
			localdev.Delete(destname);
			fiHandle dst = localdev.Create(destname);
			streamAssertf(dst != fiHandleInvalid, "Error opening %s for write", destname);

			u32 size = srcdevice->GetFileSize(f.m_SrcFilename);
			u32 copied = 0;
			while (copied < size)
			{
				char buffer[32768];
				int amtRead = Min(sizeof(buffer), size - copied);
				srcdevice->Read(src, buffer, amtRead);
				if (localdev.Write(dst, buffer, amtRead) != amtRead) 
					break;
				copied += amtRead;
				m_BytesCopied += amtRead;

                if (timer.GetTime() > nextProgressReport)
                {
                    nextProgressReport += 1.0f;
                    /*streamDisplayf("Copied %llu/%lluMB (%llu%%)", 
                        m_BytesCopied/(1024*1024), m_InstallSize/(1024*1024), 
                        m_BytesCopied * 100 / m_InstallSize);*/

                    if (m_progressCallback)
					    m_progressCallback(InstallStatus(), InstallProgress());
                }

				if (CSystem::WantToExit())
					break;
			}
			srcdevice->Close(src);
			localdev.Close(dst);

			if (copied == size)
			{
				// copy file timestamp
				u64 timestamp = srcdevice->GetFileTime(f.m_SrcFilename);
				localdev.SetFileTime(destname, timestamp);
				f.m_Installed = true;
			}
			else
			{
				m_InstallFailed = true;
				break;
			}
		}
	}	
	m_progressCallback(InstallStatus(), 1.0f);
	streamDisplayf("..INSTALLED in %i seconds!", (int)timer.GetTime());

	if (m_InstallFailed)
	{
		streamErrorf("Game Data installation failed!");
	}
}

#if __DEV
static void DumpCellGameDataStatGet(CellGameDataStatGet *get)
{
	streamDisplayf("Dump CellGameDataStatGet in CellGameDataStatCallback--------------------" );
	streamDisplayf("\tget->hddFreeSizeKB 0x%x", get->hddFreeSizeKB);
	streamDisplayf("\tget->isNewData: %d", get->isNewData );
	streamDisplayf("\tget->contentInfoPath : %s", get->contentInfoPath );
	streamDisplayf("\tget->gameDataPath    : %s", get->gameDataPath );
	streamDisplayf("\tget->sizeKB : %d", get->sizeKB);
	streamDisplayf("\tget->sysSizeKB : 0x%x", get->sysSizeKB);
	streamDisplayf("\tget->%lld  %lld  %lld", get->st_atime, get->st_ctime, get->st_mtime );

	streamDisplayf("\n\tPARAM.SFO:TITLE = [%s]", get->getParam.title );

	// titleLang
	for(int i=0 ; i < 16 ; i++)
	{
		streamDisplayf("\tPARAM.SFO:TITLE%02d = [%s]", i, get->getParam.titleLang[i]);
	}

	streamDisplayf("\tPARAM.SFO:TITLEID        = [%s]", get->getParam.titleId );
	streamDisplayf("\tPARAM.SFO:VERSION        = [%s]", get->getParam.dataVersion );
	//streamDisplayf("\tPARAM.SFO:ATTRIBUTE      = [%d]", get->getParam.attribute ); // no longer used with 210 SDK - KS
	//streamDisplayf("\tPARAM.SFO:PARENTAL_LEVEL = [%d]\n", get->getParam.parentalLevel );// no longer used with 210 SDK - KS
}
#endif

void fiPsnInstallerDevice::GetInstalledPath(char* buf, u32 bufsize, const fiInstalledFile& f) const
{
	if (f.m_IsContentInfo)
		safecpy(buf, m_ContentInfoDir, bufsize);
	else
		safecpy(buf, m_GameDataDir, bufsize);

	safecat(buf, f.m_InstallName, bufsize);
}

void fiPsnInstallerDevice::GetInstalledPath(char* buf, u32 bufsize, const char* path) const
{
	safecpy(buf, m_GameDataDir, bufsize);
	safecat(buf, ASSET.FileName(path), bufsize);
}

void fiPsnInstallerDevice::GameDataStatCallbackStub(CellGameDataCBResult* result, 
													CellGameDataStatGet* get, CellGameDataStatSet* set)
{
	s_InstallerDevice->GameDataStatCallback(result, get, set);
}

#if !__FINAL
PARAM(fakefreespace, "Fake amount of free hard disk space (in KB)");
#endif

void fiPsnInstallerDevice::GameDataStatCallback(CellGameDataCBResult* result, 
												CellGameDataStatGet* get, CellGameDataStatSet* set)
{
	DEV_ONLY(DumpCellGameDataStatGet(get);)

	// initialise the game data device to the correct path
	safecpy(m_GameDataDir, get->gameDataPath, NELEM(m_GameDataDir));
	char* end = &m_GameDataDir[strlen(m_GameDataDir)-1];
	if (*end != '/' && *end != '\\')
	{
		*++end = '/'; *++end = 0;
	}

	safecpy(m_ContentInfoDir, get->contentInfoPath, NELEM(m_ContentInfoDir));
	end = &m_ContentInfoDir[strlen(m_ContentInfoDir)-1];
	if (*end != '/' && *end != '\\')
	{
		*++end = '/'; *++end = 0;
	}

	int requiredSpace = 0;

#if !__FINAL
    if (PARAM_fakefreespace.Get(get->hddFreeSizeKB))
        streamDisplayf("Faking %iKB free space", get->hddFreeSizeKB);
#endif

	if (get->isNewData) 
	{
		streamDisplayf("Creating new game data at %s", get->contentInfoPath);
		requiredSpace = get->sysSizeKB;
	}
	PrepareNewGameData(get->getParam);

	requiredSpace += CalculateSizeToInstall();

	if (requiredSpace > get->hddFreeSizeKB) 
	{
		result->errNeedSizeKB = requiredSpace - get->hddFreeSizeKB;
		result->result = CELL_GAMEDATA_CBRESULT_ERR_NOSPACE;
        m_NotEnoughSpace = true;
		streamErrorf("HDD size check error. needs %d KB disc space more.", result->errNeedSizeKB);
		return;
	}

	set->setParam = &get->getParam;
	set->reserved = NULL;

	result->result = CELL_GAMEDATA_CBRESULT_OK;
	result->reserved = NULL;
}

void fiPsnInstallerDevice::PrepareNewGameData(CellGameDataSystemFileParam& param)
{
	// may be better just to declare a constant CellGameDataSystemFileParam somewhere?
//	GxtChar *pTempGxtString = TheText.Get("MO_JIM_LEVEL");
	strcpy(param.title, (char *) "FRAMEWORK"); //SAMPLE_PARAMSFO_TITLE );
	// 		strcpy(get->getParam.titleLang[0],	SAMPLE_PARAMSFO_TITLE00 );
	// 		strcpy(get->getParam.titleLang[1],	SAMPLE_PARAMSFO_TITLE01 );
	// 		strcpy(get->getParam.titleLang[2],	SAMPLE_PARAMSFO_TITLE02 );
	// 		strcpy(get->getParam.titleLang[3],	SAMPLE_PARAMSFO_TITLE03 );
	// 		strcpy(get->getParam.titleLang[4],	SAMPLE_PARAMSFO_TITLE04 );
	// 		strcpy(get->getParam.titleLang[5],	SAMPLE_PARAMSFO_TITLE05 );
	// 		strcpy(get->getParam.titleLang[6],	SAMPLE_PARAMSFO_TITLE06 );
	// 		strcpy(get->getParam.titleLang[7],	SAMPLE_PARAMSFO_TITLE07 );
	// 		strcpy(get->getParam.titleLang[8],	SAMPLE_PARAMSFO_TITLE08 );
	// 		strcpy(get->getParam.titleLang[9], 	SAMPLE_PARAMSFO_TITLE09 );
	// 		strcpy(get->getParam.titleLang[10],	SAMPLE_PARAMSFO_TITLE10 );
	// 		strcpy(get->getParam.titleLang[11],	SAMPLE_PARAMSFO_TITLE11 );
	// 		strcpy(get->getParam.titleLang[12],	SAMPLE_PARAMSFO_TITLE12 );
	// 		strcpy(get->getParam.titleLang[13],	SAMPLE_PARAMSFO_TITLE13 );
	// 		strcpy(get->getParam.titleLang[14],	SAMPLE_PARAMSFO_TITLE14 );
	// 		strcpy(get->getParam.titleLang[15],	SAMPLE_PARAMSFO_TITLE15 );
	strcpy(param.titleId,		XEX_TITLE_ID );
	strcpy(param.dataVersion,	SAMPLE_PARAMSFO_VERSION );
	//param.parentalLevel = SAMPLE_PARAMSFO_PARENTALLEV; // no longer used with 210 SDK - KS
	//param.attribute     = SAMPLE_PARAMSFO_ATTRIBUTE; // no longer used with 210 SDK - KS
}

static bool TimeIsClose(u64 timeA,u64 timeB) 
{
	// Time is in 100ns units.  FAT filesystem is only accurate to two seconds.
	const u64 minTime = 3 * 10000000;
	if (timeA > timeB)
		return timeA - timeB < minTime;
	else
		return timeB - timeA < minTime;
}

u32 fiPsnInstallerDevice::CalculateSizeToInstall()
{
	u32 requiredSpace = 0;
	m_InstallSize = 0;
	const fiDevice& localdev = fiDeviceLocal::GetInstance();
	for(atArray<fiInstalledFile>::iterator it = m_Files.begin(); it != m_Files.end(); ++it)
	{
		fiInstalledFile& f = *it;
		char destname[2048];
		GetInstalledPath(destname, NELEM(destname), f);
		u64 dstsize = localdev.GetFileSize(destname);
		u64 dsttime = localdev.GetFileTime(destname);
		const fiDevice* srcdevice = fiDevice::GetDevice(f.m_SrcFilename, true);
		u64 srcsize = srcdevice->GetFileSize(f.m_SrcFilename);
		u64 srctime = srcdevice->GetFileTime(f.m_SrcFilename);
		if (dstsize != srcsize || !TimeIsClose(dsttime, srctime) || m_ForceReinstall)
		{
			requiredSpace -= (dstsize + 1023) / 1024; // space reclaimed by deleting old file
			requiredSpace += (srcsize + 1023) / 1024;
			m_InstallSize += srcsize;
		}
		else
		{
			streamDisplayf("%s already installed at %s", f.m_SrcFilename, destname);
			f.m_Installed = true;
		}
	}
	return requiredSpace;
}

fiHandle fiPsnInstallerDevice::InstallIconFile(const char* path)
{
	return InstallFile(path, true, "ICON0.PNG", true);
}

void fiPsnInstallerDevice::SetProgressCallback(Functor2<eInstallStatus, float> fn) 
{
    m_progressCallback = fn;
}

void fiPsnInstallerDevice::SetProgress(eInstallStatus status, float progress)
{
	if(m_progressCallback)
		m_progressCallback(status, progress);
}

float fiPsnInstallerDevice::InstallProgress()
{
	if (m_InstallSize == 0)
		return 1.0f;

	return float(m_BytesCopied) / m_InstallSize;
}

eInstallStatus fiPsnInstallerDevice::InstallStatus()
{
	if(m_StartedInstalling == false)
		return INSTALL_NOT_STARTED;
    else if (m_NotEnoughSpace)
        return INSTALL_ERROR_NOT_ENOUGH_SPACE;
    else if (m_InstallFailed)
        return INSTALL_ERROR_INSTALLING;
    return INSTALL_OK;
}

#endif // __PPU
