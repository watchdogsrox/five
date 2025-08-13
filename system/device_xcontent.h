//
// filename:	device_xcontent.h
// description:	
//

#ifndef INC_DEVICE_XCONTENT_H_
#define INC_DEVICE_XCONTENT_H_

#if __XENON
// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "atl/ilist.h"
#include "atl/staticpool.h"
#include "system/xtl.h"
#include "file/device.h"
// Game headers
#include "templates/LinkList.h"
#include "fwtl/pool.h"

// --- Defines ------------------------------------------------------------------

#define MAX_XCONTENT_HANDLES	(64)	//	32
#define MAX_XCONTENT_OPEN_FILE_HANDLES	(272)	

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

struct XContentFileInfo
{
	fiHandle m_handle;
	int m_deviceId;
	char m_filename[128];
	u32 m_fileposn;
};

class fiDeviceXContent : public fiDeviceLocal
{
public:
	typedef Functor1<bool> DeviceChangeCB;

	~fiDeviceXContent() {Shutdown();}
	
	static void InitClass();

	bool Init(s32 userIndex, XCONTENT_DATA* pData);
	DWORD MountAs(const char* pName);
	void Shutdown();

	virtual fiHandle Open(const char *filename,bool readOnly) const;
	virtual fiHandle OpenBulk(const char *filename,u64 &outBias) const;
	virtual fiHandle CreateBulk(const char *filename) const;
	virtual fiHandle Create(const char *filename) const;
	virtual int Read(fiHandle handle,void *outBuffer,int bufferSize) const;
	virtual int ReadBulk(fiHandle handle,u64 offset,void *outBuffer,int bufferSize) const;
	virtual int WriteBulk(fiHandle handle,u64 offset,const void *inBuffer,int bufferSize) const;
	virtual int Write(fiHandle handle,const void *buffer,int bufferSize) const;
	virtual int Seek(fiHandle handle,int offset,fiSeekWhence whence) const;
	virtual u64 Seek64(fiHandle handle,s64 offset,fiSeekWhence whence) const;
	virtual int Close(fiHandle handle) const;
	virtual int CloseBulk(fiHandle handle) const;

	static void SetLocalIndex(s32 index);
	static void StartEnumerate();
	static bool FinishEnumerate(bool bWait);
	static void LockEnumerate();
	static void UnlockEnumerate();
	static int GetNumXContent();
	static XCONTENT_DATA* GetXContent(int index);
	static void SetDeviceChangeCallback(DeviceChangeCB cb);
	static bool IsContentAvailable();

	static fiHandle GetRealFileHandle(fiHandle handle);

private:
	DWORD CreateContent() const;
	void CloseContent() const;
	fiHandle InternalOpen(const char*filename, bool readOnly) const;
	fiHandle InternalOpenBulk(const char*filename,u64 &outBias) const;

	static bool ResetXContentDevices();
	static void EnumerateContentThread(void* );

#if !__FINAL
	static void DumpOpenFiles();
#endif // !__FINAL

	s32 m_userIndex;
	XCONTENT_DATA m_contentData;
	char m_mountName[32];
	CLink<fiDeviceXContent*>* m_INode;
};

// --- Globals ------------------------------------------------------------------

#endif // __XENON
#endif // !INC_DEVICE_XCONTENT_H_
