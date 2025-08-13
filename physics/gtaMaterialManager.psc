<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<structdef type="phRumbleProfile">
    <string name="m_profileName" type="atHashString"/>

	<float name="m_triggerProbability" min="0.f" max="1.f" description="Probability of this profile triggering"/>
	<float name="m_minVelocity" min="0.f" max="500.f" description="Minimum velocity at which to start the rumble"/>
	<float name="m_maxVelocity" min="0.f" max="500.f" description="Maximum velocity at which rumble will be at max intensity"/>
	<u32 name="m_triggerInterval" min="0" max="10000" description="Interval in milliseconds for triggering (based on probability) rumble"/>

	<u8 name="m_minVelocityFrequency" min="0" max="255" description="When using velocity to interpolate the intensity, this value clamps the result at the lower end"/>
	<u8 name="m_frequencyMin_1" min="0" max="255" description="Min frequency for motor 1"/>
	<u8 name="m_frequencyMax_1" min="0" max="255" description="Max frequency for motor 1"/>

	<u8 name="m_frequencyMin_2" min="0" max="255" description="Min frequency for motor 2"/>
	<u8 name="m_frequencyMax_2" min="0" max="255" description="Max frequency for motor 2"/>

	<u16 name="m_durationMin" min="0" max="5000" description="Min duration for vibration"/>
	<u16 name="m_durationMax" min="0" max="5000" description="Max duration for vibration"/>
</structdef>

<structdef type="phMaterialMgrGta::RumbleProfileList">
	<array name="m_rumbleProfiles" type="atArray">
		<struct type="phRumbleProfile" />
	</array>
</structdef>

</ParserSchema>
