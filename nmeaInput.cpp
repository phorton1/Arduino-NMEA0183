//---------------------------------------------
// nmeaInput.cpp
//---------------------------------------------

#include <myDebug.h>
#include "nmea0183.h"
#include <math.h>

typedef void (*decode_fxn)(int msg_num, const char *msg);

typedef struct {
	bool seen;
	const char *name;
	// decode_fxn *decode_fxn;
	const char *descrip;
} decoder_t;



const decoder_t decoders[] =
{
	{ 0,	"APB",	"Autopilot Sentence B" },
	{ 0,	"BWC",	"Bearing & Distance to Waypoint - Geat Circle" },
	{ 0,	"BWR",	"Bearing and Distance to Waypoint - Rhumb Line" },
	{ 1,	"DBT",	"Depth below transducer" },
	{ 1,	"DPT",	"Depth of Water" },
	{ 1,	"GGA",	"Global Positioning System Fix Data" },
	{ 1,	"GLL",	"Geographic Position - Latitude/Longitude" },
	{ 0,	"MTW",	"Mean Temperature of Water" },
	{ 1,	"MWV",	"Wind Speed and Angle" },
	{ 0,	"RMA",	"Recommended Minimum Navigation Information" },
	{ 0,	"RMB",	"Recommended Minimum Navigation Information" },
	{ 1,	"RMC",	"Recommended Minimum Navigation Information" },
	{ 0,	"RSD",	"RADAR System Data" },
	{ 0,	"RTE",	"Routes" },
	{ 0,	"TTM",	"Tracked Target Message" },
	{ 1,	"VHW",	"Water speed and heading" },
	{ 0,	"VLW",	"Distance Traveled through Water" },
	{ 0,	"WPL",	"Waypoint Location" },
	{ 1,	"VTG",	"Track made good and Ground speed" },
	{ 1,	"ZDA",	"Time & Date - UTC, day, month, year and local time zone" },
	{ 1,	"AIQ",	"Proprietary AIS query" }
};


#define NUM_DECODERS (sizeof(decoders) / sizeof(decoder_t))

int show_input = 0;
static int input_msg_num = 0;



void handleNMEAInput(const String &msg)
{
	input_msg_num++;
	if (show_input)
	{
		String name = msg.substring(3,6);

		int decoder_num = 0;
		const decoder_t *found = 0;
		while (!found && decoder_num < NUM_DECODERS)
		{
			const decoder_t *dec = &decoders[decoder_num++];
			if (name.equals(dec->name))
				found = dec;
		}
		bool seen = 0;
		if (!found)
			my_error("Could not find decoder(%s)",name.c_str());
		else
			seen = found->seen;

		if (!seen || show_input == 2)
			display(0,"%-4d <-- %s",input_msg_num,msg.c_str());
	}
}



// end of nmeaInput.cpp
