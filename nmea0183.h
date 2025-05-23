//---------------------------------------------
// nmea0183.h
//---------------------------------------------

#pragma once

#define NMEA_SERIAL	Serial2
#define RXD2	16
#define TXD2	17


typedef struct
{
	const char *name;
	float lat;
	float lon;
} waypoint_t;


typedef struct
{
	const char *name;
	const waypoint_t *wpts;
	int	num_wpts;
} route_t;


extern const waypoint_t *waypoints;
extern int num_waypoints;
	// based on currently picked route

extern bool show_output;

extern float headingToWaypoint(const waypoint_t *waypoint);
extern float distanceToWaypoint(const waypoint_t *waypoint);
	// in simulator cpp

//---------------------------
// in nmea0183.cpp
//---------------------------

extern const char *nmeaDepth(float depth);
extern const char *nmeaLatLon(float latitude, float longitude);
extern const char *nmeaNavInfoC(float lat, float lon, float sog, float cog);
extern const char *nmeaWind(char type, float speed, float angle);
extern const char *nmeaWaterSpeed(float rel_water_speed, float true_rel_water_degrees);

// issued while "going_to" a waypoint

extern const char *nmeaNavInfoB(float lat, float lon, float sog, float cog, const waypoint_t *wp, bool *arrived);


//---------------------------
// nmeaInput.cpp
//---------------------------

extern int show_input;
extern void handleNMEAInput(const String &msg);


//---------------------------
// wherever
//---------------------------

extern void nmeaExperiment();


// end of nmea0183.h
