//
// system/MappingManager.cpp
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//

#include "MappingManager.h"

#if ENABLE_INPUT_COMBOS

// Rage Headers.
#include "math/amath.h"
#include "parser/macros.h"
#include "parsercore/settings.h"

extern ::rage::parEnumData parser_rage__InputType_Data;
extern ::rage::parEnumData parser_rage__ControlInput__Trigger_Data;

namespace rage
{
RAGE_DEFINE_CHANNEL(MappingManager)


void CMappingManager::LoadMappingFile(const char* path)
{
	RemoveMappingFile(path);

	// Find the first available index in the mappings!
	ComboGroup* group = NULL;
	for(u32 i = 0; i < m_MappingGroups.size(); ++i)
	{
		if(m_MappingGroups[i].GetId() == INVALID_GROUP_ID)
		{
			group = &m_MappingGroups[i];
			break;
		}
	}

	if(group == NULL)
	{
		group = &m_MappingGroups.Grow();
	}

	parSettings settings;
	settings.SetFlag(parSettings::READ_SAFE_BUT_SLOW, true);
	
	INIT_PARSER;
	ControlInput::MappingData mappingData;
	if(mapmgrVerifyf(PARSER.LoadObject(path, "", mappingData, &settings), "Failed to mapping file '%s'!", path))
	{
		group->Load(GetPathHash(path), mappingData);
	}
	SHUTDOWN_PARSER;

	BANK_ONLY(RebuildWidgets());
}

void CMappingManager::RemoveMappingFile(const char* path)
{
	const atHashWithStringNotFinal fileId = GetPathHash(path);
	for(u32 i = 0; i < m_MappingGroups.size(); ++i)
	{
		if(m_MappingGroups[i].GetId() == fileId)
		{
			m_MappingGroups[i].Unload();
			return;
		}
	}
}

void CMappingManager::Update(u32 timeMS)
{
	++m_UpdateId;

	for(u32 i = 0; i < MAX_INPUTS; ++i)
	{
		m_Inputs[i].Update(m_UpdateId, timeMS);
	}

	for(u32 i = 0; i < m_MappingGroups.size(); ++i)
	{
		ComboGroup& group = m_MappingGroups[i];
		if(group.GetId().GetHash() != INVALID_GROUP_ID)
		{
			const u32 winningCombo = group.Update();
			if(winningCombo != ComboGroup::INVALID_COMBO_MAPPING_ID)
			{
				const InputType input = group.GetComboInput(winningCombo);
				if(mapmgrVerifyf(input != UNDEFINED_INPUT, "Unknown input (%d)!", input))
				{
					mapmgrDebugf1( "%s fired in combo group %u from combo %u from '%s'.",
						GetInputName(input),
						i,
						winningCombo,
						group.GetId().TryGetCStr() );
					m_Inputs[input].SetCurrentValue(ioValue::MAX_AXIS, group.GetComboSource(winningCombo));
				}
				group.Restart();
			}
		}
	}

	BANK_ONLY(UpdateDebugDisplay());
}


rage::ioSource CMappingManager::ConvertSource(const ControlInput::InputSource& inputSource)
{
	ioSource source;

	// TR: TODO: Use correct index!
	source.m_DeviceIndex = 0; 
	source.m_Device = inputSource.m_Type;

	// TR: TODO: Convert parameters.
	source.m_Parameter = ioMapper::ConvertParameterToDeviceValue(inputSource.m_Type, inputSource.m_Parameters[0]);

	return source;
}

atHashWithStringNotFinal CMappingManager::GetPathHash(const char* path)
{
	// As we hash the path as the key, we need to ensure our hash matches the parser otherwise we get an assert about a hash collision
	// if are slashes do not match what the parser uses. The parser calls ASSET.AddExtension() that normalizes the path.
	char fullPath[RAGE_MAX_PATH] = {0};
	ASSET.AddExtension(fullPath, RAGE_MAX_PATH, path, "");
	return atHashWithStringNotFinal(fullPath);
}

const char* CMappingManager::GetInputName(InputType input)
{
	if(mapmgrVerifyf(input >= 0 && input < MAX_INPUTS && input != UNDEFINED_INPUT, "Invalid input!"))
	{
		return parser_rage__InputType_Data.m_Names[input];
	}

	return "";
}

void CMappingManager::ComboTrack::Update(u32 step)
{
	if(mapmgrVerifyf(step < m_Steps.size(), "Invalid step index!"))
	{
		switch(m_Steps[step])
		{
		case ControlInput::DOWN:
			if(Abs(ioMapper::GetSourceValue(m_Source, false)) >= ioValue::BUTTON_DOWN_THRESHOLD)
			{
				m_StepState = IN_PROGRESS;
			}
			else
			{
				m_StepState = FAILED;
			}
			break;

		case ControlInput::UP:
			if(Abs(ioMapper::GetSourceValue(m_Source, false)) < ioValue::BUTTON_DOWN_THRESHOLD)
			{
				m_StepState = IN_PROGRESS;
			}
			else
			{
				m_StepState = FAILED;
			}
			break;

		case ControlInput::PRESSED:
			if(Abs(ioMapper::GetSourceValue(m_Source, false)) >= ioValue::BUTTON_DOWN_THRESHOLD)
			{
				if(m_StepState != NOT_STARTED)
				{
					m_StepState = PASSED;
				}
			}
			else
			{
				m_StepState = STARTED;
			}
			break;

		case ControlInput::RELEASED:
			if(Abs(ioMapper::GetSourceValue(m_Source, false)) < ioValue::BUTTON_DOWN_THRESHOLD)
			{
				if(m_StepState != NOT_STARTED)
				{
					m_StepState = PASSED;
				}
			}
			else
			{
				m_StepState = STARTED;
			}
			break;

		case ControlInput::NONZERO:
			{
				float val = ioMapper::GetSourceValue(m_Source, false);
				if(ioValue::RequiresDeadZone(m_Source))
				{
					val = ioAddDeadZone(val, ioValue::DEFAULT_DEAD_ZONE_VALUE);
				}

				if(Abs(val) != 0.0f)
				{
					m_StepState = PASSED;
				}
				else
				{
					m_StepState = NOT_STARTED;
				}
			}
			break;

		case ControlInput::ANY_STATE:
			m_StepState = PASSED;
			break;

		default:
			mapmgrAssertf(false, "Unknown mapping test type!");
			m_StepState = FAILED;
			break;
		}
	}
}

void CMappingManager::ComboMapping::Update()
{
	mapmgrDebugf3("Testing step %u.", m_CurrentStep);

	if(mapmgrVerifyf(m_Tracks.size() > 0, "Mapping has no tracks!") && m_RunningState == RUNNING)
	{
		bool passedAllTracks = true;
		bool trackNotStarted = false;
		bool failedATrack	 = false;

		for(u32 trackIndex = 0; trackIndex < m_Tracks.size(); ++trackIndex)
		{
			mapmgrDebugf3("Testing step %u track %u.", m_CurrentStep, trackIndex);
			ComboTrack& track = m_Tracks[trackIndex];
			track.Update(m_CurrentStep);

			if(track.m_StepState == ComboTrack::FAILED)
			{
				mapmgrDebugf3("Step %u track %u failed.", m_CurrentStep, trackIndex);
				failedATrack	= true;
				passedAllTracks = false;

				// This track has not passed the combo so stop checking others.
				break;
			}
			else if(track.m_StepState == ComboTrack::NOT_STARTED)
			{
				mapmgrDebugf3("Step %u track %u not started.", m_CurrentStep, trackIndex);
				trackNotStarted = true;
				passedAllTracks = false;
			}
			else if(track.m_StepState != ComboTrack::PASSED)
			{
				mapmgrDebugf3("Step %u track %u not finished (state %d).", m_CurrentStep, trackIndex, track.m_StepState);
				passedAllTracks = false;
			}
			else
			{
				mapmgrDebugf3( "Step %u track %u passed.", m_CurrentStep, trackIndex);
			}
		}

		// The first state can start at any time so if we are waiting restart the timer.
		if(failedATrack)
		{
			mapmgrDebugf3("Failed a track, stopping combo.");
			m_RunningState = FAILED_WAITING;
		}
		else if(m_CurrentStep == 0 && trackNotStarted)
		{
			mapmgrDebugf3("On first step or no start time, restarting timer.");
			m_StepStartTime = fwTimer::GetSystemTimeInMilliseconds();
		}
		else if( passedAllTracks ||
			m_StepStartTime == 0u ||
			(m_StepStartTime + m_TimeDelta) < fwTimer::GetSystemTimeInMilliseconds() )
		{
			mapmgrDebugf3("Step %u has finished or failed, progressing combo step.", m_CurrentStep);
			ProgressStep();
		}
	}
}

void CMappingManager::ComboMapping::Restart()
{
	m_RunningState	= RUNNING;
	m_StepStartTime = fwTimer::GetSystemTimeInMilliseconds();
	m_CurrentStep   = 0;

	for(u32 i = 0; i < m_Tracks.size(); ++i)
	{
		m_Tracks[i].m_StepState = ComboTrack::NOT_STARTED;
	}
}

void CMappingManager::ComboMapping::ProgressStep()
{
	bool passed = true;
	m_StepStartTime = fwTimer::GetSystemTimeInMilliseconds();
	for(u32 i = 0; i < m_Tracks.size(); ++i)
	{
		ComboTrack& track = m_Tracks[i];
		if(track.m_StepState != ComboTrack::PASSED && track.m_StepState != ComboTrack::IN_PROGRESS)
		{
			mapmgrDebugf3("Step %u track %u not passed and not in progress.", m_CurrentStep, i);
			passed = false;
		}

		m_Tracks[i].m_StepState = ComboTrack::NOT_STARTED;
	}

	if(passed)
	{
		mapmgrDebugf2("Combo step passed, progressing from step %u.", m_CurrentStep);

		if(m_CurrentStep >= (m_Tracks[0].m_Steps.size() -1))
		{
			mapmgrDebugf1("Combo mapped to %s completed successfully.", CMappingManager::GetInputName(m_Input));
			m_RunningState = SUCCESS_WAITING;
		}
		else
		{
			++m_CurrentStep;
		}
	}
	else
	{
		mapmgrDebugf3("Resetting combo step to 0.");
		m_CurrentStep = 0;
		m_RunningState = FAILED_WAITING;
	}
}

void CMappingManager::ComboMapping::Load(const ControlInput::ComboMapping &mapping)
{
	m_Input		   = mapping.m_Input;
	m_TimeDelta	   = mapping.m_TimeDelta;
	m_CurrentStep  = 0u;

	// We use Reset() rather than ResetCount() because m_Tracks is an array of an array and we dont want extra
	// unused memory lying around! We will not likely do this often so its best free the memory.
	m_Tracks.Reset();

	// We need to find out exactly how and which sources are used in the combo.
	for(u32 stepIndex = 0; stepIndex < mapping.m_Steps.size(); ++stepIndex)
	{
		const ControlInput::ComboStep& step = mapping.m_Steps[stepIndex];
		for(u32 stateIndex = 0; stateIndex < step.m_States.size(); ++stateIndex)
		{
			const ControlInput::ComboState& state = step.m_States[stateIndex];

			const ioSource stateSource = ConvertSource(state.m_Source);
			u32 trackId = INVALID_TRACK_ID;

			// Find the track that represents this source.
			for(u32 trackIndex = 0; trackIndex < m_Tracks.size(); ++trackIndex)
			{
				const ioSource& trackSource = m_Tracks[trackIndex].m_Source;
				if(trackSource.m_Device == stateSource.m_Device && trackSource.m_Parameter == stateSource.m_Parameter)
				{
					trackId = trackIndex;
					break;
				}
			}

			if(trackId == INVALID_TRACK_ID)
			{
				// This is a new source for the combo so add a new track.
				trackId = m_Tracks.size();
				ComboTrack& track = m_Tracks.Grow();

				track.m_Source = stateSource;
				track.m_Steps.Reserve(mapping.m_Steps.size());

				for(u32 paddingIndex = 0; paddingIndex < stepIndex; ++paddingIndex)
				{
					track.m_Steps.Push(ControlInput::ANY_STATE);
				}
			}

			m_Tracks[trackId].m_Steps.Push(state.m_Trigger);
		}

		// Now fill in any tracks with out a step as any state.
		for(u32 trackIndex = 0; trackIndex < m_Tracks.size(); ++trackIndex)
		{
			ComboTrack& track = m_Tracks[trackIndex];
			while(track.m_Steps.size() < (stepIndex+1))
			{
				track.m_Steps.Push(ControlInput::ANY_STATE);
			}
		}
	}
}

u32 CMappingManager::ComboGroup::Update()
{
	bool hasHigherRunningCombo = false;

	for (u32 mappingIndex = 0; mappingIndex < m_ComboMappings.size(); ++mappingIndex)
	{
		ComboMapping& mapping = m_ComboMappings[mappingIndex];

		if(mapping.m_RunningState == ComboMapping::FAILED_WAITING)
		{
			mapmgrDebugf3( "Combo mapping %u mapped to %s in '%s' has stopped, restarting.",
				mappingIndex,
				CMappingManager::GetInputName(mapping.m_Input),
				m_Id.TryGetCStr() );

			mapping.Restart();
		}
		
		if(mapping.m_RunningState == ComboMapping::RUNNING)
		{
			mapmgrDebugf3( "Updating combo mapping %u mapped to %s in '%s'.",
				mappingIndex,
				CMappingManager::GetInputName(mapping.m_Input),
				m_Id.TryGetCStr() );

			mapping.Update();
			if(mapping.m_RunningState == ComboMapping::RUNNING)
			{
				hasHigherRunningCombo = true;
			}
		}

		if(mapping.m_RunningState == ComboMapping::SUCCESS_WAITING)
		{
			if(hasHigherRunningCombo == false)
			{
				// We have a winner so stop.
				mapmgrDebugf2( "Combo mapping %u mapped to %s completed in group '%s', finishing.",
					mappingIndex,
					CMappingManager::GetInputName(mapping.m_Input),
					m_Id.TryGetCStr() );

				return mappingIndex;
			}
			else
			{
				mapmgrDebugf2( "Combo mapping %u mapped to %s completed but a higher priority combo is still running in group '%s'.",
					mappingIndex,
					CMappingManager::GetInputName(mapping.m_Input),
					m_Id.TryGetCStr() );

#if RSG_ASSERT
				// This is to detect input deadlocks where a combo has passed but waiting for more important combos to complete.
				// It is possible that the more important combos keep cycling between running and failed so this combo will never
				// fire. We assert if this happens as its a design flaw in the combo sequences.
				if(m_WaitStartTime == 0)
				{
					m_WaitStartTime = fwTimer::GetSystemTimeInMilliseconds();
				}
				else
				{
					mapmgrAssertf( (m_WaitStartTime + m_WaitDuration) > fwTimer::GetSystemTimeInMilliseconds(),
						"A mapping in to %s in '%s' has passed but multiple higher priority combos continuing to run. This is likely a design flaw in the combos!",
						CMappingManager::GetInputName(mapping.m_Input),
						m_Id.TryGetCStr() );
				}
#endif // RSG_ASSERT
			}
		}
	}

	return INVALID_COMBO_MAPPING_ID;
}

rage::InputType CMappingManager::ComboGroup::GetComboInput(u32 comboId) const
{
	if( mapmgrVerifyf(comboId != INVALID_COMBO_MAPPING_ID, "Invalid combo id used to retrieve input!") &&
		mapmgrVerifyf(comboId < m_ComboMappings.size(), "Out of range combo id used to retrieve input!"))
	{
		return m_ComboMappings[comboId].m_Input;
	}
	else
	{
		return UNDEFINED_INPUT;
	}
}

rage::ioSource CMappingManager::ComboGroup::GetComboSource(u32 comboId) const
{
	if( mapmgrVerifyf(comboId != INVALID_COMBO_MAPPING_ID, "Invalid combo id used to retrieve input!") &&
		mapmgrVerifyf(comboId < m_ComboMappings.size(), "Out of range combo id used to retrieve input!"))
	{
		const ComboMapping& mapping = m_ComboMappings[comboId];
		if(mapping.m_Tracks.size() == 1)
		{
			return mapping.m_Tracks[0].m_Source;
		}
		else if(mapmgrVerifyf(mapping.m_Tracks.size() > 0, "No tracks for mapping!"))
		{
			const s32 stepSize = mapping.m_Tracks[0].m_Steps.size();

			u32 sourceTrack = ComboMapping::INVALID_TRACK_ID;

			// We need to search the last steps
			for (u32 trackIndex = 0; trackIndex < mapping.m_Tracks.size(); ++trackIndex)
			{
				const ComboTrack& track = mapping.m_Tracks[trackIndex];
				if(mapmgrVerifyf(track.m_Steps.size() ==  stepSize, "Wrong number of combo track!"))
				{
					ControlInput::Trigger trigger = track.m_Steps[stepSize-1];
					if(trigger == ControlInput::DOWN || trigger == ControlInput::PRESSED)
					{
						// Save the track to use for the source index.
						if(sourceTrack == ComboMapping::INVALID_TRACK_ID)
						{
							sourceTrack = trackIndex;
						}
						// There are more than one so stop and use none of them.
						else
						{
							sourceTrack = ComboMapping::INVALID_TRACK_ID;
							break;
						}
					}
				}
			}

			if(sourceTrack != ComboMapping::INVALID_TRACK_ID)
			{
				return mapping.m_Tracks[sourceTrack].m_Source;
			}
		}
	}

	return ioSource::UNKNOWN_SOURCE;
}

void CMappingManager::ComboGroup::Load(atHashWithStringNotFinal id, const ControlInput::MappingData& mappingData)
{
	Unload();

	m_Id = id;

	ASSERT_ONLY(m_WaitDuration = 0u);

	m_ComboMappings.Reserve(mappingData.m_ComboMappings.size());
	for(u32 mappingIndex = 0; mappingIndex < mappingData.m_ComboMappings.size(); ++mappingIndex)
	{
		const ControlInput::ComboMapping& mapping = mappingData.m_ComboMappings[mappingIndex];
		ComboMapping& newMapping = m_ComboMappings.Append();
		newMapping.Load(mapping);

#if RSG_ASSERT
		// Get the full duration of the longest running combo.
		if(newMapping.m_Tracks.size() > 0)
		{
			u32 duration = newMapping.m_Tracks[0].m_Steps.size() * newMapping.m_TimeDelta;
			if(duration > m_WaitDuration)
			{
				m_WaitDuration = duration;
			}
		}
#endif // RSG_ASSERT
	}

#if RSG_ASSERT
	// Verify data structures are valid!
	for (u32 mappingIndex = 0; mappingIndex < m_ComboMappings.size(); ++mappingIndex)
	{
		const ComboMapping& mapping = m_ComboMappings[mappingIndex];
		if(mapmgrVerifyf(mapping.m_Tracks.size() > 0, "A mapping has been loaded with no input mappings!"))
		{
			const s32 size = mapping.m_Tracks[0].m_Steps.size();
			for(u32 trackIndex = 1; trackIndex < mapping.m_Tracks.size(); ++trackIndex)
			{
				const ComboTrack& track = mapping.m_Tracks[trackIndex];
				mapmgrAssertf( size == track.m_Steps.size(),
					"Mapping %u has tracks of different sizes (%u and %u) inside '%s'!",
					mappingIndex,
					size,
					track.m_Steps.size(),
					id.TryGetCStr() );

				for(u32 stepIndex = 0; stepIndex < track.m_Steps.size(); ++stepIndex)
				{
					mapmgrAssertf( track.m_Steps[stepIndex] != ControlInput::INVALID_TRIGGER,
						"Invalid trigger on mapping %u track %u step %u! This is probably a typo in '%s'!",
						mappingIndex,
						trackIndex,
						stepIndex,
						id.TryGetCStr() );
				}
			}
		}
	}
#endif // RSG_ASSERT
}


void CMappingManager::ComboGroup::Unload()
{
	m_Id = "";

	// We use Reset() rather than ResetCount() because m_Tracks is an array of an array and we don't want extra
	// unused memory lying around! We will not likely do this often so it's best to free the memory.
	m_ComboMappings.Reset();
}

void CMappingManager::ComboGroup::Restart()
{
	ASSERT_ONLY(m_WaitStartTime = 0);

	for(u32 i = 0; i < m_ComboMappings.size(); ++i)
	{
		m_ComboMappings[i].Restart();
	}
}

#if RSG_BANK
void CMappingManager::InitWidgets(bkBank& bank)
{
	// Reset any currently displaying debug data.
	m_DebugGroup      = 0;
	m_EnableDebugging = false;

	// Store the bank to update later.
	m_BankParent = &bank;
	m_BankGroup  = bank.PushGroup("Mapping Manager");

	bank.AddText("Path", m_FilePath, RAGE_MAX_PATH);
	bank.AddButton("Load File", datCallback(MFA(CMappingManager::DebugLoadMappingfile), this));

	bank.AddToggle("EnableDebugging", &m_EnableDebugging);

	const char** groupNames = Alloca(const char*, m_MappingGroups.size());
	int groupAmount = 0;
	for(u32 i = 0; i < m_MappingGroups.size(); ++i)
	{
		if(m_MappingGroups[i].GetId().GetHash() != INVALID_GROUP_ID)
		{
			groupNames[i] = m_MappingGroups[i].GetId().TryGetCStr();
			++groupAmount;
		}
	}

	bank.AddCombo( "Debug Group",
		&m_DebugGroup,
		groupAmount,
		groupNames );

	for(u32 i = 0; i < m_MappingGroups.size(); ++i)
	{
		if(m_MappingGroups[i].GetId().GetHash() != INVALID_GROUP_ID)
		{
			m_MappingGroups[i].InitWidgets(bank);
		}
	}

	bank.PopGroup();
}

void CMappingManager::RebuildWidgets()
{
	if(m_BankGroup && m_BankParent)
	{
		m_BankParent->Remove(*m_BankGroup);
		m_BankGroup = NULL;
		InitWidgets(*m_BankParent);
	}
}

void CMappingManager::UpdateDebugDisplay()
{
	if( m_EnableDebugging &&
		m_DebugGroup < m_MappingGroups.size() &&
		m_MappingGroups[m_DebugGroup].GetId().GetHash() != INVALID_GROUP_ID )
	{
		m_MappingGroups[m_DebugGroup].UpdateDebugInfo();
	}
}

const char* CMappingManager::GetTriggerName(ControlInput::Trigger trigger)
{
	if(mapmgrVerifyf(trigger >= ControlInput::INVALID_TRIGGER && trigger <= ControlInput::ANY_STATE, "Invalid trigger!"))
	{
		return parser_rage__ControlInput__Trigger_Data.m_Names[trigger];
	}

	return "Unknown";
}

void CMappingManager::DebugLoadMappingfile()
{
	LoadMappingFile(m_FilePath);
}

const char* CMappingManager::ComboTrack::GetTestStateStr(TestState state)
{
	switch(state)
	{
	case NOT_STARTED:
		return "Not Started";

	case STARTED:
		return "Started";

	case IN_PROGRESS:
		return "In Progress";

	case PASSED:
		return "Passed";

	case FAILED:
		return "Failed";

	default:
		mapmgrAssertf(false, "Unknown TestState (%d)!", state);
		return "Unknown";
	}
}

rage::Color32 CMappingManager::ComboTrack::GetTestStateColor(TestState state)
{
	switch(state)
	{
	case NOT_STARTED:
		return Color_DarkOrange;

	case STARTED:
		return Color_orange;

	case IN_PROGRESS:
		return Color_DarkOliveGreen;

	case PASSED:
		return Color_green;

	case FAILED:
		return Color_red;

	default:
		mapmgrAssertf(false, "Unknown TestState (%d)!", state);
		return Color_red;
	}
}

const char* CMappingManager::ComboMapping::GetRunningStateStr(RunningState state)
{
	switch(state)
	{
	case FAILED_WAITING:
		return "Failed/Stopped";

	case RUNNING:
		return "Running";

	case SUCCESS_WAITING:
		return "Success/Waiting";

	default:
		mapmgrAssertf(false, "Unknown RunningState (%d)!", state);
		return "Unknown";
	}
}

rage::Color32 CMappingManager::ComboMapping::GetRunningStateColor(RunningState state)
{
	switch(state)
	{
	case FAILED_WAITING:
		return Color_red;

	case RUNNING:
		return Color_orange;

	case SUCCESS_WAITING:
		return Color_green;

	default:
		mapmgrAssertf(false, "Unknown RunningState (%d)!", state);
		return Color_red;
	}
}

void CMappingManager::ComboGroup::InitWidgets(bkBank& bank)
{
	bank.PushGroup(m_Id.TryGetCStr());
	
	// + 1 for the for the whole group.
	const char** mappingNames = Alloca(const char*, m_ComboMappings.size() + 1);
	mappingNames[0] = "All Mappings";
	for(u32 i = 0; i < m_ComboMappings.size(); ++i)
	{
		mappingNames[i + 1] = CMappingManager::GetInputName(m_ComboMappings[i].m_Input);
	}

	bank.AddCombo( "Debug Group",
		&m_DebugMapping,
		m_ComboMappings.size() + 1,
		mappingNames );

	bank.PopGroup();
}

void CMappingManager::ComboGroup::UpdateDebugInfo()
{
	if(m_DebugMapping == 0)
	{
		DebugDrawComboList();
	}
	// + 1 as 0 means all mappings, all mappings are offset by 1.
	else if(mapmgrVerifyf(m_DebugMapping > 0 && m_DebugMapping < (m_ComboMappings.size() + 1), "Invalid mapping index!"))
	{
		DebugDrawCombo(m_DebugMapping - 1);
	}
}

void CMappingManager::ComboGroup::DebugDrawCombo(u32 comboId) const
{
	if(m_ComboMappings.size() > comboId && m_ComboMappings[comboId].m_Tracks.size() > 0)
	{
		const ComboMapping& mapping = m_ComboMappings[comboId];

		const float paddingY   = 0.2f;
		const float paddingX   = 0.15f;
		const float fullLength = 1.0f - (2.0f * paddingX);
		const float dy         = Min((1.0f - (2.0f * paddingY)) / mapping.m_Tracks.size(), 0.2f);
		const float height     = dy / 2.0f;
		const float stepLength = fullLength / static_cast<float>(mapping.m_Tracks[0].m_Steps.size());
		const Vector2 vDirection(1.0f,0.0f);

		// Output the input that this combo is mapped to.
		grcDebugDraw::Text(Vector2(0, 0.0f), Color_DarkOrange, GetInputName(mapping.m_Input));

		// Loop through each track and display its status.
		for(u32 trackIndex = 0u; trackIndex < mapping.m_Tracks.size(); ++trackIndex)
		{
			const ComboTrack& track = mapping.m_Tracks[trackIndex];
			const float trackY      = paddingY + (trackIndex * dy);

			// Output the track button information (currently displayed as TODO until we can get a string representation).
			grcDebugDraw::Text(Vector2(0.0f, trackY - (dy / 2.0f)), Color_blue, "TODO: Button");

			// Display each step in the track as meter.
			for(u32 stepIndex = 0u; stepIndex < track.m_Steps.size(); ++stepIndex)
			{
				const Vector2 stepPos(paddingX + (stepIndex * stepLength), trackY);

				Color32 stepColor;
				// If the step has already been passed.
				if(mapping.m_CurrentStep > stepIndex)
				{
					stepColor = Color_green;
				}
				// If this is the step currently in progress.
				else if(mapping.m_CurrentStep == stepIndex)
				{
					stepColor = ComboTrack::GetTestStateColor(track.m_StepState);

					if(mapping.m_RunningState == ComboMapping::RUNNING)
					{

						float progress = static_cast<float>(fwTimer::GetSystemPrevElapsedTimeInMilliseconds() - mapping.m_StepStartTime) /
							static_cast<float>(mapping.m_TimeDelta);

						// Show a value (in this case a time bar) of how far through the step we are.
						grcDebugDraw::MeterValue(stepPos, vDirection, stepLength, progress, height, Color_red, true);
					}
				}
				// The step has not been started yet.
				else
				{
					stepColor = Color_DarkOliveGreen4;
				}

				grcDebugDraw::Meter( stepPos,
					vDirection,
					stepLength,
					height,
					stepColor,
					CMappingManager::GetTriggerName(track.m_Steps[stepIndex]) );
			}

			// Add summary information to the end of the track.
			char stateText[60];
			formatf( stateText,
				"%s: %s",
				ComboTrack::GetTestStateStr(track.m_StepState),
				CMappingManager::GetTriggerName(track.m_Steps[mapping.m_CurrentStep]) );

			grcDebugDraw::Text( Vector2(1.0f - paddingX, trackY - (dy / 2.0f)),
				ComboTrack::GetTestStateColor(track.m_StepState),
				stateText );
		}

	}
}

void CMappingManager::ComboGroup::DebugDrawComboList() const
{
	// Output the group id being displayed (the group id is the file path).
	grcDebugDraw::Text(Vector2(0, 0.0f), Color_DarkOrange, m_Id.TryGetCStr());

	// Although each mapping has a different number of steps, we use the largest to keep all steps the same size making
	// the debug display easier to understand.
	u32 maxSteps = 0u;
	for(u32 mappingIndex = 0; mappingIndex < m_ComboMappings.size(); ++mappingIndex)
	{
		if(m_ComboMappings[mappingIndex].m_Tracks.size() > 0)
		{
			maxSteps = Max(maxSteps, (u32)m_ComboMappings[mappingIndex].m_Tracks[0].m_Steps.size());
		}
	}

	const float paddingY   = 0.2f;
	const float paddingX   = 0.15f;
	const float fullLength = 1.0f - (2.0f * paddingX);
	const float dy         = Min((1.0f - (2.0f * paddingY)) / m_ComboMappings.size(), 0.2f);
	const float height     = dy / 2.0f;
	const float stepLength = fullLength / static_cast<float>(maxSteps);
	const Vector2 vDirection(1.0f,0.0f);

	// Display all mappings in this group.
	for(u32 mappingIndex = 0; mappingIndex < m_ComboMappings.size(); ++mappingIndex)
	{
		const ComboMapping& mapping = m_ComboMappings[mappingIndex];
		const float comboY          = paddingY + (mappingIndex * dy);
		const Color32 mappingColor  = ComboMapping::GetRunningStateColor(mapping.m_RunningState);

		// Display the input name for this mapping next to its track.
		grcDebugDraw::Text(Vector2(0.0f, comboY - (dy / 2.0f)), mappingColor, GetInputName(mapping.m_Input));

		if(mapping.m_Tracks.size() > 0)
		{
			// Display each step separately for a mapping but without individual track states. This can help track down
			// mapping hierarchy issues when a mapping is waiting on a higher mapping that has a lot of steps.
			for(u32 stepIndex = 0u; stepIndex < mapping.m_Tracks[0].m_Steps.size(); ++stepIndex)
			{
				const Vector2 stepPos(paddingX + (stepIndex * stepLength), comboY);

				Color32 stepColor;
				const char* stepStr = NULL;
				
				// If this step has already been passed.
				if(mapping.m_CurrentStep > stepIndex)
				{
					stepStr   = "Complete";
					stepColor = Color_green;
				}
				// If this is the current step.
				else if(mapping.m_CurrentStep == stepIndex)
				{
					stepStr   = ComboMapping::GetRunningStateStr(mapping.m_RunningState);
					stepColor = ComboMapping::GetRunningStateColor(mapping.m_RunningState);

					if(mapping.m_RunningState == ComboMapping::RUNNING)
					{
						float progress = static_cast<float>(fwTimer::GetSystemPrevElapsedTimeInMilliseconds() - mapping.m_StepStartTime) /
							static_cast<float>(mapping.m_TimeDelta);

						grcDebugDraw::MeterValue(stepPos, vDirection, stepLength, progress, height, Color_red, true);
					}
				}
				// This step has not started yet.
				else
				{
					stepStr   = "Waiting";
					stepColor = Color_DarkOliveGreen4;
				}

				grcDebugDraw::Meter( stepPos,
					vDirection,
					stepLength,
					height,
					stepColor,
					stepStr );
			}
		}

		grcDebugDraw::Text( Vector2(1.0f - paddingX, comboY - (dy / 2.0f)),
			mappingColor,
			ComboMapping::GetRunningStateStr(mapping.m_RunningState) );
	}
}

#endif // RSG_BANK
}

#endif // ENABLE_INPUT_COMBOS
