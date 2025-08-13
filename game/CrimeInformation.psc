<?xml version="1.0"?> 
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<enumdef type="CWitnessInformation::Flags">
		<enumval name="MustBeWitnessed"/>
		<enumval name="MustNotifyLawEnforcement"/>
		<enumval name="OnlyVictimCanNotifyLawEnforcement"/>
	</enumdef>

	<structdef type="CWitnessInformation">
		<float name="m_MaxDistanceToHear" init="0.0f"/>
		<bitset name="m_Flags" type="fixed8" values="CWitnessInformation::Flags"/>
		<bitset name="m_AdditionalFlagsForSP" type="fixed8" values="CWitnessInformation::Flags"/>
		<bitset name="m_AdditionalFlagsForMP" type="fixed8" values="CWitnessInformation::Flags"/>
	</structdef>

	<structdef type="CCrimeInformation">
		<struct name="m_WitnessInformation" type="CWitnessInformation"/>
		<float name="m_ImmediateDetectionRange" init="25.0f"/>
	</structdef>

	<structdef type="CCrimeInformations">
		<map name="m_CrimeInformations" type="atBinaryMap" key="atHashString">
			<struct type="CCrimeInformation" />
		</map>
	</structdef>
	
</ParserSchema>
