//
// system/BacktraceDumpUpload.winrt.h
//
// Copyright (C) 2022 Rockstar Games.  All Rights Reserved.
//

#pragma once

#if RSG_DURANGO

class BacktraceDumpUpload
{
public:

    static bool UploadDump(const wchar_t* dumpFullPath, const wchar_t* logFullPath, const atMap<atString, atString>* annotations, const char* url);
};

#endif