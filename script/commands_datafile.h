// 
// savegame/savegame_datadict_script.h 
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved. 
// 
#ifndef SAVEGAME_DATADICT_SCRIPT_H
#define SAVEGAME_DATADICT_SCRIPT_H H

namespace datafile_commands {
	void SetupScriptCommands();
	void InitialiseThreadPool();
	void ShutdownThreadPool();

#if __DEV && 0
	void TestLoadFilesInDir(const char* dirName);
#endif
};

#endif	// SAVEGAME_DATADICT_SCRIPT_H
