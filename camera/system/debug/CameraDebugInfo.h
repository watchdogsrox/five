//
// camera/system/debug/CameraDebugInfo.h
// 
// Copyright (C) 1999-2022 Rockstar Games.  All Rights Reserved.
//
#ifndef CAMERA_SYSTEM_DEBUG_CAMERA_DEBUG_INFO_H
#define CAMERA_SYSTEM_DEBUG_CAMERA_DEBUG_INFO_H

#include "ai/debug/system/AIDebugInfo.h"

#if AI_DEBUG_OUTPUT_ENABLED

class camBaseCamera;
class camFrame;

class CCameraDebugInfo : public CAIDebugInfo
{
public:
	CCameraDebugInfo(DebugInfoPrintParams printParams = DebugInfoPrintParams());
	virtual ~CCameraDebugInfo();

	void Print() override;

private:
	void PrintCameraInfo(const camBaseCamera& camera, const char* label, Color32 color);
	void PrintCameraFrameInfo(const camFrame& frame, const char* label, Color32 color);
	void PrintViewModes(const camBaseCamera& camera);
};

#endif //AI_DEBUG_OUTPUT_ENABLED

#endif //CAMERA_SYSTEM_DEBUG_CAMERA_DEBUG_INFO_H