//---------------------------------------------
// NMEA0183.ino
//---------------------------------------------

#include <boatSimulator.h>
#include <myDebug.h>
#include "nmea0183.h"
#include <math.h>

#define NMEA_SERIAL	Serial2
#define RXD2	16
#define TXD2	17

#define SEND_INTERVAL 	2000	// ms between sending update to E80
#define SEND_DELAY		80		// ms between each sentence

#define ALIVE_LED		2
#define ALIVE_OFF_TIME	980
#define ALIVE_ON_TIME	20

#define WITH_DEPTH_MESSAGES		1
#define WITH_GPS_SATELLITES		1
	// Sends needed GSA and GSV messages to VHF radio to spoof location.
	// These, along with the others I send, are apparently not sufficient for the E80.
	

static bool show_output = 0;


static void usage()
{
	// display(0,"STATE: wp(%d) cog(%0.1f) sog(%0.1f) input(%d) output(%d) route(%s) going_to(%d) routing(%d)",
	// 	waypoint_num,cog,sog,show_input,show_output,route_name,going_to,routing);
	display(0,"NMEA0183 USAGE",0);
	proc_entry();
	display(0,"? = show this help",0);
	display(0,"y = re-initialize simulator",0);
	display(0,"i = show received datagrams",0);
	display(0,"o = show sent datagrams",0);
	display(0,"x = start/stop simulator",0);
	display(0,"p[name] = pick route; turns off any current routing or goto",0);
	display(0,"hN, h+N, h-N = set/increment/decrement heading (cog)",0);
	display(0,"sN, s+N, s-N = set/increment/decrement speed (sog)",0);
	display(0,"jN, j+N, j-N = jump to waypoint; next waypoint, prev waypoint",0);
	display(0,"wN, w+N, w-N = set heading to waypoint; next waypoint, prev waypoint",0);
	display(0,"a = toggle 'autopilot' (0 turns off 'routing' too)",0);
	display(0,"r = toggle 'routing' (1 turns on 'autopilot' too)",0);
	proc_leave();
}



//----------------------------------
// setup
//----------------------------------

void setup()
{
	#if ALIVE_LED
		pinMode(ALIVE_LED,OUTPUT);
		digitalWrite(ALIVE_LED,1);
	#endif

	Serial.begin(921600);
	delay(1000);
	Serial.println("WTF");
	display(0,"NMEA0183 setup() started",0);

	NMEA_SERIAL.begin(38400, SERIAL_8N1, RXD2, TXD2);

	// decode_vdm("!AIVDM,1,1,,B,B52K4V@00>Qp6l1EMvw>WwtUoP06,0*72");
		// "!AIVDM,1,1,,A,H3Q;;JhhtuT60PDD00000000000,2*77");

	boat.init();

	display(0,"NMEA0183 setup() completed",0);
	usage();

	#if ALIVE_LED
		digitalWrite(ALIVE_LED,0);
	#endif
}



//--------------------------------------------------
// getSentence
//--------------------------------------------------

static const char *getSentence(int num)
	// Returns next simulation sentence to send or 0 at end of list
	// When 'going_to" a waypoint, we send one extra nmeaNavInfoB sentence.
{
#if WITH_DEPTH_MESSAGES == 0
	if (num == 2)
		num = 3;
#endif

#if WITH_GPS_SATELLITES == 0
	if (num >= 5 && num <= 8)
		num = 9;
#endif

	if (!boat.getAutopilot() && num == 9)
		num = 10;

	switch (num)
	{
		case 0: return nmeaNavInfoC(boat.getLat(), boat.getLon(), boat.getSOG(), boat.getCOG());
		case 1: return nmeaLatLon(boat.getLat(), boat.getLon());
		case 2: return nmeaDepth(boat.getDepth());
		case 3: return nmeaWind('R',boat.apparentWindSpeed(), boat.apparentWindAngle());
			// R = apparent wind relative to the bow
		case 4: return nmeaWaterSpeed(boat.getSOG(), boat.getCOG());
			// The unmoving water is moving at sog, cog+180 relative to the boat
			// Once this was implemented, The E80 started calculating the "True"
			// and "Ground" speeds from the Relative wind, and I no longer needed
			// to send the 'True' wind speed and direction.

		// fake satellite sentences

		case 5:
		case 6:
		case 7:
		case 8:
			return fakeGPSSatellites(num - 5);


		// going to waypoints and automatic route advancement

		case 9:
		{
			int wp_num = boat.getWaypointNum();
			const waypoint_t *wp = boat.getWaypoint(wp_num);
			const char *send = nmeaNavInfoB(
				boat.getLat(),
				boat.getLon(),
				boat.getSOG(),
				boat.getCOG(),
				wp->name,
				boat.distanceToWaypoint(),
				boat.headingToWaypoint(),
				boat.getArrived());
			return send;

		}	// send navInfoB (going to)


#if 0	// UNUSED messages kept for posterities sake

		case N:
			return nmeaRTE(waypoints, num_waypoints, going_to ? waypoint_num : -1);
				// This does not appear to do anything on the E80

		case N:
		{
			// This was necessary until I sent InfoC, LatLon, and finally WaterSpeed msgs
			// The E80 combined with NMEA013 is lame when it comes to the wind.
			// The E80 doesn't calculate anything.
			// The E80 only accepts MWV messages (which are, themselves, lame)
			// Note that 'T' in MWV does not mean 'True' it means 'Theoretical'
			// The yellow arrow is the True wind
			// The E80 shows both the "True" and "Relative"
			//		as relative	to the (P) or (S) of the bow
			return nmeaWind('R',app_wind_speed, app_wind_angle);
			float use_wind = wind_angle - cog;
			if (use_wind < 0) use_wind += 360;
			return nmeaWind('T',wind_speed, use_wind);
				// wacky NMEA definition of Theoretical wind is
				// "AS IF THE BOAT WAS STANDING STILL"
				// and relative to the bow.
		}
#endif	// unimplemented messages

	}	// switch (num)

	return 0;   // done with sentences

}	// getSentence()



//--------------------------------------------------
// handleCommand()
//--------------------------------------------------

static void handleCommand(int command, char *buf)
{
	int val = 1;
	int inc = 0;
	char *orig_buf = buf;
	if (*buf == '+')
	{
		inc = 1;
		buf++;
	}
	else if (*buf == '-')
	{
		inc = -1;
		buf++;
	}
	if (*buf >= '0' && *buf <= '9')
	{
		val = atoi(buf);
	}

	display(0,"command(%c) inc(%d) val(%d)",command,inc,val);
	if (command == 'h')
	{
		double cog = boat.getCOG();

		if (inc)
			cog += (inc * val);
		else
			cog = val;

		if (cog < 0)
			cog += 360;
		if (cog > 360)
			cog -= 360;

		display(0,"HEADING (COG) <= %0.1f",cog);
		boat.setCOG(cog);
	}
	else if (command == 's')
	{
		double sog = boat.getSOG();
		if (inc)
			sog += (inc * val);
		else
			sog = val;
		if (sog > 99)
			sog = 99;
		if (sog < 0)
			sog = 0;

		display(0,"SPEED (SOG) <= %0.1f",sog);
		boat.setSOG(sog);
	}
	else if (command == 'w')
	{
		int wp_num = boat.getWaypointNum();
		int num_wps = boat.getNumWaypoints();

		if (inc)
			wp_num += (inc * val);
		else
			wp_num = val;

		if (wp_num < 0)
			wp_num = 0;
		if (wp_num > num_wps - 1)
			wp_num = num_wps - 1;

		boat.setWaypointNum(wp_num);
	}
	else if (command == 'j')
	{
		int wp_num = boat.getWaypointNum();
		int num_wps = boat.getNumWaypoints();

		// we presume that the waypoint we are leaving is
		// the set waypoint-1

		wp_num--;
		if (wp_num<0) wp_num = 0;

		if (inc)
			wp_num += (inc * val);
		else
			wp_num = val;

		if (wp_num < 0)
			wp_num = 0;
		if (wp_num > num_wps - 1)
			wp_num = num_wps - 1;

		boat.jumpToWaypoint(wp_num);
	}
	else if (command == 'p')
	{
		boat.setRoute(orig_buf);
	}
}


//------------------------------------------------
// handleSerial()
//------------------------------------------------

static void handleSerial()
{
	// Serial UI

	if (Serial.available())
	{
		#define MAXCOMMAND	12

		static int in_command = 0;
		static int cmd_ptr = 0;
		static char command[MAXCOMMAND+1];

		int c = Serial.read();

		if (in_command)
		{
			if (c == 0x0a || cmd_ptr==MAXCOMMAND)
			{
				command[cmd_ptr++] = 0;
				handleCommand(in_command,command);
				in_command = 0;
				cmd_ptr = 0;
			}
			else if (c != 0x0d)
			{
				command[cmd_ptr++] = c;
			}
		}

		// program specific

		else if (c == '?')
		{
			usage();
		}
		else if (c == 'i')
		{
			show_input = (show_input + 1) % 3;
			display(0,"SHOW INPUT(%d)",show_input);
		}
		else if (c == 'o')
		{
			show_output = !show_output;
			display(0,"SHOW OUTPUT(%d)",show_output);
		}

		// simulator

		else if (c == 'x')
		{
			if (boat.running())
				boat.stop();
			else
				boat.start();
		}
		else if (c == 'y')
		{
			boat.init();
		}
		else if (c == 'a')
		{
			boat.setAutopilot(!boat.getAutopilot());
		}
		else if (c == 'r')
		{
			boat.setRouting(!boat.getRouting());
		}
		else if (c == 'h' || c == 's' || c == 'w' || c == 'j' || c == 'p')
		{
			in_command = c;
		}
	}
}	// handleSerial()



//--------------------------------------------------------------
// loop
//--------------------------------------------------------------


void loop()
{
	#if ALIVE_LED
		static bool alive_on = 0;
		static uint32_t last_alive_time = 0;
		uint32_t alive_now = millis();
		uint32_t alive_delay = alive_on ? ALIVE_ON_TIME : ALIVE_OFF_TIME;
		if (alive_now - last_alive_time >= alive_delay)
		{
			alive_on = !alive_on;
			digitalWrite(ALIVE_LED,alive_on);
			last_alive_time = alive_now;
		}
	#endif
	 

	if (boat.running())
	{

		static int send_num = 0;
		static uint32_t last_send_time = 0;
		static uint32_t last_update_ms = 0;

		boat.run();
		uint32_t now = millis();
		uint32_t diff = now - last_send_time;
		if ((!send_num && (diff > SEND_INTERVAL)) ||
			 (send_num && (diff > SEND_DELAY)))
		{
			last_send_time = millis();
			const char *msg = getSentence(send_num);
			if (msg)
			{
				send_num++;
				if (show_output)
					display(0,"%-4d --> %s",send_num,msg);
				NMEA_SERIAL.println(msg);
			}
			else
			{
				send_num = 0;
			}
		}
	}	// if running


	handleSerial();


	#if 1	// listen for input data

		while (NMEA_SERIAL.available())
		{
			#define MAX_MSG 180
			int c = NMEA_SERIAL.read();
			static char buf[MAX_MSG+1];
			static int buf_ptr = 0;

			// display(0,"got Serial2 0x%02x %c",c,c>32 && c<127 ? c : ' ');

			if (buf_ptr >= MAX_MSG || c == 0x0a)
			{
				buf[buf_ptr] = 0;
				handleNMEAInput(String(buf));
				buf_ptr = 0;
			}
			else if (c != 0x0d)
			{
				buf[buf_ptr++] = c;
			}
		}

	#endif

}	// loop()



// end of NMEA0183.ino
