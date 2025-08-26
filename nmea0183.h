//---------------------------------------------
// nmea0183.h
//---------------------------------------------

#pragma once


//---------------------------
// in encode0183.cpp
//---------------------------

extern const char *nmeaDepth(float depth);
extern const char *nmeaLatLon(float latitude, float longitude);
extern const char *nmeaNavInfoC(float lat, float lon, float sog, float cog);
extern const char *nmeaWind(char type, float speed, float angle);
extern const char *nmeaWaterSpeed(float rel_water_speed, float true_rel_water_degrees);

// issued while "going_to" a waypoint

const char *nmeaNavInfoB(
	float lat,
	float lon,
	float sog,
	float cog,
	const char *wp_name,
	float dist_to_wp,
	float head_to_wp,
	bool arrived);

// fake gps satellites for VHF radio

const char *fakeGPSSatellites(int num);


//---------------------------
// in decode0183.cpp
//---------------------------

extern int show_input;
extern void handleNMEAInput(const String &msg);
extern void decode_vdm(const char* p);


// end of nmea0183.h
