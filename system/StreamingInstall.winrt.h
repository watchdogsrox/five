//
// system/StreamingInstall.winrt.cpp
//
// Copyright (C) 2014 Rockstar Games.  All Rights Reserved.
//

#ifndef _STREAMING_INSTALL_H
#define _STREAMING_INSTALL_H 

class StreamingInstall
{
public:

    static bool Init();
    static void Shutdown();

    static bool HasInstallFinished();
    static void BlockUntilFinished();

	static void ClearInstalledThisSession() { ms_installedThisSession = false; }
    static bool HasInstalledThisSession() { return ms_installedThisSession; }

#if RSG_DURANGO
	static void AddResumeDelegate();
	static void RemoveResumeDelegate();
	static void InitTransferPackageWatcher();
	static void ShutdownTransferPackageWatcher();
#endif	// RSG_DURANGO

private:

	static void RenderInstallProgress();
	static void CreateStateBlocks();
	ORBIS_ONLY(static bool UpdateSleepWarning());

	static bool ms_installedThisSession;
};

#endif // _STREAMING_INSTALL_H 
