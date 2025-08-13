USING "commands_brains.sch"
USING "types.sch"

CONST_INT MAX_NUM_COORDS_FOR_WORLD_POINT	(5)

STRUCT coords_struct
	INT number_of_coords
	VECTOR vec_coord[MAX_NUM_COORDS_FOR_WORLD_POINT]
	FLOAT headings[MAX_NUM_COORDS_FOR_WORLD_POINT]
ENDSTRUCT

/*
STRUCT sched_struct
	PED_INDEX schPed 
	TRIGGER_METHOD schTrig	
ENDSTRUCT
*/

