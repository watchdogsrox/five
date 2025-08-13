//These are all commands defined by the scripting system itself (in script/thread.cpp)
//Please keep them in the order listed in scrThread::RegisterBuiltinCommands.

//PURPOSE: gets the current value for the timer A
//RETURNS: the integer value for timer A (in milliseconds)
NATIVE FUNC INT TIMERA () = "0x83666f9fb8febd4b"

//PURPOSE: gets the current value for the timer B
//RETURNS: the integer value for timer B (in milliseconds)
NATIVE FUNC INT TIMERB () = "0xc9d9444186b5a374"

//PURPOSE: sets the value for the timer A (in milliseconds)
NATIVE PROC SETTIMERA (INT I) = "0xc1b1e9a034a63a62"

//PURPOSE: sets the value for the timer B (in milliseconds)
NATIVE PROC SETTIMERB (INT I) = "0x5ae11bc36633de4e"

//PURPOSE: returns the current timestep (in seconds)
//This is actually built into the language itself because it is referenced
//implicitly by the +@ operator, so we cannot explicitly declare it here.
//NATIVE FUNC FLOAT TIMESTEP

//PURPOSE: stops script execution for I milliseconds
NATIVE PROC WAIT (INT I) = "0x4ede34fbadd967a6"

///////////////////
// TTY
//PURPOSE: procedure that prints a string to the console (NOT the game screen)
NATIVE DEBUGONLY PROC PRINTSTRING (STRING S) = "0x98119e8aebfadf2e"
//PURPOSE: procedure that prints a float to the console (NOT the game screen)
NATIVE DEBUGONLY PROC PRINTFLOAT (FLOAT F) = "0x8288714b173c1042"
//PURPOSE: procedure that prints a float to the console (NOT the game screen)
//Accepts field width and precision like C printf function
NATIVE DEBUGONLY PROC PRINTFLOAT2 (INT WIDTH,INT PRECISION,FLOAT F) = "0x0b38d631ec77e0db"
//PURPOSE: procedure that prints an integer to the console (NOT the game screen)
NATIVE DEBUGONLY PROC PRINTINT (INT I) = "0x9192b022dbbaf2b4"
//PURPOSE: procedure that prints an integer to the console (NOT the game screen)
//Accepts field width like C printf function
NATIVE DEBUGONLY PROC PRINTINT2 (INT WIDTH,INT I) = "0x5e6b1c5242268fe0"
//PURPOSE: procedure that prints a newline to the console (NOT the game screen)
NATIVE DEBUGONLY PROC PRINTNL () = "0x096bf1fe3e5d27d2"
//PURPOSE: procedure that prints a vector to the console (NOT the game screen)
NATIVE DEBUGONLY PROC PRINTVECTOR(VECTOR V) = "0x5f5f8ee44dd2ff4e"
//PURPOSE: displays a variable number of arguments
//			e.g. PRINTLN(myInt, " ", myString, " ", myTextLabel, " ", myFloat, " ", myVector)
NATIVE DEBUGONLY PROC PRINTLN(VARARGS) = "0x4be6713cecfcfff3"
//PURPOSE: displays a variable number of arguments, with SEVERITY_ASSERT (ie a popup)
NATIVE DEBUGONLY PROC ASSERTLN(VARARGS) = "0xcf8d79ecfcf47473"
//PURPOSE: should only be called when USE_FINAL_PRINTS is enabled
NATIVE PROC PRINTLN_FINAL(VARARGS) = "0x91a49dbad86281f8"

ENUM CHANNEL_SEVERITY
	CHANNEL_SEVERITY_FATAL,
	CHANNEL_SEVERITY_ASSERT,
	CHANNEL_SEVERITY_ERROR,
	CHANNEL_SEVERITY_WARNING,
	CHANNEL_SEVERITY_DISPLAY,
	CHANNEL_SEVERITY_DEBUG1,
	CHANNEL_SEVERITY_DEBUG2,
	CHANNEL_SEVERITY_DEBUG3
ENDENUM

ENUM CHANNEL_TYPE
	CHANNEL_FILELOG_LEVEL,
	CHANNEL_TTY_LEVEL,
	CHANNEL_POPUP_LEVEL
ENDENUM
CONST_INT MAX_CHANNEL_INDEX		191

FORWARD ENUM DEBUG_CHANNELS

//PURPOSE: Register a script TTY channel; CHANNEL must be between 0 and MAX_CHANNEL_INDEX
NATIVE DEBUGONLY PROC REGISTER_SCRIPT_CHANNEL(DEBUG_CHANNELS CHANNEL, STRING CHANNEL_NAME, INT PARENT_CHANNEL = -1) = "0x1bf3433758178133"

//PURPOSE: Returns CHANNEL_SEVERITY_... value associated with channel
NATIVE DEBUGONLY FUNC CHANNEL_SEVERITY GET_SCRIPT_CHANNEL_LEVEL(DEBUG_CHANNELS CHANNEL, CHANNEL_TYPE CHAN_TYPE) = "0xa54a5969c06e2354"

//PURPOSE: Sets severity associated with channel
NATIVE DEBUGONLY PROC SET_SCRIPT_CHANNEL_LEVEL(DEBUG_CHANNELS CHANNEL, CHANNEL_TYPE CHAN_TYPE, CHANNEL_SEVERITY CHAN_SEVERITY) = "0xc626c7753808559e"

//PURPOSE: Channeled print line function.
// First parameter must be a value passed to REGISTER_SCRIPT_TTY_CHANNEL
// Second parameter must be one of the CHANNEL_SEVERITY_... constants above
// Subsequent parameters are as per PRINTLN.
NATIVE DEBUGONLY PROC CLOGLN(VARARGS3) = "0xef256ae8a5a27966"
//PURPOSE: Channeled print line function with implicity severity of CHANNEL_SEVERITY_DISPLAY
// First parameter must be a value passed to REGISTER_SCRIPT_TTY_CHANNEL
// Subsequent parameters are as per PRINTLN.
NATIVE DEBUGONLY PROC CPRINTLN(VARARGS2) = "0xf0783374333fd8ce"
//PURPOSE: Channeled print line function with implicity severity of CHANNEL_SEVERITY_WARNING
// First parameter must be a value passed to REGISTER_SCRIPT_TTY_CHANNEL
// Subsequent parameters are as per PRINTLN.
NATIVE DEBUGONLY PROC CWARNINGLN(VARARGS2) = "0x9a6c65dddbec9c52"
//PURPOSE: Channeled print line function with implicity severity of CHANNEL_SEVERITY_ERROR
// First parameter must be a value passed to REGISTER_SCRIPT_TTY_CHANNEL
// Subsequent parameters are as per PRINTLN.
NATIVE DEBUGONLY PROC CERRORLN(VARARGS2) = "0xd9911c7b5f8cd69c"
//PURPOSE: Channeled print line function with implicity severity of CHANNEL_SEVERITY_ASSERT (ie same as SCRIPT_ASSERT)
// First parameter must be a value passed to REGISTER_SCRIPT_TTY_CHANNEL
// Subsequent parameters are as per PRINTLN.
NATIVE DEBUGONLY PROC CASSERTLN(VARARGS2) = "0x83407b92d46f25c3"
//PURPOSE: Channeled print line function with implicity severity of CHANNEL_SEVERITY_DEBUG3
// First parameter must be a value passed to REGISTER_SCRIPT_TTY_CHANNEL
// Subsequent parameters are as per PRINTLN.
NATIVE DEBUGONLY PROC CDEBUG3LN(VARARGS2) = "0x1b08d1eb9d8c4931"
//PURPOSE: Channeled print line function with implicity severity of CHANNEL_SEVERITY_DEBUG2
// First parameter must be a value passed to REGISTER_SCRIPT_TTY_CHANNEL
// Subsequent parameters are as per PRINTLN.
NATIVE DEBUGONLY PROC CDEBUG2LN(VARARGS2) = "0x4dc69742196f818a"
//PURPOSE: Channeled print line function with implicity severity of CHANNEL_SEVERITY_DEBUG1
// First parameter must be a value passed to REGISTER_SCRIPT_TTY_CHANNEL
// Subsequent parameters are as per PRINTLN.
NATIVE DEBUGONLY PROC CDEBUG1LN(VARARGS2) = "0xa308f935bdeccec0"

NATIVE DEBUGONLY PROC DEBUG_PRINTCALLSTACK() = "0x355e72323aee83cc"

//PURPOSE: Triggers a breakpoint in the C++ debugger, useful for programmers
NATIVE PROC BREAKPOINT () = "0x0c2977d4e0d7f26c"

//PURPOSE: Sine (input is in degrees)
NATIVE FUNC FLOAT SIN(FLOAT T) = "0x0badbfa3b172435f"
//PURPOSE: Cosine (input is in degrees)
NATIVE FUNC FLOAT COS(FLOAT T) = "0xd0ffb162f40a139c"
//PURPOSE: Square root
NATIVE FUNC FLOAT SQRT(FLOAT T) = "0x71d93b57d07f9804"

//PURPOSE: Returns a raised to the b'th power
NATIVE FUNC FLOAT POW(FLOAT a, FLOAT b) = "0xe3621cc40f31fe2e"

//PURPOSE: Returns log base 10 of the input
NATIVE FUNC FLOAT LOG10(FLOAT F) = "0xe816e655de37fe20"

//PURPOSE: Vector magnitude
NATIVE FUNC FLOAT VMAG(VECTOR V) = "0x652d2eeef1d3e62c"
//PURPOSE: Square of vector magnitude (much less costly to compute)
NATIVE FUNC FLOAT VMAG2(VECTOR V) = "0xa8ceacb4f35ae058"
//PURPOSE: Distance between two points
NATIVE FUNC FLOAT VDIST(VECTOR A,VECTOR B) = "0x2a488c176d52cca5"
//PURPOSE: Square of distance between two points (much less costly to compute)
NATIVE FUNC FLOAT VDIST2(VECTOR A,VECTOR B) = "0xb7a628320eff8e47"

NATIVE THREADID

//PURPOSE: function that starts a new script
//PARAMS:	S - Name of the script to start
//			STACKSIZE - Stack size, in words, or zero for default
//RETURNS: the thread id of the new script (0 if no threads are available)
NATIVE FUNC THREADID START_NEW_SCRIPT (STRING S,INT STACKSIZE) = "0xe81651ad79516e48"

//PURPOSE: function that starts a new script
//PARAMS:	S - Name of the script to start
//			ARGS - Arg structure (can be any structure)
//			SIZE_OF_ARGS - SIZE_OF(args)
//			STACKSIZE - Stack size, in words, or zero for default
//RETURNS: the thread id of the new script (0 if no threads are available)
NATIVE FUNC THREADID START_NEW_SCRIPT_WITH_ARGS (STRING S,STRUCT &ARGS,INT SIZE_OF_ARGS,INT STACKSIZE) = "0xb8ba7f44df1575e1"

//PURPOSE: Works in the same way as START_NEW_SCRIPT but takes the hash of the script name as an integer instead of the name as a string
NATIVE FUNC THREADID START_NEW_SCRIPT_WITH_NAME_HASH(INT HashOfScriptName, INT STACKSIZE) = "0xeb1c67c3a5333a92"

//PURPOSE: Works in the same way as START_NEW_SCRIPT_WITH_ARGS but takes the hash of the script name as an integer instead of the name as a string
NATIVE FUNC THREADID START_NEW_SCRIPT_WITH_NAME_HASH_AND_ARGS(INT HashOfScriptName,STRUCT &ARGS,INT SIZE_OF_ARGS,INT STACKSIZE) = "0xc4bb298bd441be78"


//PURPOSE: Returns input number rounded down toward negative infinity
NATIVE FUNC INT FLOOR(FLOAT F) = "0xf34ee736cf047844"

//PURPOSE: Returns input number rounded up toward positive infinity
NATIVE FUNC INT CEIL(FLOAT F) = "0x11e019c8f43acc8a"

//PURPOSE: Returns input number rounded to the nearest number.  
//   x.5 is rounded toward positive infinity for both negative and positive inputs.
NATIVE FUNC INT ROUND(FLOAT F) = "0xf2db717a73826179"

//PURPOSE: Converts an integer to a floating-point number.
NATIVE FUNC FLOAT TO_FLOAT(INT I) = "0xbbda792448db5a89"

//PURPOSE: Takes a copy of the values of all globals variables so that RESET_GLOBALS can be called later to restore these values.
//			We're going to try taking a Snapshot when entering multiplayer and Resetting when returning to single player.
NATIVE PROC SNAPSHOT_GLOBALS() = "0xbe4c545422f87d30"

//PURPOSE: Restores all the global variables to the values they had when SNAPSHOT_GLOBALS was last called.
//			We're going to try taking a Snapshot when entering multiplayer and Resetting when returning to single player.
NATIVE PROC RESET_GLOBALS() = "0xaa8e0c99f1e62da7"

//PURPOSE: Resets the input text label to the empty string
NATIVE PROC CLEAR_TEXT_LABEL(TEXT_LABEL &TL) = "0x0a69342469658fd3"

//PURPOSE: Implements C++ operator << (shift left)
NATIVE FUNC INT SHIFT_LEFT(INT VALUE,INT COUNT) = "0xedd95a39e5544de8"

//PURPOSE: Implements C++ operator >> (shift right)
NATIVE FUNC INT SHIFT_RIGHT(INT VALUE,INT COUNT) = "0x97ef1e5bce9dc075"

// PURPOSE: Returns current thread priority (will be one of SCRIPT_PRIO_HIGHEST, SCRIPT_PRIO_NORMAL, or SCRIPT_PRIO_LOWEST)
NATIVE FUNC INT GET_THIS_THREAD_PRIORITY() = "0x02c7f121b3b2f9ca"

// PURPOSE: Sets current thread priority (should be one of SCRIPT_PRIO_HIGHEST, SCRIPT_PRIO_NORMAL, or SCRIPT_PRIO_LOWEST)
NATIVE PROC SET_THIS_THREAD_PRIORITY(INT P) = "0x42b65deef2edf2a1"

CONST_INT THREAD_PRIO_HIGHEST 0
CONST_INT THREAD_PRIO_NORMAL 1
CONST_INT THREAD_PRIO_LOWEST 2
