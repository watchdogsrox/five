<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema"
							generate="class">

  <enumdef type="CEventResponseDecisionFlags::Flags" generate="bitset">
    <enumval name="ValidOnFoot"/>
    <enumval name="ValidInCar"/>
    <enumval name="ValidInHeli"/>
    <enumval name="ValidOnBicycle"/>
    <enumval name="NotValidInComplexScenario"/>
    <enumval name="ValidOnlyInComplexScenario"/>
    <enumval name="InvalidInCrosswalk" />
    <enumval name="ValidOnlyInCrosswalk" />
    <enumval name="InvalidWhenAfraid"/>
    <enumval name="ValidOnlyForDrivers"/>
    <enumval name="ValidOnlyForPassengersWithDriver"/>
    <enumval name="ValidOnlyForPassengersWithNoDriver"/>
    <enumval name="ValidForTargetInsideVehicle"/>
    <enumval name="ValidForTargetOutsideVehicle"/>
    <enumval name="ValidOnlyIfSourceHasGun"/>
    <enumval name="ValidOnlyIfSourceDoesNotHaveGun"/>
    <enumval name="ValidOnlyIfInsideScenarioRadius"/>
    <enumval name="ValidOnlyIfOutsideScenarioRadius"/>
    <enumval name="ValidOnlyIfNoScenarioRadius"/>
    <enumval name="InvalidIfHasARider"/>
    <enumval name="ValidOnlyIfFriendlyWithTarget"/>
    <enumval name="InvalidIfFriendlyWithTarget"/>
    <enumval name="InvalidInAnInterior"/>
    <enumval name="ValidOnlyInAnInterior"/>
    <enumval name="ValidOnlyIfSourceIsPlayer"/>
    <enumval name="ValidOnlyIfRandom"/>
    <enumval name="ValidOnlyIfTarget"/>
    <enumval name="ValidOnlyIfTargetIsInvalid"/>
    <enumval name="ValidOnlyIfTargetIsInvalidOrFriendly"/>
    <enumval name="ValidOnlyIfTargetIsValidAndNotFriendly"/>
    <enumval name="ValidOnlyIfSource"/>
    <enumval name="ValidOnlyIfSourceIsInvalid"/>
    <enumval name="ValidOnlyIfSourceIsAnAnimal"/>
    <enumval name="ValidOnlyForStationaryPeds"/>
    <enumval name="InvalidForStationaryPeds"/>
    <enumval name="InvalidIfSourceEntityIsOtherEntity"/>
    <enumval name="ValidOnlyIfSourceEntityIsOtherEntity"/>
    <enumval name="ValidOnlyIfOtherVehicleIsYourVehicle"/>
    <enumval name="InvalidIfOtherVehicleIsYourVehicle"/>
    <enumval name="InvalidIfTargetDoesNotInfluenceWanted"/>
    <enumval name="ValidOnlyIfSourcePed"/>
    <enumval name="InvalidIfSourcePed"/>
    <enumval name="InvalidIfSourceIsAnAnimal"/>
    <enumval name="InvalidIfThereAreAttackingSharks"/>
    <enumval name="InvalidIfMissionPedInMP"/>
	  <enumval name="ValidOnlyIfMissionPedInMP"/>
    <enumval name="ValidOnlyIfSourceIsThreatening" />
    <enumval name="ValidOnlyIfSourceIsNotThreatening" />
  </enumdef>

</ParserSchema>
