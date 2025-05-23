//---------------------------------------------
// simulator.cpp
//---------------------------------------------
// NMEA0183 command processor and simulator.
// The E80 invalidates data after about 2 seconds and
// 		alarms on loss of position, heading, depth, etc.
// We currently assume no current, so the water always
//		moves at sog,cog+180 relative to the boat, and
// 		the heading is always === cog

#include <myDebug.h>
#include <math.h>
#include "nmea0183.h"
#include "ge_routes.h"


#define SEND_INTERVAL 			2000	// ms between sending update to E80
#define SEND_DELAY				80		// ms between each sentence
#define CLOSEST_NONE			65535	// exact




// UI variables

bool show_output = 0;

static bool running = 0;

static float depth;			// 0-999.9	depth below transducer
static float sog;			// 0-99.9	speed over ground, knots,
static float cog;			// 0-359.5  course over ground
static float wind_angle;	// 0-359.5  TRUE wind angle (relative to ground)
static float wind_speed;	// 0-127.9	TRUE wind speed (relative to ground)
static float latitude;		// initialized and calculated
static float longitude;		// initialized and calculated

static const char* route_name;			// picked route name (must be at least one!)
const waypoint_t *waypoints;		// array of waypoitns for picked route
int num_waypoints;				// number of waypoitns for picked route

static int waypoint_num;		// 0..num_waypoints-1
static bool going_to;			// going to a waypoint?
static bool routing;			// are we routing through waypoints?
static uint16_t closest_goto;	// integer feet
static bool arrived;


// state variables

static bool inited = 0;
static int send_num = 0;
static uint32_t last_send_time = 0;
static uint32_t last_update_ms = 0;

static float app_wind_speed;
static float app_wind_angle;




// forward

static void calculateApparentWind();


//-----------------------------------------------
// implementation
//-----------------------------------------------

static void setRoute(const route_t *rte)
{
	going_to = 0;
	routing = 0;
	closest_goto = CLOSEST_NONE;
	
	route_name = rte->name;
	waypoints = rte->wpts;
	num_waypoints = rte->num_wpts;
	display(0,"ROUTE(%s) num_waypoints=%d",route_name,num_waypoints);

	latitude = waypoints[0].lat;
	longitude = waypoints[0].lon;

	sog = 0;
	cog = 180;
	if (num_waypoints)
	{
		waypoint_num = 1;
		cog = headingToWaypoint(&waypoints[1]);
	}
}


static void init_sim()
{
	display(0,"INITIALIZE SIMULATOR",0);

	depth = 10;
	sog = 0;
	cog = 180;
	wind_angle = 90;	// true east
	wind_speed = 12;
	waypoint_num = 0;

	setRoute(&routes[0]);
	calculateApparentWind();

	going_to = 0;
	routing = 0;
	closest_goto = CLOSEST_NONE;

	send_num = 0;
	last_send_time = 0;
	last_update_ms = 0;
	inited = true;
}


static void usage()
{
	// display(0,"STATE: wp(%d) cog(%0.1f) sog(%0.1f) input(%d) output(%d) route(%s) going_to(%d) routing(%d)",
	// 	waypoint_num,cog,sog,show_input,show_output,route_name,going_to,routing);
	display(0,"USAGE",0);
	proc_entry();
	display(0,"? = show this help",0);
	display(0,"x = start/stop simulator",0);
	display(0,"y = re-initialize simulator",0);
	display(0,"i = show received datagrams",0);
	display(0,"o = show sent datagrams",0);
	display(0,"p[name] = pick route; turns off any current routing or goto",0);
	display(0,"hN, h+N, h-N = set/increment/decrement heading (cog)",0);
	display(0,"sN, s+N, s-N = set/increment/decrement speed (sog)",0);
	display(0,"jN, j+N, j-N = jump to waypoint; next waypoint, prev waypoint",0);
	display(0,"wN, w+N, w-N = set heading to waypoint; next waypoint, prev waypoint",0);
	display(0,"g = toggle 'going_to' (0 turns off 'routing' too)",0);
	display(0,"r = toggle 'routing' (1 turns on 'going_to' too)",0);
	display(0,"z = experiment",0);
	proc_leave();
}



#define rad2deg(degrees)	((degrees) * M_PI / 180.0)
#define deg2rad(radians)	((radians) * (180.0 / M_PI))


static void updatePosition()
	// Calculate and set new latitude and longitude
	// based on cog, sog, and millis() since last call
{
	uint32_t now = millis();
	double elapsed_secs = (now - last_update_ms) / 1000.0;
	last_update_ms = now;
	if (sog == 0)
		return;

	// adjust our heading if we're routing to a waypoint
	if (routing && !arrived)
	{
		cog = headingToWaypoint(&waypoints[waypoint_num]);
		// calculateApparentWind();
		//  	too much debug output - it's approximate 
	}

	const double EARTH_RADIUS = 6371000; 	// in meters
	const double KNOTS_TO_MPS = 0.514444;
	double distance_m = sog * KNOTS_TO_MPS * elapsed_secs;
	double cog_rad = rad2deg(cog);
	double delta_lat = (distance_m * cos(cog_rad)) / EARTH_RADIUS;
	double delta_lon = (distance_m * sin(cog_rad)) / (EARTH_RADIUS * cos(rad2deg(latitude)));
	latitude += deg2rad(delta_lat);
	longitude += deg2rad(delta_lon);


}


float headingToWaypoint(const waypoint_t *wp)
	// Returns heading in true degrees to given waypoint
	// from current latitute and longitude
{
	double delta_lon = wp->lon - longitude;
	double y = sin(rad2deg(delta_lon)) * cos(rad2deg(wp->lat));
	double x = cos(rad2deg(latitude)) * sin(rad2deg(wp->lat)) -
		sin(rad2deg(latitude)) * cos(rad2deg(wp->lat)) * cos(rad2deg(delta_lon));
	double heading_rad = atan2(y, x);
	double heading_deg = deg2rad(heading_rad);
	heading_deg = fmod((heading_deg + 360.0), 360.0);
	return heading_deg;
}


float distanceToWaypoint(const waypoint_t *wp)
	// Returns distance in NM to given waypoint
	// from current latitute and longitude
{
	const float EARTH_RADIUS_NM = 3440.065; 	// in nautical miles
    float d_lat = rad2deg(wp->lat - latitude);
    float d_lon = rad2deg(wp->lon - longitude);
    float a = sin(d_lat / 2) * sin(d_lat / 2) +
        cos(rad2deg(latitude)) * cos(rad2deg(wp->lat)) *
        sin(d_lon / 2) * sin(d_lon / 2);
    float c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
    return EARTH_RADIUS_NM * c;
}


static void calculateApparentWind()
	// Calculates and sets app_wind_angle and app_wind_speed
	// based on cog, sog, wind_angle, and wind_epeed
	// as degrees relative to the bow of the boat
{
	#define dbg_wind 	1

	display(0,"calculateApparentWind speed/angle boat(%0.3f,%0.3f) wind(%0.3f,%0.3f)",
		sog,cog,wind_speed,wind_angle);

	float wx = -wind_speed * cos(rad2deg(wind_angle));
	float wy = -wind_speed * sin(rad2deg(wind_angle));
	float bx = sog * cos(rad2deg(cog));
	float by = sog * sin(rad2deg(cog));
	float ax = wx - bx;
	float ay = wy - by;

	proc_entry();
	display(dbg_wind,"wx(%0.3f) wy(%0.3f) bx(%0.3f) by(%0.3f) ax(%0.3f) ay(%0.3f)", wx,wy,bx,by,ax,ay);

	app_wind_speed = sqrt(ax * ax + ay * ay);
	app_wind_angle = deg2rad(atan2(ay, ax));
	app_wind_angle += 180.0;
	if (app_wind_angle >= 360.0)
		app_wind_angle -= 360.0;
	display(dbg_wind,"app_wind_speed(%0.3f) absolute angle(%0.3f)", app_wind_speed,app_wind_angle);

	app_wind_angle -= cog;
	display(dbg_wind,"relative angle(%0.3f)", app_wind_angle);

	if (app_wind_angle < 0)
	{
		app_wind_angle += 360.0;
		display(0,"normalized angle(%0.3f)", app_wind_angle);
	}

	display(0,"app_wind_speed(%0.3f) app_wind_angle(%0.3f)", app_wind_speed, app_wind_angle);
	proc_leave();
}



//--------------------------------------------------
// getSentence
//--------------------------------------------------

static const char *getSentence(int num)
	// Returns next simulation sentence to send or 0 at end of list
	// When 'going_to" a waypoint, we send one extra sentence.
{
	int num_msgs = going_to ? 5 : 4;

	if (num == num_msgs)
		return 0;
	switch (num)
	{
		case 0: return nmeaNavInfoC(latitude, longitude, sog, cog);
		case 1: return nmeaLatLon(latitude,longitude);
		// case 2: return nmeaDepth(depth);
		case 2: return nmeaWind('R',app_wind_speed, app_wind_angle);
			// R = apparent wind relative to the bow
		case 3: return nmeaWaterSpeed(sog,cog);
			// The unmoving water is moving at sog, cog+180 relative to the boat
			// Once this was implemented, The E80 started calculating the "True"
			// and "Ground" speeds from the Relative wind, and I no longer needed
			// to send the 'True' wind speed and direction.

		// going to waypoints and automatic route advancement

		case 4:
		{
			const waypoint_t *wp = &waypoints[waypoint_num];
			const char *send = nmeaNavInfoB(latitude, longitude, sog, cog, wp, &arrived);
			if (arrived)
				display(0,"WAYPOINT ARRIVAL %s",wp->name);
			if (routing && (arrived || closest_goto != CLOSEST_NONE))
			{
				// if it is the last waypoint, we stop the boat
				//		and stop routing and going_to
				// otherwise, we advance to the next waypoint
				// However, we only want to advance to the next waypoint
				//		when we have reached the closest point to the
				//		current waypoint.  This also gives the e80 time
				//		to beep.

				const float NM_TO_FEET = 6076.12;
				uint16_t dist = distanceToWaypoint(wp) * NM_TO_FEET;
				if (closest_goto == CLOSEST_NONE)		// first arrival message
				{
					display(0,"FIRST ARRIVAL feet=%d",dist);
					closest_goto = dist;
				}
				else if (dist <= closest_goto)			// still getting closer
				{
					display(0,"STILL ARRIVING feet=%d",dist);
					closest_goto = dist;
				}
				else	// moving away, advance to next waypoint
				{
					display(0,"FINISHED ARRIVING feet=%d",dist);
					waypoint_num++;
					closest_goto = CLOSEST_NONE;
					if (waypoint_num >= num_waypoints)
					{
						display(0,"ROUTE_COMPLETE",0);
						going_to = false;
						route_name = "";
						sog = 0;
					}
					else
					{
						wp = &waypoints[waypoint_num];
						display(0,"NEXT WAYPOINT %s",wp->name);
						cog = headingToWaypoint(wp);
					}
					calculateApparentWind();
				}
			}	// routing
			
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
}	// getSentence()



//--------------------------------------------------
// handleCommand()
//--------------------------------------------------

static void start_sim()
{
	display(0,"STARTING SIMULATOR",0);
	running = true;
}
static void stop_sim()
{
	display(0,"STOPPING SIMULATOR",0);
	running = false;
}


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
		if (inc)
			cog += (inc * val);
		else
			cog = val;

		if (cog < 0)
			cog += 360;
		if (cog > 360)
			cog -= 360;

		display(0,"HEADING (COG) <= %0.1f",cog);
		calculateApparentWind();
	}
	else if (command == 's')
	{
		if (inc)
			sog += (inc * val);
		else
			sog = val;
		if (sog > 99)
			sog = 99;
		if (sog < 0)
			sog = 0;

		display(0,"SPEED (SOG) <= %0.1f",sog);
		calculateApparentWind();
	}
	else if (command == 'w')
	{
		if (inc)
			waypoint_num += (inc * val);
		else
			waypoint_num = val;

		if (waypoint_num < 0)
			waypoint_num = 0;
		if (waypoint_num > num_waypoints - 1)
			waypoint_num = num_waypoints - 1;

		const waypoint_t *wp = &waypoints[waypoint_num];
		cog = headingToWaypoint(wp);
		display(0,"WAYPOINT[%d} %s %0.6f %0.6f  heading=%0.1f",
			waypoint_num,
			wp->name,
			wp->lat,
			wp->lon,
			cog);
		calculateApparentWind();
	}
	else if (command == 'j')
	{
		if (inc)
			waypoint_num += (inc * val);
		else
			waypoint_num = val;

		if (waypoint_num < 0)
			waypoint_num = 0;
		if (waypoint_num > num_waypoints - 1)
			waypoint_num = num_waypoints - 1;

		const waypoint_t *wp = &waypoints[waypoint_num];
		latitude = wp->lat;
		longitude = wp->lon;

		display(0,"JUMP TO WAYPOINT[%d} %s %0.6f %0.6f",
			waypoint_num,
			wp->name,
			wp->lat,
			wp->lon);
	}
	else if (command == 'p')
	{
		routing = 0;
		going_to = 0;
		closest_goto = CLOSEST_NONE;
		const route_t *found = 0;

		for (int i=0; i<NUM_ROUTES; i++)
		{
			const route_t *rte = &routes[i];
			if (String(orig_buf).equalsIgnoreCase(String(rte->name)))
			{
				found = rte;
				i = NUM_ROUTES;
			}
		}

		if (!found)
		{
			my_error("Could not find route(%s)",orig_buf);
		}
		else
		{
			setRoute(found);
		}
	}
}


//------------------------------------
// simulator_run()
//-----------------------------------


void simulator_run()
{
	if (!inited)
	{
		init_sim();
		usage();
	}

	if (running)
	{
		uint32_t now = millis();
		uint32_t diff = now - last_send_time;
		if ((!send_num && (diff > SEND_INTERVAL)) ||
			 (send_num && (diff > SEND_DELAY)))
		{
			last_send_time = millis();

			updatePosition();

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


	// UI

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
		else if (c == '?')
		{
			usage();
		}
		else if (c == 'x')
		{
			if (running)
				stop_sim();
			else
				start_sim();
		}
		else if (c == 'y')
		{
			init_sim();
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
		else if (c == 'z')
		{
			display(0,"---- EXPERIMENT ----",0);
			proc_entry();
			nmeaExperiment();
			proc_leave();
		}
		else if (c == 'g')
		{
			going_to = !going_to;
			display(0,"GOING_TO(%d)",going_to);
			if (routing && !going_to)
			{
				route_name = "";
				display(0,"TURNING routing OFF",0);
			}
		}
		else if (c == 'r')
		{
			routing = !routing;
			display(0,"ROUTING(%d)",routing);
			if (routing && !going_to)
			{
				going_to = 1;
				display(0,"TURNING going_to ON",0);
			}
			closest_goto = CLOSEST_NONE;
		}
		else if (c == 'h' || c == 's' || c == 'w' || c == 'j' || c == 'p')
		{
			in_command = c;
		}
	}
}


// end of simulator.cpp
