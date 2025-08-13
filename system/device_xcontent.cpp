//
// filename:	device_xcontent.cpp
// description:	
//

#if __XENON
// --- Include Files ------------------------------------------------------------

#include "device_xcontent.h"

// C headers
// Rage headers
#include "system/ipc.h"
#include "system/xtl.h"
#include "system/criticalsection.h"
// Game headers
#include "system/Installer.h"

// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------

// --- Static Globals -----------------------------------------------------------

static atStaticPool<XContentFileInfo, MAX_XCONTENT_OPEN_FILE_HANDLES> g_fileInfoPool;

static CLinkList<fiDeviceXContent*> g_deviceList;
static int g_localIndex=-1;
static HANDLE g_storageDeviceListener;
static sysIpcSema g_StartEnumerateSema;
static sysIpcSema g_EndEnumerateSema;
static sysCriticalSectionToken g_enumerateToken;
static XCONTENT_DATA g_enumerateContentData[MAX_XCONTENT_HANDLES];
static DWORD g_numContent = 0;
static fiDeviceXContent::DeviceChangeCB g_DeviceChangeCB = fiDeviceXContent::DeviceChangeCB::NullFunctor();
static bool g_ContentAvailable = true;

// --- Static class members -----------------------------------------------------

// --- Code ---------------------------------------------------------------------

void fiDeviceXContent::InitClass()
{
	// make sure we allocate enough space in this 'linked list' (it's a secret to everyone) 
	g_deviceList.Init(64);
	
	// create notification listener to catch when mounted DLCs are removed
	g_storageDeviceListener = XNotifyCreateListener(XNOTIFY_SYSTEM);

	// create semaphores
	g_StartEnumerateSema = sysIpcCreateSema(0);
	g_EndEnumerateSema = sysIpcCreateSema(0);
	// create thread reading notify message
	sysIpcCreateThread(EnumerateContentThread, NULL, sysIpcMinThreadStackSize, PRIO_NORMAL, "Enumerate Content", 2);
}

//
// name:		EnumerateContentThread
// description:	Enumerate content worker thread
void fiDeviceXContent::EnumerateContentThread(void* )
{
	bool bDeviceChanged = false;
	bool bDeviceChangedWithoutProfile = false;
	while(true)
	{
		DWORD id;
		ULONG_PTR param;

		// has there been a storage device change
		if(XNotifyGetNext(g_storageDeviceListener, XN_SYS_STORAGEDEVICESCHANGED, &id, &param))
			bDeviceChanged = true;

		if (g_localIndex != -1 && bDeviceChangedWithoutProfile)
		{
			bDeviceChangedWithoutProfile = false;
			bDeviceChanged = true;
		}
		
		// if there was a device change or an enumerate request
		if(bDeviceChanged || sysIpcPollSema(g_StartEnumerateSema))
		{
			sysCriticalSection cs(g_enumerateToken);

			DWORD bufferSize;
			DWORD status = ERROR_SUCCESS; 
			HANDLE contentEnum;

			if(g_localIndex != -1)
			{
				if(bDeviceChanged)
					g_ContentAvailable = false;

				DWORD rt = XContentCreateEnumerator(XUSER_INDEX_NONE, XCONTENTDEVICE_ANY, XCONTENTTYPE_MARKETPLACE, 0, MAX_XCONTENT_HANDLES, &bufferSize, &contentEnum);
				if(rt == ERROR_SUCCESS)
				{
					Assert(bufferSize == MAX_XCONTENT_HANDLES * sizeof(XCONTENT_DATA));
					//DWORD numItems;
					status = XEnumerate(contentEnum, &g_enumerateContentData[0], bufferSize, &g_numContent, NULL);

					CloseHandle(contentEnum);

					if(bDeviceChanged)
					{
						bool bSuccess = ResetXContentDevices();
						g_ContentAvailable = bSuccess;
						g_DeviceChangeCB(bSuccess);
					}
				}
				else if(rt != ERROR_NO_SUCH_USER)
				{
					Warningf("XContentCreateEnumerator returned error 0x%x\n", rt);
					if(bDeviceChanged)
						g_DeviceChangeCB(false);
				}
			}
			else if (bDeviceChanged)
				bDeviceChangedWithoutProfile = true;

			// If this was not caused by a device change then signal end enumerate
			if(!bDeviceChanged)
				sysIpcSignalSema(g_EndEnumerateSema);
			bDeviceChanged = false;
		}
		sysIpcSleep(16);
	}
}

//
// name:		ResetXContentDevices
// description:	Go through list of active devices and close and open their content
bool fiDeviceXContent::ResetXContentDevices()
{
	CLink<fiDeviceXContent*>* pDeviceLink = g_deviceList.GetFirst()->GetNext();
	bool bSuccess = true;

	while(pDeviceLink != g_deviceList.GetLast())
	{
		// go through list of content and if filename is the same check the device ids. If they have changed then
		// remount content
		DWORD i;
		for(i=0; i<g_numContent; i++)
		{
			fiDeviceXContent* pDevice = pDeviceLink->item;
			if(!strcmp(g_enumerateContentData[i].szFileName, pDevice->m_contentData.szFileName))
			{
				if(g_enumerateContentData[i].DeviceID != pDevice->m_contentData.DeviceID)
				{
					pDevice->CloseContent();
					pDevice->m_contentData.DeviceID = g_enumerateContentData[i].DeviceID;
					pDevice->CreateContent();
				}
				break;
			}
		}
		if(i == g_numContent)
			bSuccess = false;
		pDeviceLink = pDeviceLink->GetNext();
	}
	return bSuccess;
}

bool fiDeviceXContent::IsContentAvailable()
{
	return g_ContentAvailable;
}

void fiDeviceXContent::SetLocalIndex(s32 index)
{
	g_localIndex = index;
}

void fiDeviceXContent::StartEnumerate()
{
	sysIpcSignalSema(g_StartEnumerateSema);
}

bool fiDeviceXContent::FinishEnumerate(bool bWait)
{
	if(bWait)
	{
		sysIpcWaitSema(g_EndEnumerateSema);
		return true;
	}
	else
	{
		return sysIpcPollSema(g_EndEnumerateSema);
	}
}

void fiDeviceXContent::LockEnumerate()
{
	g_enumerateToken.Lock();
}

void fiDeviceXContent::UnlockEnumerate()
{
	g_enumerateToken.Unlock();
}

int fiDeviceXContent::GetNumXContent()
{
	return (int)g_numContent;
}

XCONTENT_DATA* fiDeviceXContent::GetXContent(int index)
{
	Assert(index < (int)g_numContent);
	return &g_enumerateContentData[index];
}

void fiDeviceXContent::SetDeviceChangeCallback(DeviceChangeCB cb)
{
	g_DeviceChangeCB = cb;
}

bool fiDeviceXContent::Init(s32 userIndex, XCONTENT_DATA* pContentData)
{
	m_userIndex = userIndex;
	m_contentData = *pContentData;

	return true;
}

DWORD fiDeviceXContent::MountAs(const char* pName)
{
	// Construct mount name
	const char* pColon = strchr(pName, ':');
	s32 len;
	Assert(pColon);
	len = pColon - pName;
	Assert(len < sizeof(m_mountName));

	strncpy(m_mountName, pName, len);
	m_mountName[len] = '\0';

	m_INode = NULL;
	DWORD r = CreateContent();

	if(r == ERROR_SUCCESS)
	{
		m_INode = g_deviceList.Insert(this);

		if( fiDevice::Mount( pName, *this, false ) )
		{
			Printf("mounting xcontent %s\n", m_mountName);
		}
		else
			Shutdown();
	}
	else
	{
		Printf("Failed to mount xcontent %s, error code = 0x%x\n", m_mountName, r);
	}

	return r;
}

void fiDeviceXContent::Shutdown()
{
	CloseContent();

	if (m_INode)
	{
		g_deviceList.Remove(m_INode);
		m_INode = NULL;
	}
}

DWORD fiDeviceXContent::CreateContent() const
{
	DWORD r = XContentCreate(XUSER_INDEX_NONE, m_mountName, &m_contentData, XCONTENTFLAG_OPENEXISTING, NULL, NULL, NULL);
	return r;
}

void fiDeviceXContent::CloseContent() const
{
	XContentClose(m_mountName, NULL);
}

fiHandle fiDeviceXContent::InternalOpen(const char *filename,bool readOnly) const
{
	fiHandle handle = fiHandleInvalid;
	int attempts = 0;
	while(attempts < 4)
	{
		if(g_ContentAvailable)
		{
			fiHandle handle = fiDeviceLocal::Open(filename, readOnly);
			if(handle == fiHandleInvalid)
				attempts++;
			else
				return handle;
		}
		else
			attempts = 0;
		sysIpcSleep(16);
	}
	return handle;
}

fiHandle fiDeviceXContent::InternalOpenBulk(const char *filename, u64& outBias) const
{
	fiHandle handle = fiHandleInvalid;
	int attempts = 0;
	while(attempts < 4)
	{
		if(g_ContentAvailable)
		{
			fiHandle handle = fiDeviceLocal::OpenBulk(filename, outBias);
			if(handle == fiHandleInvalid)
				attempts++;
			else
				return handle;
		}
		else
			attempts = 0;
		sysIpcSleep(16);
	}
	return handle;
}

#if !__FINAL
void fiDeviceXContent::DumpOpenFiles()
{
	Displayf("fiDeviceXContent::DumpOpenFiles");

	for(int i = 0; i < g_fileInfoPool.GetSize(); i++)
	{
		const XContentFileInfo *pFileInfo = &(g_fileInfoPool.GetElement(i));

		if(pFileInfo->m_handle != fiHandleInvalid)
		{
			Displayf("	%s, handle = 0x%x", pFileInfo->m_filename, pFileInfo->m_handle);
		}
	}
}
#endif // !__FINAL

fiHandle fiDeviceXContent::Open(const char *filename,bool readOnly) const
{
	if(g_fileInfoPool.IsFull())
	{
#if !__FINAL
		DumpOpenFiles();
		Quitf("fiDeviceXContent ran out of open file handles, max = %d. Please check TTY for the list of open files.", g_fileInfoPool.GetSize());
#endif // !__FINAL

		return fiHandleInvalid;	
	}

	XContentFileInfo* pFileInfo = g_fileInfoPool.New();
	if(pFileInfo)
	{
		strncpy(pFileInfo->m_filename, filename, sizeof(pFileInfo->m_filename));
		pFileInfo->m_handle = InternalOpen(filename, readOnly);
		if(pFileInfo->m_handle == fiHandleInvalid)
		{
			g_fileInfoPool.Delete(pFileInfo);
			return fiHandleInvalid;
		}
		pFileInfo->m_fileposn = 0;
	}
	return (fiHandle*)pFileInfo;
}

fiHandle fiDeviceXContent::OpenBulk(const char *filename,u64 &outBias) const
{
	if(g_fileInfoPool.IsFull())
	{
#if !__FINAL
		DumpOpenFiles();
		Quitf("fiDeviceXContent ran out of open file handles, max = %d. Please check TTY for the list of open files.", g_fileInfoPool.GetSize());
#endif // !__FINAL

		return fiHandleInvalid;	
	}

	XContentFileInfo* pFileInfo = g_fileInfoPool.New();
	if(pFileInfo)
	{
		strncpy(pFileInfo->m_filename, filename, sizeof(pFileInfo->m_filename));
		pFileInfo->m_handle = InternalOpenBulk(filename, outBias);
		if(pFileInfo->m_handle == fiHandleInvalid)
		{
			g_fileInfoPool.Delete(pFileInfo);
			return fiHandleInvalid;
		}
		pFileInfo->m_fileposn = 0;
	}
	return (fiHandle*)pFileInfo;

}

int fiDeviceXContent::Read(fiHandle handle,void *outBuffer,int bufferSize) const
{
	Assert(handle != fiHandleInvalid);
	XContentFileInfo* pFileInfo = (XContentFileInfo*)handle;
	DWORD numRead;

	while (!::ReadFile(pFileInfo->m_handle, outBuffer, bufferSize, &numRead, NULL)) {
		if(GetLastError() == ERROR_NOT_READY)
		{
			fiDeviceLocal::Close(pFileInfo->m_handle);
			pFileInfo->m_handle = InternalOpen(pFileInfo->m_filename, true);
			SetFilePointer(pFileInfo->m_handle, pFileInfo->m_fileposn, NULL, FILE_BEGIN);
		}
	}
	pFileInfo->m_fileposn += numRead;

	return numRead;
}

int fiDeviceXContent::ReadBulk(fiHandle handle,u64 offset,void *outBuffer,int bufferSize) const
{
	Assert(handle != fiHandleInvalid);
	XContentFileInfo* pFileInfo = (XContentFileInfo*)handle;

	OVERLAPPED overlapped = {0};
	overlapped.Internal = 0;
	overlapped.InternalHigh = 0;
	overlapped.Offset = (u32) offset;
	overlapped.OffsetHigh = (u32) (offset >> 32);
	overlapped.hEvent = NULL;

	// int result = -1;
	DWORD result = 0;
	while (!::ReadFile(pFileInfo->m_handle, outBuffer, bufferSize, &result, &overlapped )) {
		if(GetLastError() == ERROR_NOT_READY)
		{
			u64 outBias;
			fiDeviceLocal::CloseBulk(pFileInfo->m_handle);
			pFileInfo->m_handle = InternalOpenBulk(pFileInfo->m_filename, outBias);
		}
	}

	return result;
}

u64 fiDeviceXContent::Seek64(fiHandle handle,s64 offset,fiSeekWhence whence) const
{
	XContentFileInfo* pFileInfo = (XContentFileInfo*)handle;
	u64 filePosn = 0;

	filePosn = fiDeviceLocal::Seek64(pFileInfo->m_handle, offset, whence);
	Assertf(filePosn < 0xFFFFFFFF, "fiDeviceXContent::Seek64 - filePosition to big!");
	pFileInfo->m_fileposn = (u32)filePosn;
	return filePosn;
}

int fiDeviceXContent::Seek(fiHandle handle,int offset,fiSeekWhence whence) const
{
	XContentFileInfo* pFileInfo = (XContentFileInfo*)handle;
	u64 filePosn = 0;

	filePosn = fiDeviceLocal::Seek64(pFileInfo->m_handle, (u64)offset, whence);
	Assertf(filePosn < 0xFFFFFFFF, "fiDeviceXContent::Seek - filePosition to big!");
	pFileInfo->m_fileposn = (u32)filePosn;
	return pFileInfo->m_fileposn;
}

int fiDeviceXContent::Close(fiHandle handle) const
{
	XContentFileInfo* pFileInfo = (XContentFileInfo*)handle;
	int rt = fiDeviceLocal::Close(pFileInfo->m_handle);
	pFileInfo->m_handle = fiHandleInvalid;
	g_fileInfoPool.Delete(pFileInfo);
	return rt;
}

int fiDeviceXContent::CloseBulk(fiHandle handle) const
{
	XContentFileInfo* pFileInfo = (XContentFileInfo*)handle;
	int rt = fiDeviceLocal::CloseBulk(pFileInfo->m_handle);
	pFileInfo->m_handle = fiHandleInvalid;
	g_fileInfoPool.Delete(pFileInfo);
	return rt;
}

fiHandle fiDeviceXContent::CreateBulk(const char *) const
{
	Assert(0);
	return fiHandleInvalid;
}
fiHandle fiDeviceXContent::Create(const char *) const
{
	Assert(0);
	return fiHandleInvalid;
}

int fiDeviceXContent::WriteBulk(fiHandle ,u64 ,const void *,int ) const
{
	Assert(0);
	return 0;
}
int fiDeviceXContent::Write(fiHandle ,const void *,int ) const
{
	Assert(0);
	return 0;
}

fiHandle fiDeviceXContent::GetRealFileHandle(fiHandle handle)
{
	Assert(handle != fiHandleInvalid);
	XContentFileInfo* pFileInfo = (XContentFileInfo*)handle;
	return pFileInfo->m_handle;
}

#endif //__XENON

