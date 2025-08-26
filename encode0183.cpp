//---------------------------------------------
// encode0183.cpp
//---------------------------------------------
// Sentences purported to be sent by E80
// and whether or not I have seen, decoded, or sent them
// GOTO = these start coming out when we have a destination waypoint
//
//			GOTO	APB Autopilot Sentence "B"
//  		GOTO	BWC Bearing & Distance to Waypoint - Geat Circle
//			GOTO	BWR Bearing and Distance to Waypoint - Rhumb Line
//			seen	DBT Depth below transducer
// sent		seen	DPT Depth of Water
//			seen	GGA Global Positioning System Fix Data
// sent		seen	GLL Geographic Position - Latitude/Longitude
//					MTW Mean Temperature of Water
// sent		seen	MWV Wind Speed and Angle
//					RMA Recommended Minimum Navigation Information
// sent		GOTO	RMB Recommended Minimum Navigation Information
// sent		seen	RMC Recommended Minimum Navigation Information
//  				RSD RADAR System Data
// experiment		RTE Routes
//					TTM Tracked Target Message
// sent				VHW Water speed and heading
//					VLW Distance Traveled through Water
//					WPL Waypoint Location
//			seen	VTG Track made good and Ground speed
//			seen	ZDA Time & Date - UTC, day, month, year and local time zone
//
// The following have been seen but are not purported to be sent
//
//			seen 	$DUAIQ,TXS*3B	- proprietary queryx
//
// Notably, it does not send (or probably support) some messages
// I would like to send, including
//
//		RPM - causes unknown sentence
//
// NMEA0183 SENTENCES ARE LIMITED TO 80 BYTES, hence RTE's multiple sentence capabilities


#include <myDebug.h>
#include "nmea0183.h"
#include <math.h>

#define MAX_NMEA_MSG	180
	// my buffer size is way bigger than official NMEA0183
	// maximum of 80 bytes
char nmea_buf[MAX_NMEA_MSG+1];


//----------------------------------------------------
// utilities
//----------------------------------------------------

static void checksum()
{
	int at = 1;
	char *ptr = &nmea_buf[1]; 	// skip the leading $
	unsigned char byte = *ptr++;
	while (*ptr)
	{
		byte ^= *ptr++;
	}
	sprintf(ptr,"*%02x",byte);
}


static const char *standardDate()
	// returns constant fake date
{
	return "100525";	// 2025-05-10
}


static const char *standardTime()
	// returns time of day as millis() since boot
{
	int secs = 12*3600 + millis()/1000;
	int hour = secs / 3600;
	int minute = (secs - hour * 3600) / 60;
	secs = secs % 60;
	if (hour > 23)
		hour = 0;
	static char time_buf[20];
	sprintf(time_buf,"%02d%02d%02d.00",hour,minute,secs);
	return time_buf;
}


static const char *standardLat(float latitude)
	// returns NMEA0183 formated "latitude,N/S" with comma
{
	char ns = 'N';
	if (latitude < 0)
	{
		latitude = abs(latitude);
		ns = 'S';
	}
	int d_lat = latitude;
	float m_lat = (latitude - d_lat) * 60.0;

	static char lat_buf[20];
	sprintf(lat_buf,"%02d%07.4f,%c",d_lat,m_lat,ns);
		// note %0X.Yf print spec is for Y decimal
		// places padded with zeros to overal length X
	return lat_buf;
}


static const char *standardLon(float longitude)
	// returns NMEA0183 formated "longitude,N/S" with comma
{
	char ew = 'W';
	if (longitude < 0)
	{
		longitude = abs(longitude);
		ew = 'W';
	}
	int d_lon = longitude;
	float m_lon = (longitude - d_lon) * 60.0;

	static char lon_buf[20];
	sprintf(lon_buf,"%03d%07.4f,%c",d_lon,m_lon,ew);
	return lon_buf;
}



//---------------------------------------------------
// sentences
//---------------------------------------------------

const char *nmeaLatLon(float latitude, float longitude)
	// GP = Global Positioning System
	// GLL = Geographic Position - Latitude/Longitude
	// E80 takes about 10 seconds to lose this
{
	//	1) Latitude
	//	2) N or S (North or South)
	//	3) Longitude
	//	4) E or W (East or West)
	//	5) Time (UTC)
	//	6) Status A - Data Valid, V - Data Invalid

	//                       12 34 5  6
	sprintf(nmea_buf,"$GPGLL,%s,%s,%s,A",
		standardLat(latitude),
		standardLon(longitude),
		standardTime());
	checksum();
	return nmea_buf;
}



const char *nmeaDepth(float depth)
	// SD = Sounder device
	// DPT = Water Depth
{
	const float FEET_TO_METERS = 0.3048;
	float d_meters = depth * FEET_TO_METERS;
	// 1) Depth, meters
	// 2) Offset from transducer;
	// 	  positive means distance from transducer to water line,
	// 	  negative means distance from transducer to keel
	//                       1     2
	sprintf(nmea_buf,"$SDDPT,%0.1f,0.0",d_meters);
	checksum();
	return nmea_buf;
}


const char *nmeaNavInfoC(float lat, float lon, float sog, float cog)
	// GP = GPS device
	// RMC = Recommended Minimum Navigation Information 'C'
	// "$GPRMC,092750.000,A,5321.6802,N,00630.3372,W,0.02,31.66,280511,,,A*43",
{
	// 1) Time (UTC)
	// 2) Status, A=valid, V=Navigation receiver warning
	// 3) Latitude
	// 4) N or S
	// 5) Longitude
	// 6) E or W
	// 7) Speed over ground, knots
	// 8) Track made good, degrees true
	// 9) Date, ddmmyy
	// 10) Magnetic Variation, degrees
	// 11) E or W
	//                       1  2 34 56 7     8     9 10&11 missing
	sprintf(nmea_buf,"$GPRMC,%s,A,%s,%s,%0.1f,%0.1f,%s,,,",
		standardTime(),
		standardLat(lat),
		standardLon(lon),
		sog,
		cog,
		standardDate());
	checksum();
	return nmea_buf;
}



const char *nmeaWind(char type, float speed, float angle)
	// WI = Weather Instruments
	// MWV = Wind Speed and Angle
	// the E80 does not not know VWR - Relative Wind Speed and Angle
	// sentence MWV device WI = Weather Instruments
	// 		can send (T)heoretical true or (R)elative
	// until cog, sog, and water speed are set
	//		the E80 does not calculate the other winds for us
	// note that even with T the degrees are passed relative to bow
{
	// 1) Wind Angle, 0 to 360 degrees
	// 2) Reference, R = Relative, T = Theoretical true
	// 3) Wind Speed
	// 4) Wind Speed Units, K/M/N
	// 5) Status, A = Data Valid
	//                       1     2  3     4 5
	sprintf(nmea_buf,"$WIMWV,%0.1f,%c,%0.1f,N,A",angle,type,speed);
	checksum();
	return nmea_buf;
}



const char *nmeaWaterSpeed(float rel_water_speed, float true_rel_water_degrees)
	// VW = Velocity Sensor, Speed Log, Water, Mechanical
	// VHW device
	// The sentence expects the RELATIVE degrees as True or Magnetic !?!
	// We only send the 'T' true degrees and speed in knots
	// Our simulator currently assumes no current, so
	// 		water speed = sog
	// 		water_heading = cog, which may be off by 180, not sure
{
	// 1) Degress True
	// 2) T = True
	// 3) Degrees Magnetic
	// 4) M = Magnetic
	// 5) Knots (speed of vessel relative to the water)
	// 6) N = Knots
	// 7) Kilometers (speed of vessel relative to the water)
	// 8) K = Kilometres
	//                       1     2 345     678
	sprintf(nmea_buf,"$VWVHW,%0.1f,T,,,%0.1f,N,,",
		true_rel_water_degrees,rel_water_speed);
	checksum();
	return nmea_buf;

	// Yay! This gave us the ground wind speed on the E80!
}



const char *nmeaNavInfoB(float lat, float lon, float sog, float cog, const char *wp_name, float dist_to_wp, float head_to_wp, bool arrived)
	// Sent when "going_to" a waypoint
	// AP = Autopilot
	// $ECRMB,A,0.000,R,,Waypoint 8,0920.044,N,08214.648,W,0.12,269.8,,V,A*5B
{
	// 1) Status, V = Navigation receiver warning
	// 2) Cross Track error - nautical miles
	// 3) Direction to Steer, Left or Right
	// 4) E80 unused FROM Waypoint ID
	// 5) E80 TO Waypoint ID (reversed from spec)
	// 6) Destination Waypoint Latitude
	// 7) N or S
	// 8) Destination Waypoint Longitude
	// 9) E or W
	// 10) Range to destination in nautical miles
	// 11) Bearing to destination in degrees True
	// 12) Destination closing velocity in knots
	// 13) Arrival Status, A = Arrival Circle Entered; V=reset arrival alarm
	// 14) Checksum

	float xte = 0.01;
	char lr = 'R';
	const char *arrive = arrived ? "A" : "V";

	//                       1 2     3  4  5  67 89 10    11    12    13
	sprintf(nmea_buf,"$APRMB,A,%0.3f,%c,%s,%s,%s,%s,%0.3f,%0.1f,%0.1f,%s,",
		xte,				// 2
		lr,					// 3
		"unused_from_wp",	// 4
		wp_name,			// 5
		standardLat(lat),	// 67
		standardLon(lon),	// 89
		dist_to_wp,			// 10
		head_to_wp,			// 11
		cog,				// 12
		arrive);			// 13

	// $APRMB,A,0.100,R,,Popa1,0920.0450,N,08214.5230,W,12.000,149.2,149.2,,*5a

	checksum();
	return nmea_buf;

	// E80 lights up a waypoint,
	// shows xte, name, bearing, and distance to waypoint,
	// and shows the time to waypoint if the boat is moving,
	// along with showing the WPT STOP_GOTO and RESTART_XTE buttons
	// and starts sending out these messages:
	//
	// $ECAPB,A,A,0.010,R,N,V,V,,,Popa1,000.0,T,,,A*67
	// $ECBWC,120044.49,0926.245,N,08214.523,W,000.0,T,000.0,M,6.20,N,Popa1,A*6A
	// $ECBWR,120044.49,0926.245,N,08214.523,W,000.0,T,000.0,M,6.20,N,Popa1,A*7B
	// $ECRMB,A,0.010,R,,Popa1,0926.245,N,08214.523,W,6.20,000.0,,V,A*7E
	//
	// When we start sending the "A" arrival letter at dist=0.064nm
	// the E80 starts beeping, shows the Waypoint Arrival Dialog and
	// the ACKNOWLEDGE button. When the user acknowledges, or we 
	// switch to 'V' it stops.
}



const char *fakeGPSSatellites(int num)		// 0 to 3 to send out 1 GSA and three GSB sentences
{
	const char* gsa = "$GPGSA,A,3,03,05,07,09,11,13,15,17,19,21,23,25,1.8,1.0,1.5";
	const char* gsv1 = "$GPGSV,3,1,12,03,45,120,42,05,60,180,45,07,30,270,40,09,15,090,38";
	const char* gsv2 = "$GPGSV,3,2,12,11,75,045,43,13,50,135,41,15,25,225,39,17,10,315,37";
	const char* gsv3 = "$GPGSV,3,3,12,19,65,060,44,21,40,150,42,23,20,240,36,25,05,330,35";

	const char *sentence =
		(num == 3) ? gsv3 :
		(num == 2) ? gsv2 :
		(num == 1) ? gsv3 : gsa;
	strcpy(nmea_buf, sentence);
	checksum();
	return nmea_buf;
}





//----------------------------------------------------
// unused sentences kept for posterities sake
//----------------------------------------------------


#if 0

	const char *nmeaRPM(int rpm)
		// sentence RPM device ER = Engine Room Monitoring Systems
		// causes "unknown sentence" on E80
	{
		// E=Engine, 1=Engine Number, 8=Pitch, A=valid
		sprintf(nmea_buf,"$ERRPM,E,1,%d,8,A",rpm);
		checksum();
		return nmea_buf;
	}


	const char *nmeaRTE(const waypoint_t *wp, int num_waypoints, int waypoint_num)
		// AP = Autopilot
		// waypoint_num >=0 means going_to
		// NOTE THAT NMEA0183 sentences have a max of 80 chars (despite my bigger buffer),
		// 		and that this will not work if the number, or length of the waypoint names
		// 		would require multiple messages.
		// I thought this might be needed to handle waypoint goto's, but
		// as far as I can tell it does nothing on the E80.
		//
		// In general, with NMEA0183, I cannot create, or get, waypoints or
		// routes to/from the E80, except for temporary ones that are created
		// as part of the goto

	{
		char type = waypoint_num >= 0 ? 'w' : 'c';
		//                      1 2 3 4
		sprintf(nmea_buf,"$APRTE,1,1,%c,RTE12",type);
			// 1 = total # messages
			// 2 = this message number
			// 3 = c=complete route, or w=working route
			// 4 = route id

		if (type == 'w')
		{
			strcat(nmea_buf,",");
			if (waypoint_num > 0)
				strcat(nmea_buf,wp[waypoint_num - 1].name);
			strcat(nmea_buf,",");
			strcat(nmea_buf,wp[waypoint_num].name);
		}

		for (int i=0; i<num_waypoints; i++)
		{
			if (type == 'w' && (
				i == waypoint_num ||
				i == waypoint_num - 1))
			{
				// skip it
			}
			else
			{
				strcat(nmea_buf,",");
				strcat(nmea_buf,wp[i].name);
			}
		}
		checksum();
		display(0,"lenght of RTE message=%d",strlen(nmea_buf));
		return nmea_buf;
	}

#endif	// 0 unused sentences



// end of nmea0183.cpp
