//
// debug/FrameDump.h
//
// Copyright (C) 2012 Rockstar Games.  All Rights Reserved.
//
// Helper class to dump rendered game frames as individual screenshots.
// Intended particularly for use with cutscenes.

#if __BANK && !defined(_FRAME_DUMP_H_)
#define _FRAME_DUMP_H_

class CFrameDump
{
public:

	// PURPOSE
	//  Initiates capture of subsequent game frames.  If a custscene is
	//  playing, the capture will be synchronized such that one file will
	//  be saved out per MotionBuilder frame.
	// PARAMS
	//  pathPrefix  - The first part of the path for output files.
	//                "_xxxx.jpg" or "_xxxx.png" will be appended to this,
	//                where xxxx is the frame number.
	//  useJpeg     - Whether to use JPEG (true) or PNG (false)
	//  singleFrame - Whether to capture only a single frame or not.
    //  waitForCutscene - Whether to wait for cutscene start before capturing (continuous capture only)
	//	MBSync		- MotionBuilder frame sync
	//	fps			- Forced FPS capture speed
	//	recordCutscene	- Pointer to a bool for the record tickbox
	static void StartCapture(const char* pathPrefix, bool useJpeg, bool singleFrame = false, bool waitForCutscene = false, bool useCutsceneNameForPath = false, bool MBSynch = true, bool binkMode = false, bool cutsceneMode = false, float fps = 30.0f, bool* recordCutscene = NULL);

	// PURPOSE
	//  Stops the capture process after finishing any frame currently being
	//  rendered.
	static void StopCapture();

	// PURPOSE
	//  Perform update for the main update thread.
	static void Update();

	static bool IsCapturing() { return pathPrefix != NULL; }
	static bool GetBinkMode() { return binkMode; }
	static bool GetCutsceneMode() { return cutsceneMode; }

private:
	static const char*      pathPrefix;
	static bool             useJpeg;
	static bool             wasCapturing;
	static bool             singleFrameCapture;
    static bool             waitForCutscene;
	static bool				useCutsceneNameForPath;
	static bool				MBSync;
	static bool				binkMode;
	static bool				cutsceneMode;
	static bool				firstRun;

	static bool*			recordCutscene;

};

#endif