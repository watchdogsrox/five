<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema"
              generate="class">

    <enumdef type="CVehicleModelInfoFlags::Flags" generate="bitset">
        <enumval name="FLAG_SMALL_WORKER" description="This vehicle is a small worker (forklift, luggage handler) and should only be placed on specific paths."/>
        <enumval name="FLAG_BIG" description="This vehicle is very big and should avoid turning"/>
        <enumval name="FLAG_NO_BOOT" description="This vehicle should not be considered for boot inventory"/>
        <enumval name="FLAG_ONLY_DURING_OFFICE_HOURS" description="This vehicle should only be streamed in during office hours"/>
        <enumval name="FLAG_BOOT_IN_FRONT" description="This car has the boot in front so the boot searches should be from front"/>
        <enumval name="FLAG_IS_VAN" description="This vehicle is a van, the rear doors open sideways"/>
        <enumval name="FLAG_AVOID_TURNS" description="This vehicle is trying to avoid turning. Go straight where possible."/>
        <enumval name="FLAG_HAS_LIVERY" description="This vehicle has liveries, some texture will be swapped upon instancing."/>
        <enumval name="FLAG_LIVERY_MATCH_EXTRA" description="This vehicle texture swap will match the part swap."/>
        <enumval name="FLAG_SPORTS" description="Sports car."/>
        <enumval name="FLAG_DELIVERY" description="Can be used for deliverys (vans and small trucks)"/>
        <enumval name="FLAG_NOAMBIENTOCCLUSION" description="Vehicle doesn't have ambient occlusion rendered."/>
        <enumval name="FLAG_ONLY_ON_HIGHWAYS" description="This vehicle can only be created on highways (like a tourbus)"/>
        <enumval name="FLAG_TALL_SHIP" description="This is a tall ship and should not be placed on nodes with LowBridge ticked."/>
        <enumval name="FLAG_SPAWN_ON_TRAILER" description="If set for a trailer it enables other cars to spawn on it. If set for normal vehicle it enabled it to spawn on a trailer"/>
        <enumval name="FLAG_SPAWN_BOAT_ON_TRAILER" description="If set for a trailer it enables a boat to spawn on it. If set for boats it enables them to spawn on a trailer"/>
        <enumval name="FLAG_EXTRAS_GANG" description="if gang use all of 5,6,7,8  if not use random of others"/>
        <enumval name="FLAG_EXTRAS_CONVERTIBLE" description="folded down roof = 1, roofs = 2,3,4"/>
        <enumval name="FLAG_EXTRAS_TAXI" description="must have a taxi light, one of 5,6,7,8"/>
        <enumval name="FLAG_EXTRAS_RARE" description="lower chance of picking any extra"/>
        <enumval name="FLAG_EXTRAS_REQUIRE" description="must pick one of available extras"/>
        <enumval name="FLAG_EXTRAS_STRONG" description="extras won't break off"/>
        <enumval name="FLAG_EXTRAS_ONLY_BREAK_WHEN_DESTROYED" description="extras won't break off until the vehicle is blown up"/>
        <enumval name="FLAG_EXTRAS_SCRIPT" description="script only extras 5,6,7,8"/>
        <enumval name="FLAG_EXTRAS_ALL" description="all suitable extras should be turned on (eg all non-gang extras if non gang car)"/>
        <enumval name="FLAG_EXTRAS_MATCH_LIVERY" description="uses the livery with same index as the extra (e.g. extra_1 forces livery 1)"/>
        <enumval name="FLAG_DONT_ROTATE_TAIL_ROTOR" description="This vehicle should not have a rotated tail rotor"/>
        <enumval name="FLAG_PARKING_SENSORS" description="This vehicle has parking sensors"/>
        <enumval name="FLAG_PEDS_CAN_STAND_ON_TOP" description="Peds can stand on top of this vehicle"/>
        <enumval name="FLAG_TAILGATE_TYPE_BOOT" description="this vehicle has a tailgate type boot (it opens down and back)"/>
        <enumval name="FLAG_GEN_NAVMESH" description="this vehicle should generate a nav mesh"/>
        <enumval name="FLAG_LAW_ENFORCEMENT" description="Is the vehicle a law enforcement vehicle"/>
        <enumval name="FLAG_EMERGENCY_SERVICE" description="Is a emergency service vehicle"/>
        <enumval name="FLAG_DRIVER_NO_DRIVE_BY" description="Driver of this vehicle cant do drive-bys"/>
        <enumval name="FLAG_NO_RESPRAY" description="Vehicle is not allowed to be resprayed at garages"/>
        <enumval name="FLAG_IGNORE_ON_SIDE_CHECK" description="Vehicle can be entered if its on its side"/>
        <enumval name="FLAG_RICH_CAR" description="Rich car, indicates what peds can be used as drivers"/>
        <enumval name="FLAG_AVERAGE_CAR" description="Average car, indicates what peds can be used as drivers"/>
        <enumval name="FLAG_POOR_CAR" description="Poor car, indicates what peds can be used as drivers"/>
        <enumval name="FLAG_ALLOWS_RAPPEL" description="This vehicle allows people to rappel out of it"/>
        <enumval name="FLAG_DONT_CLOSE_DOOR_UPON_EXIT" description="Vehicle prevents peds from closing door when exiting"/>
        <enumval name="FLAG_USE_HIGHER_DOOR_TORQUE" description="Make the doors open/close faster"/>
        <enumval name="FLAG_DISABLE_THROUGH_WINDSCREEN" description="Prevent peds in this vehicle from being thrown from windscreen"/>
        <enumval name="FLAG_IS_ELECTRIC" description="Vehicle doesn't have a petrol tank"/>
        <enumval name="FLAG_NO_BROKEN_DOWN_SCENARIO" description="Explicitly disable broken down car scenarios on this vehicle"/>
        <enumval name="FLAG_IS_JETSKI" description="This vehicle is a jetski"/>
        <enumval name="FLAG_DAMPEN_STICKBOMB_DAMAGE" description="Whether or not we should dampen the total damage of attached stickybombs"/>
        <enumval name="FLAG_DONT_SPAWN_IN_CARGEN" description="Stops the vehicle from being spawned by cargens (i.e. parked)"/>
        <enumval name="FLAG_IS_OFFROAD_VEHICLE" description="Flags the vehicles as an offroad only vehicle"/>
        <enumval name="FLAG_INCREASE_PED_COMMENTS" description="Peds will have a bigger probability of commenting on this vehicle"/>
        <enumval name="FLAG_EXPLODE_ON_CONTACT" description="Whether or not a bullet/projectile will force the vehicle to explode."/>
        <enumval name="FLAG_USE_FAT_INTERIOR_LIGHT" description="Vehicle needs a bigger interior light"/>
        <enumval name="FLAG_HEADLIGHTS_USE_ACTUAL_BONE_POS" description="Vehicle uses actual bone position for headlight positioning (warning: Expensive)"/>
        <enumval name="FLAG_FAKE_EXTRALIGHTS" description="Vehicle's extra light don't cast lights but are turned on and off when headlights are"/>
        <enumval name="FLAG_CANNOT_BE_MODDED" description="Stops the vehicle from being modded"/>
        <enumval name="FLAG_DONT_SPAWN_AS_AMBIENT" description="Stops the vehicle from spawning in the ambient population"/>
        <enumval name="FLAG_IS_BULKY" description="Vehicle is bulky (vans, suvs, etc.) and will be prefered for cargen spawns"/>
        <enumval name="FLAG_BLOCK_FROM_ATTRACTOR_SCENARIO" description="Stops the vehicle from being attracted by vehicle scenarios."/>
        <enumval name="FLAG_IS_BUS" description="This vehicle is a bus."/>
        <enumval name="FLAG_USE_STEERING_PARAM_FOR_LEAN" description="Pass the steering move param rather then the lean."/>
        <enumval name="FLAG_CANNOT_BE_DRIVEN_BY_PLAYER" description="Player can't drive this vehicle."/>
        <enumval name="FLAG_SPRAY_PETROL_BEFORE_EXPLOSION" description="Whether or not to play a spraying petrol vfx before explosion"/>
        <enumval name="FLAG_ATTACH_TRAILER_ON_HIGHWAY" description="Allows attachment of trailers on highway"/>
        <enumval name="FLAG_ATTACH_TRAILER_IN_CITY" description="Allows attachment of trailers in the city (i.e. non highway roads but still no narrow ones)"/>
        <enumval name="FLAG_HAS_NO_ROOF" description="Vehicle doesn't have a roof, at all"/>
        <enumval name="FLAG_ALLOW_TARGETING_OF_OCCUPANTS" description="Allow locking on to vehicle occupants"/>
        <enumval name="FLAG_RECESSED_HEADLIGHT_CORONAS" description="Head light coronas are recessed inside vehicle body (corona visibility is restricted)"/>
        <enumval name="FLAG_RECESSED_TAILLIGHT_CORONAS" description="Tail light coronas are recessed inside vehicle body (corona visibility is restricted)"/>
        <enumval name="FLAG_IS_TRACKED_FOR_TRAILS" description="This vehicle is tracked by the EntityTrackerManager for perposes of making trails in the world (ie grass)"/>
        <enumval name="FLAG_HEADLIGHTS_ON_LANDINGGEAR" description="The headlights are/is attached to the landing gears"/>
        <enumval name="FLAG_CONSIDERED_FOR_VEHICLE_ENTRY_WHEN_STOOD_ON" description="If stood on this vehicle, we'll try to enter it"/>
        <enumval name="FLAG_GIVE_SCUBA_GEAR_ON_EXIT" description=""/>
        <enumval name="FLAG_IS_DIGGER" description=""/>
        <enumval name="FLAG_IS_TANK" description=""/>
        <enumval name="FLAG_USE_COVERBOUND_INFO_FOR_COVERGEN" description=""/>
        <enumval name="FLAG_CAN_BE_DRIVEN_ON" description="This allows other vehicles to drive on this without the wheels ignoring impacts due to the chassis hitting the vehicle first."/>
        <enumval name="FLAG_HAS_BULLETPROOF_GLASS" description="Can't smash this"/>
        <enumval name="FLAG_CANNOT_TAKE_COVER_WHEN_STOOD_ON" description=""/>
        <enumval name="FLAG_INTERIOR_BLOCKED_BY_BOOT" description="Has a hollow interior enterable by a boot door mechanism"/>
        <enumval name="FLAG_DONT_TIMESLICE_WHEELS" description="Turn off wheel timeslicing optimisation for this vehicle"/>
        <enumval name="FLAG_FLEE_FROM_COMBAT" description="If in this vehicle when entering combat peds will flee instead of fight"/>
        <enumval name="FLAG_DRIVER_SHOULD_BE_FEMALE" description="This vehicle should use only female drivers (only support for scenarios, initially)"/>
        <enumval name="FLAG_DRIVER_SHOULD_BE_MALE" description="This vehicle should use only male drivers (only support for scenarios, initially)"/>
        <enumval name="FLAG_COUNT_AS_FACEBOOK_DRIVEN" description="This vehicle should will count towards all vehicle driven post to facebook"/>
        <enumval name="FLAG_BIKE_CLAMP_PICKUP_LEAN_RATE" description="This smooths out the pickup of bikes with luggage outside the rear wheel."/>
        <enumval name="FLAG_PLANE_WEAR_ALTERNATIVE_HELMET" description="If this is set then we use an alternative helmet for these vehicles"/>
        <enumval name="FLAG_USE_STRICTER_EXIT_COLLISION_TESTS" description="If vehicle is parked sideways on a slope we check against the vehicle to prevent getting stuck or walkking through it"/>
        <enumval name="FLAG_TWO_DOORS_ONE_SEAT" description=""/>
        <enumval name="FLAG_USE_LIGHTING_INTERIOR_OVERRIDE" description="If vehicle can have passengers inside of it in MP override the lighting when there is no view to the outside"/>
        <enumval name="FLAG_USE_RESTRICTED_DRIVEBY_HEIGHT" description="Don't blend in the high sweep during drivebys"/>
        <enumval name="FLAG_CAN_HONK_WHEN_FLEEING" description="This vehicle can, under certain conditions, honk its horn while fleeing."/>
        <enumval name="FLAG_PEDS_INSIDE_CAN_BE_SET_ON_FIRE_MP" description="Allows peds inside the vehicle to be set on fire in mp (take damage)"/>
        <enumval name="FLAG_REPORT_CRIME_IF_STANDING_ON" description="Standing on this vehicle as a player will get a crime reported"/>
        <enumval name="FLAG_HELI_USES_FIXUPS_ON_OPEN_DOOR" description="Perform fixups on AlignToEnterSeat and OpenDoor task states (because the anims don't use climb_up)"/>
        <enumval name="FLAG_FORCE_ENABLE_CHASSIS_COLLISION" description=" B*1806240: Prevents InitCompositeBounds in CVehicle from removing most collision include flags for non-BVH chassis bounds."/>
        <enumval name="FLAG_CANNOT_BE_PICKUP_BY_CARGOBOB" description=" If set, it can not be picked up by the cargobob's grappling hook gadgets."/>
        <enumval name="FLAG_CAN_HAVE_NEONS" description="If set, this car can spawn in the ambient population with neons on."/>
        <enumval name="FLAG_HAS_INTERIOR_EXTRAS" description="If set, this vehicle will treat extras 10, 11, and 12 as special interior extras which always have a chance of spawning."/>
		    <enumval name="FLAG_HAS_TURRET_SEAT_ON_VEHICLE" description=""/>
        <enumval name="FLAG_ALLOW_OBJECT_LOW_LOD_COLLISION" description="This vehicle allows certain objects to collide with the low-LOD chassis rather than the regular chassis."/>
        <enumval name="FLAG_DISABLE_AUTO_VAULT_ON_VEHICLE" description=""/>
        <enumval name="FLAG_USE_TURRET_RELATIVE_AIM_CALCULATION" description=""/>
        <enumval name="FLAG_USE_FULL_ANIMS_FOR_MP_WARP_ENTRY_POINTS" description="Force full animations to play on vehicle entry points marked as MPWarpInOut (if available)"/>
        <enumval name="FLAG_HAS_DIRECTIONAL_SHUFFLES" description=""/>
        <enumval name="FLAG_DISABLE_WEAPON_WHEEL_IN_FIRST_PERSON" description="This vehicle prevents the weapon wheel from displaying if in first person"/>
        <enumval name="FLAG_USE_PILOT_HELMET" description="This vehicle uses PV_FLAG_PILOT_HELMET instead of the standard flight helmet, even if the vehicle seat has no weapons."/>
		<enumval name="FLAG_USE_WEAPON_WHEEL_WITHOUT_HELMET" description=""/>
        <enumval name="FLAG_PREFER_ENTER_TURRET_AFTER_DRIVER" description="If this vehicle already has a driver, prefer entering the turret seats"/>
        <enumval name="FLAG_USE_SMALLER_OPEN_DOOR_RATIO_TOLERANCE" description="Consider doors of this vehicle to be open with a smaller open door ratio"/>
        <enumval name="FLAG_USE_HEADING_ONLY_IN_TURRET_MATRIX" description="Returns the turret matrix computed from the aim heading only"/>
        <enumval name="FLAG_DONT_STOP_WHEN_GOING_TO_CLIMB_UP_POINT" description=""/>
        <enumval name="FLAG_HAS_REAR_MOUNTED_TURRET" description=""/>
        <enumval name="FLAG_DISABLE_BUSTING" description="Dont allow cops to bust a player in this vehicle"/>
        <enumval name="FLAG_IGNORE_RWINDOW_COLLISION" description="Ignore bullet collision with the vehicle if shooting through rear windscreen"/>
		<enumval name="FLAG_HAS_GULL_WING_DOORS" description="The doors of this vehicle open upwards instead of outwards."/>
		<enumval name="FLAG_CARGOBOB_HOOK_UP_CHASSIS" description="Uses chassis bounding box to compute the offset for Cargobob's grappling hook attachment"/>
        <enumval name="FLAG_USE_FIVE_ANIM_THROW_FP" description="Allow vehicle to use wider throw animations, but only in first person"/>
        <enumval name="FLAG_ALLOW_HATS_NO_ROOF" description="Ensures hats don't get hidden in CPed::SetPedInVehicle if the vehicle has FLAG_NO_ROOF flag set."/>
        <enumval name="FLAG_HAS_REAR_SEAT_ACTIVITIES" description="Rear seat has activities. Used for allowing rear seat entry (currently only on Luxor2 - see url:bugstar:2282719)."/>
        <enumval name="FLAG_HAS_LOWRIDER_HYDRAULICS" description="Vehicle has lowrider hydraulics. Sets up hydraulic mod on vehicle in CVehicle::SetModelId."/>
        <enumval name="FLAG_HAS_BULLET_RESISTANT_GLASS" description="Glass on this vehicle has a health bar; won't smash until health gets below 0. Damage applied depends on weapon group/weapon (see CWeaponInfoManager::sWeaponGroupArmouredGlassDamage). See url:bugstar:2596486."/>
      <enumval name="FLAG_HAS_INCREASED_RAMMING_FORCE" description="Vehicle is armoured and can ram roadblocks / other cars much easier"/>
        <enumval name="FLAG_HAS_CAPPED_EXPLOSION_DAMAGE" description="Damage from an explosion is capped, allowing the vehicle to take one hit from an explosion without instantly exploding. Search for EXPLOSIVE_DAMAGE_CAP_FOR_FLAGGED_VEHICLES to find capped value."/>
      <enumval name="FLAG_HAS_LOWRIDER_DONK_HYDRAULICS" description="Vehicle has lowrider donk hydraulics. Sets up hydraulic mod on vehicle in CVehicle::SetModelId."/>
        <enumval name="FLAG_HELICOPTER_WITH_LANDING_GEAR" description="Vehicle is a helicopter that has landing gears like planes (that can be lowered or raised)."/>
      <enumval name="FLAG_JUMPING_CAR" description="Vehicle can perform a jump."/>
      <enumval name="FLAG_HAS_ROCKET_BOOST" description="Vehicle has a rechargable rocket boost."/>
      <enumval name="FLAG_RAMMING_SCOOP" description="Vehicle has a scoop in front that can ram other cars to the side."/>
      <enumval name="FLAG_HAS_PARACHUTE" description="Vehicle has a parachute."/>
      <enumval name="FLAG_RAMP" description="Vehicle can be used as a ramp."/>
			<enumval name="FLAG_HAS_EXTRA_SHUFFLE_SEAT_ON_VEHICLE" description="Vehicle has extra shuffle seats that can be manually moved between (similar to some functionality of the turret seats)."/>
      <enumval name="FLAG_FRONT_BOOT" description="Vehicle has its boot at the front."/>
      <enumval name="FLAG_HALF_TRACK" description="Vehicle has normal wheels at the front but tracks at the rear."/>
      <enumval name="FLAG_RESET_TURRET_SEAT_HEADING" description="The turret heading should be reset when there is no one using it"/>
      <enumval name="FLAG_TURRET_MODS_ON_ROOF" description="The roof mods are used to store the turret mod"/>
      <enumval name="FLAG_UPDATE_WEAPON_BATTERY_BONES" description="The weapon battery bones need to be updated"/>
      <enumval name="FLAG_DONT_HOLD_LOW_GEARS_WHEN_ENGINE_UNDER_LOAD" description="The gearbox should change up as soon as it reaches the change up point even when the engine is under load"/>
      <enumval name="FLAG_HAS_GLIDER" description="The vehicle is equiped with a glider"/>
      <enumval name="FLAG_INCREASE_LOW_SPEED_TORQUE" description="Torque is increased at low speed to aid getting up hills"/>
			<enumval name="FLAG_USE_AIRCRAFT_STYLE_WEAPON_TARGETING" description="Does the same behaviour has aircraft for targeting (only locks onto players and hostile targets) and reticules (renders along weapon probe)"/>
      <enumval name="FLAG_KEEP_ALL_TURRETS_SYNCHRONISED" description="Makes sure all turret base and barrel bones have the same rotation"/>
      <enumval name="FLAG_SET_WANTED_FOR_ATTACHED_VEH" description="If player inside this vehicle gets wanted level, this flag will also give the same wanted level to any peds inside attached vehicles (like trailer) and vice versa"/>
			<enumval name="FLAG_TURRET_ENTRY_ATTACH_TO_DRIVER_SEAT" description="Force attach ped to the driver seat when entering a turret seat that can rotate, in order to correctly calculate align positions"/>
      <enumval name="FLAG_USE_STANDARD_FLIGHT_HELMET" description="Force the standard flight helment to be used instead of the PV_FLAG_PILOT_HELMET, even if the vehicle seat has weapons."/>
      <enumval name="FLAG_SECOND_TURRET_MOD" description="Has a secondary turret mod, need to decide which mod this maps too"/>
      <enumval name="FLAG_THIRD_TURRET_MOD" description="Has a third turret mod, need to decide which mod this maps too"/>
      <enumval name="FLAG_HAS_EJECTOR_SEATS" description="Hey, what does this button do?"/>
      <enumval name="FLAG_TURRET_MODS_ON_CHASSIS" description="Has turret mods in the chassis mod slot"/>
      <enumval name="FLAG_HAS_JATO_BOOST_MOD" description="Has a jato rocket boost to assist takeoff, uses the exhaust mod slot"/>
      <enumval name="FLAG_IGNORE_TRAPPED_HULL_CHECK" description="Will not do shapetests to see if we're trapped in convex hull. This will make ped always go to climb up point directly"/>
      <enumval name="FLAG_HOLD_TO_SHUFFLE" description="Will make shuffling between seats be on hold rather than tap"/>
			<enumval name="FLAG_TURRET_MOD_WITH_NO_STOCK_TURRET" description=" The vehicle does not have a turret as stock, but ones should be activated with an appropriate mod. This applies -1 to the mod index in CVehicle::SetupWeaponModCollision."/>
			<enumval name="FLAG_EQUIP_UNARMED_ON_ENTER" description="Forces unarmed weapon to be equipped when entering the vehicle rather than calling EquipBestWeapon."/>
      <enumval name="FLAG_DISABLE_CAMERA_PUSH_BEYOND" description="Disables camera behaviour that pushes the camera beyond this entity during spawning, useful for very large vehicles"/>
      <enumval name="FLAG_HAS_VERTICAL_FLIGHT_MODE" description="has a vertical flight mode"/>
      <enumval name="FLAG_HAS_OUTRIGGER_LEGS" description="has outrigger legs that need to be deployed"/>
      <enumval name="FLAG_CAN_NAVIGATE_TO_ON_VEHICLE_ENTRY" description="Flags standard vehicles as having on-vehicle entry points to navigate towards when standing on the vehicle. Aquatic vehicles and planes use this behaviour by default."/>
      <enumval name="FLAG_DROP_SUSPENSION_WHEN_STOPPED" description="Suspension lowers when the vehicle is stopped."/>
      <enumval name="FLAG_DONT_CRASH_ABANDONED_NEAR_GROUND" description="Dont crash teh vehicle near ground if everyone exited it"/>
      <enumval name="FLAG_USE_INTERIOR_RED_LIGHT" description="Vehicle needs a special red interior light"/>
      <enumval name="FLAG_HAS_HELI_STRAFE_MODE" description="It is a helicopter with a special strafe handling mode"/>
      <enumval name="FLAG_HAS_VERTICAL_ROCKET_BOOST" description="Has a rocket boost, or rocket boost mod, that works vertically"/>
			<enumval name="FLAG_CREATE_WEAPON_MANAGER_ON_SPAWN" description="Creates vehicle weapon manager on spawn instead of entering a seat with a vehicle weapon. This is already done for tanks and some vehicles with turret seats automatically."/>
      <enumval name="FLAG_USE_ROOT_AS_BASE_LOCKON_POS" description="Makes lockon position to be root of the vehicle instead of center position of the bounding box"/>
      <enumval name="FLAG_HEADLIGHTS_ON_TAP_ONLY" description="Makes headlights only toggle on tap of a button, not a hold"/>
      <enumval name="FLAG_CHECK_WARP_TASK_FLAG_DURING_ENTER" description="Makes headlights only toggle on tap of a button, not a hold"/>
			<enumval name="FLAG_USE_RESTRICTED_DRIVEBY_HEIGHT_HIGH" description="Don't blend in the low sweep during drivebys"/>
      <enumval name="FLAG_INCREASE_CAMBER_WITH_SUSPENSION_MOD" description="Increases the front wheel camber when a suspension mod is applied"/>
      <enumval name="FLAG_NO_HEAVY_BRAKE_ANIMATION" description="Prevents heavy brake animation from being applied (due to low roof, etc.)"/>
      <enumval name="FLAG_HAS_TWO_BONNET_BONES" description="Vehicle has two linked bonnet bones"/>
      <enumval name="FLAG_DONT_LINK_BOOT2" description="Vehicle the special linked door bone shouldn't be linked"/>
      <enumval name="FLAG_HAS_INCREASED_RAMMING_FORCE_WITH_CHASSIS_MOD" description="If the vehicle has the best chassis mod then it will have increased ramming force"/>
      <enumval name="FLAG_HAS_INCREASED_RAMMING_FORCE_VS_ALL_VEHICLES" description="The increased ramming force is applied against all vehicles not just police ones"/>
      <enumval name="FLAG_HAS_EXTENDED_COLLISION_MODS" description="Any of the mod bones can be used to enable or disable collision"/>
      <enumval name="FLAG_HAS_NITROUS_MOD" description="The vehicle has mods that can enable nitrous"/>
      <enumval name="FLAG_HAS_JUMP_MOD" description="The vehicle has mods that can enable nitrous"/>
      <enumval name="FLAG_HAS_RAMMING_SCOOP_MOD" description="The vehicle has mods that enable the ramming scoop"/>
      <enumval name="FLAG_HAS_SUPER_BRAKES_MOD" description="The vehicle has mods that enable the to stop very quickly"/>
      <enumval name="FLAG_CRUSHES_OTHER_VEHICLES" description="The vehicle crushes other vehicles when it drives over them"/>
      <enumval name="FLAG_HAS_WEAPON_BLADE_MODS" description="Has weapon blade mods"/>
      <enumval name="FLAG_HAS_WEAPON_SPIKE_MODS" description="Has weapon spike mods"/>
      <enumval name="FLAG_FORCE_BONNET_CAMERA_INSTEAD_OF_POV" description="Will always use bonnet camera instead of POV camera, ignoring pause menu option"/>
      <enumval name="FLAG_RAMP_MOD" description="Vehicle can be used as a ramp if the mod is enabled."/>
      <enumval name="FLAG_HAS_TOMBSTONE" description="Has a breakable off bumper mod"/>
      <enumval name="FLAG_HAS_SIDE_SHUNT" description="Has a side shunt mod - uses mod slot VMT_ENGINEBAY2"/>
      <enumval name="FLAG_HAS_FRONT_SPIKE_MOD" description="Has front spike mods"/>
      <enumval name="FLAG_HAS_RAMMING_BAR_MOD" description="Has front ramming bar mods"/>
      <enumval name="FLAG_TURRET_MODS_ON_CHASSIS5" description="uses the same turret mod setup as the FLAG_TURRET_MODS_ON_ROOF but they are on CHASSIS5"/>
      <enumval name="FLAG_HAS_SUPERCHARGER" description="The vehicle model looks like it has a supercharger. At the moment this only affects the gauges "/>
      <enumval name="FLAG_IS_TANK_WITH_FLAME_DAMAGE" description="The vehicle is a tank that can take fire damage "/>
      <enumval name="FLAG_DISABLE_DEFORMATION" description="The vehicle doesn't deform"/>
      <enumval name="FLAG_ALLOW_RAPPEL_AI_ONLY" description="Allow AI peds to rappel from vehicles that dont support it"/>
      <enumval name="FLAG_USE_RESTRICTED_DRIVEBY_HEIGHT_MID_ONLY" description="Don't blend high or low sweeps during drivebys"/>
      <enumval name="FLAG_FORCE_AUTO_VAULT_ON_VEHICLE_WHEN_STUCK" description="Will force auto vault scan when stuck on top of this vehicle"/>
      <enumval name="FLAG_SPOILER_MOD_DOESNT_INCREASE_GRIP" description="The mod that has been allocated the spoiler slot should not increase downforce"/>
      <enumval name="FLAG_NO_REVERSING_ANIMATION" description="Prevents reversing animation from being applied (due to low roof, etc.)"/>
      <enumval name="FLAG_IS_QUADBIKE_USING_BIKE_ANIMATIONS" description="Identifying trike vehicles of type VEHICLE_TYPE_QUADBIKE (created as CAutomobile) that use bike animations / layouts."/>
      <enumval name="FLAG_IS_FORMULA_VEHICLE" description="Marks vehicle as a Formula vehicle, currently used to warp out of the vehicle when on its side."/>
      <enumval name="FLAG_LATCH_ALL_JOINTS" description="Latches all the joints when the vehicle is created"/>
      <enumval name="FLAG_REJECT_ENTRY_TO_VEHICLE_WHEN_STOOD_ON" description="If stood on this vehicle, don't consider it for entry"/>
      <enumval name="FLAG_CHECK_IF_DRIVER_SEAT_IS_CLOSER_THAN_TURRETS_WITH_ON_BOARD_ENTER" description="Checks if the driver seat is closer than found turret seat when entering while stood on the vehicle"/>
	  <enumval name="FLAG_RENDER_WHEELS_WITH_ZERO_COMPRESSION" description="Even if a vehicle has suspension just render the wheels as if they have none."/>
      <enumval name="FLAG_USE_LENGTH_OF_VEHICLE_BOUNDS_FOR_PLAYER_LOCKON_POS" description="Dynamically adjusts player's lock on position using the vehicle bounds."/>
      <enumval name="FLAG_PREFER_FRONT_SEAT" description="Prefer the front seat rather than rear seats in this vehicle (Boss peds normally go for rear seats)"/>
    </enumdef>
</ParserSchema>
