//
// system/MappingManager.h
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//

#ifndef SYSTEM_MAPPINGMANAGER_H
#define SYSTEM_MAPPINGMANAGER_H

// Rage Headers.
#include "input/mapper.h"

#if ENABLE_INPUT_COMBOS

// Game Headers.
#include "system/MappingManagerData.h"
#include "system/control_mapping.h"

namespace rage
{
RAGE_DECLARE_CHANNEL(MappingManager)

#define mapmgrAssert(cond)					RAGE_ASSERT(MappingManager,cond)
#define mapmgrAssertf(cond,fmt,...)			RAGE_ASSERTF(MappingManager,cond,fmt,##__VA_ARGS__)
#define mapmgrFatalAssertf(cond,fmt,...)	RAGE_FATALASSERTF(MappingManager,cond,fmt,##__VA_ARGS__)
#define mapmgrVerify(cond)					RAGE_VERIFY(MappingManager,cond)
#define mapmgrVerifyf(cond,fmt,...)			RAGE_VERIFYF(MappingManager,cond,fmt,##__VA_ARGS__)
#define mapmgrErrorf(fmt,...)				RAGE_ERRORF(MappingManager,fmt,##__VA_ARGS__)
#define mapmgrWarningf(fmt,...)				RAGE_WARNINGF(MappingManager,fmt,##__VA_ARGS__)
#define mapmgrDisplayf(fmt,...)				RAGE_DISPLAYF(MappingManager,fmt,##__VA_ARGS__)
#define mapmgrDebugf1(fmt,...)				RAGE_DEBUGF1(MappingManager,fmt,##__VA_ARGS__)
#define mapmgrDebugf2(fmt,...)				RAGE_DEBUGF2(MappingManager,fmt,##__VA_ARGS__)
#define mapmgrDebugf3(fmt,...)				RAGE_DEBUGF3(MappingManager,fmt,##__VA_ARGS__)
#define mapmgrLogf(severity,fmt,...)		RAGE_LOGF(MappingManager,severity,fmt,##__VA_ARGS__)

class CMappingManager : public datBase
{
public:
	// PURPOSE: Constructor.
	CMappingManager();

	// PURPOSE: Destructor.
	virtual ~CMappingManager();

	// PURPOSE: Loads a mapping file.
	// PARAMS:  path - the path of the file to load.
	void LoadMappingFile(const char* path);

	// PURPOSE: Unloads a mapping file.
	// PARAMS:  path - the path of the file to be unloaded.
	// NOTES:   This is identical to RemoveMappingFile() except this function updates the bank
	//          widgets. This is generally what you want unless the bank widgets are going to
	//          be updated later.
	void UnloadMappingFile(const char* path);

	// PURPOSE: Update mappings.
	// PARAMS:  timeMS - the time in MS.
	// NOTES:   This needs to be called once per frame.
	void Update(u32 timeMS);

	// PURPOSE: Retrieves an input's name as a c-string.
	// PARAMS:  input - the input to get the name for.
	// RETURNS: the string name of the input (this is the same as the enum name).
	static const char* GetInputName(InputType input);

#if RSG_BANK
	// PURPOSE: Initializes the bank widgets.
	// NOTES:   This function keeps a pointer to bank to update as files are loaded and unloaded.
	void InitWidgets(bkBank& bank);
#endif // RSG_BANK

private:
	// PURPOSE: All the information regarding a combo track.
	// NOTES:   A combo track is what one input source (such as a specific pad button) is doing
	//          during the combo.
	struct ComboTrack
	{
		// PURPOSE: The state a step test can be in.
		enum TestState
		{
			// PURPOSE: The test has not been started yet.
			// NOTES:   An example of this use is if we are looking for a press but the button was
			//          already down when we started.
			NOT_STARTED,

			// PURPOSE: The test has started.
			STARTED,

			// PURPOSE: The test is an ongoing test and it is in progress.
			// NOTES:   An example of its use is looking for a key to remain up. Whilst it is up the
			//          state is in progress. If it goes down then we fail the test. At the end of a
			//          step IN_PROGRESS is treated as PASSED.
			IN_PROGRESS,

			// PURPOSE: The test was passed.
			PASSED,

			// PURPOSE: The test was failed.
			FAILED,
		};

		// PURPOSE: Constructor.
		ComboTrack();

		// PURPOSE: Updates a combo track state.
		// PARAMS:	The step the track should use.
		void Update(u32 step);

		// PURPOSE: The input source.
		ioSource  m_Source;

		// PURPOSE: The step state.
		TestState m_StepState;

		// PURPOSE: The steps of the combo.
		atArray<ControlInput::Trigger>	m_Steps;

#if RSG_BANK
		// PURPOSE: Get the test state color.
		// PARAMS:  state - the state to get the color for.
		// RETURNS: the requested color.
		static Color32     GetTestStateColor(TestState state);

		// PURPOSE: Gets the test state c-string.
		// PARAMS:  state - the state to get the string for.
		// RETURNS: the requested c-string.
		static const char* GetTestStateStr(TestState state);
#endif // RSG_BANK
	};

	// PURPOSE: All the information for a combo mapping.
	struct ComboMapping
	{
		// PURPOSE: Constructor.
		ComboMapping();

		// PURPOSE: Represents an invalid track id.
		const static u32 INVALID_TRACK_ID = 0xffffffff;

		// PURPOSE: Updates the mapping.
		// NOTES:   Must be called once per frame.
		void Update();

		// PURPOSE: Restarts the combo.
		void Restart();

		// PURPOSE: Progress the combo onto the next step.
		// NOTES:   At the end of the combo, this will mark the combo as completed.
		void ProgressStep();

		// PURPOSE: Loads the combo information.
		// PARAMS:  mapping - the parser structure containing the mapping information.
		void Load(const ControlInput::ComboMapping &mapping);

		// PURPOSE: The combo running states.
		enum RunningState
		{
			// PURPOSE: The combo has stopped as it was failed It is waiting to be restarted.
			FAILED_WAITING,

			// PURPOSE: The combo is running.
			RUNNING,

			// PURPOSE: The combo was completed successfully. It is waiting to be restarted.
			SUCCESS_WAITING,
		};

#if RSG_BANK
		// PURPOSE: Gets a string representing a combo running state.
		// NOTES:   state - the state to get a string name of.
		static const char* GetRunningStateStr(RunningState state);

		// PURPOSE: Gets a color representing the combo running state.
		// NOTES:   state - the state to get a color for.
		static Color32     GetRunningStateColor(RunningState state);
#endif // RSG_BANK

		// PURPOSE: The input this combo is mapped to.
		InputType			m_Input;

		// PURPOSE: The current step this combo is on.
		u32					m_CurrentStep;

		// PURPOSE: The time the step was started.
		u32					m_StepStartTime;

		// PURPOSE: The duration of the combo steps.
		u32					m_TimeDelta;

		// PURPOSE: The combo mapping running state.
		RunningState		m_RunningState;

		// PURPOSE: The tracks of the combo.
		// NOTES:   A combo track is what one input source (such as a specific pad button) is
		//          doing during the combo.
		atArray<ComboTrack>	m_Tracks;
	};

	// PURPOSE: A combo group.
	// NOTES:   A combo group contains a group of mutually exclusive combo mappings. Only one
	//          combo in the group can fire/succeed at a time. This is based on the order of the
	//          mappings with the mappings at the top taking priority. If a combo passes, it will
	//          wait if there are any higher combos running. If no higher combos are running
	//          then the combo mapping will trigger the input. The whole sequence is then restarted.
	class ComboGroup
	{
	public:
		// PURPOSE: Constructor.
		ComboGroup();

		// PURPOSE: Represents an invalid combo mapping id.
		const static u32 INVALID_COMBO_MAPPING_ID = 0xffffffff;

		// PURPOSE: Retrieves the group ID.
		atHashWithStringNotFinal GetId() const;

		// PURPOSE: Updates a mapping group.
		// RETURNS: The mapping id that has completed or INVALID_COMBO_MAPPING_ID if not combo has completed.
		u32 Update();

		// PURPOSE: Restarts a group.
		void Restart();

		// PURPOSE: Retrieves an input associated with a combo id.
		// PARAMS:  comboId - the combo id to retrieve the input from.
		// RETURNS: the mapped input or UNDEFINED_INPUT if comboId is not valid.
		InputType GetComboInput(u32 comboId) const;

		// PURPOSE: Tries to retrieve a source for a combo id.
		// PARAMS:  comboId - the combo id to try and retrieve the source from.
		// RETURNS: the source input or ioSource::UNKNOWN_SOURCE if it could not be extracted.
		// NOTES:   A source can only be retrieved if the combo only uses one source or only one
		//          source is used (down/pressed) for the final step of the combo.
		ioSource  GetComboSource(u32 comboId) const;

		// PURPOSE: Loads a mapping group.
		// PARAMS:  id - the id of the group.
		//          mappingData - the parser group mapping data to be loaded.
		void Load(atHashWithStringNotFinal id, const ControlInput::MappingData& mappingData);

		// PURPOSE: Unloads the group data.
		void Unload();


#if RSG_BANK
		// PURPOSE: Initializes the bank widgets.
		void InitWidgets(bkBank& bank);

		// PURPOSE: Updates debug info.
		void UpdateDebugInfo();

		// PURPOSE: Draws a combo progress to the screen.
		// PARAMS:  comboId - the comboId to draw.
		void DebugDrawCombo(u32 comboId) const;

		// PURPOSE: Draws the state of the whole group to the screen.
		void DebugDrawComboList() const;
#endif // RSG_BANK

	private:
		// These is used to track down if a combo waits too long as the result of higher priority combos
		// cycling in progress.
#if RSG_ASSERT
		// PURPOSE: The time a combo is passed as started waiting (PASSED_WAITING).
		u32	m_WaitStartTime;

		// PURPOSE: The duration the passed combo (PASSED_WAITING) should wait for before asserting.
		u32	m_WaitDuration;
#endif // RSG_ASSERT

#if RSG_BANK
		// PURPOSE: The id of the mapping to display debug info for.
		// NOTES:   0 represents all mappings (show the whole group). As a result, this id is offset by 1.
		int m_DebugMapping;
#endif // RSG_BANK

		// PURPOSE: The id of the group.
		atHashWithStringNotFinal m_Id;

		// PURPOSE: The combo's in the group.
		atArray<ComboMapping>	 m_ComboMappings;
	};

	// PURPOSE: Represents an invalid groups id.
	const static u32 INVALID_GROUP_ID = 0;

	// PURPOSE: Internal function to remove a mapping.
	// PARAMS:  path - the path of the mapping to be removed.
	// NOTES:   UnloadMappingFile() calls this but UnloadMappingFile() also updates the bank widgets whereas this function
	//          does not. Only call this if they are going to be updated later.
	void RemoveMappingFile(const char* path);

	// PURPOSE: Converts a parser InputSource to an rage ioSource.
	// NOTES:   inputSource - the source to be converted.
	// RETURNS: The converted ioSource if possible or ioSource::UNKNOWN_SOURCE if it could not be converted.
	static ioSource ConvertSource(const ControlInput::InputSource& inputSource);

	// PURPOSE: Converts a file path to an id.
	// PARAMS:  path - the path to be converted.
	// RETURNS: The path as an id.
	// NOTES:   This function does some path cleaning (such as changing \ to /). This function was created as the parser
	//          adds the path strings to the hash string table but it also cleans up the path. This causes the two strings
	//          'x:\test.meta' and 'x:/test.meta' to have the same hash but different strings causing an assert.
	//
	//          This function calls the same function that the parser does to do this even though its not really necessary.
	static atHashWithStringNotFinal GetPathHash(const char* path);

	// PURPOSE: The inputs that are used for the mappings.
	atRangeArray<ioValue, MAX_INPUTS> m_Inputs;

	// PURPOSE: Update id.
	u8 m_UpdateId;

	// PURPOSE: The different mapping groups.
	// NOTES:   Only one mapping in a mapping group can trigger at a time.
	atArray<ComboGroup>	m_MappingGroups;

#if RSG_BANK
	// PURPOSE: Updates the debug display.
	void UpdateDebugDisplay();

	// PURPOSE: Rag callback to load a mapping file.
	void DebugLoadMappingfile();

	// PURPOSE: Rebuilds the rag widgets.
	void RebuildWidgets();

	// PURPOSE: Gets a Trigger as a c-string.
	// PARAMS:  trigger - the trigger to get the c-string name of.
	// RETURNS: the requested c-string.
	// NOTES:   The string returned is the enum as a string.
	static const char* GetTriggerName(ControlInput::Trigger trigger);

	// PURPOSE: On screen debugging enabled state.
	bool     m_EnableDebugging;

	// PURPOSE: The group to be display debug info for.
	int      m_DebugGroup;

	// PURPOSE: A pointer to the bank group created containing all the debug widgets.
	// NOTES:   This is kept so we can remove it when the mappings are updated.
	bkGroup* m_BankGroup;
	
	// PURPOSE: A pointer to the parent bank which we use to add the rag widgets to.
	// NOTES:   This is kept so we can update the widgets.
	bkBank*  m_BankParent;

	// PURPOSE: A path to a file to be loaded from the bank widget.
	char     m_FilePath[RAGE_MAX_PATH];
#endif // RSG_BANK
};

inline CMappingManager::CMappingManager()
	: m_UpdateId(0)
#if RSG_BANK
	, m_EnableDebugging(false)
	, m_DebugGroup(0)
	, m_BankGroup(NULL)
	, m_BankParent(NULL)
#endif // RSG_BANK
{
	BANK_ONLY(m_FilePath[0] = '\0');
}

inline CMappingManager::~CMappingManager()
{}

inline void CMappingManager::UnloadMappingFile(const char* path)
{
	RemoveMappingFile(path);
	BANK_ONLY(RebuildWidgets());
}

inline CMappingManager::ComboTrack::ComboTrack()
	: m_Source()
	, m_StepState(NOT_STARTED)
{
}

inline CMappingManager::ComboMapping::ComboMapping()
	: m_Input(UNDEFINED_INPUT)
	, m_TimeDelta(0u)
{
	Restart();
}

inline CMappingManager::ComboGroup::ComboGroup()
	: m_Id(0u)
#if RSG_ASSERT
	, m_WaitStartTime(0u)
	, m_WaitDuration(0u)
#endif // RSG_ASSERT
#if RSG_BANK
	, m_DebugMapping(0)
#endif // RSG_BANK
{
}

inline atHashWithStringNotFinal CMappingManager::ComboGroup::GetId() const
{
	return m_Id;
}

}

#endif // ENABLE_INPUT_COMBOS

#endif // SYSTEM_MAPPINGMANAGER_H