//------------------------------------------
// nmeaGPS.cpp
//------------------------------------------
// A gps emulator based on https://github.com/luk-kop/nmea-gps-emulator
//
//		unit's position;
//		unit's speed;
//		unit's course;
//
// Output Sentences
//
//		GPGGA - Global Positioning System Fix Data
//		GPGLL - Position data: position fix, time of position fix, and status
//		GPRMC - Recommended minimum specific GPS/Transit data
//		GPGSA - GPS DOP and active satellites
//		GPGSV - GPS Satellites in view
//		GPHDT - True Heading
//		GPVTG - Track made good and ground speed
//		GPZDA - Date & Time
//
// Output Example
//
//		$GPGGA,173124.00,5430.000,N,01921.029,E,1,09,0.92,15.2,M,32.5,M,,*6C
//		$GPGSA,A,3,22,11,27,01,03,02,10,21,19,,,,1.56,0.92,1.25*02
//		$GPGSV,4,1,15,26,25,138,53,16,25,091,67,01,51,238,77,02,45,085,41*79
//		$GPGSV,4,2,15,03,38,312,01,30,68,187,37,11,22,049,44,09,67,076,71*77
//		$GPGSV,4,3,15,10,14,177,12,19,86,235,37,21,84,343,95,22,77,040,66*79
//		$GPGSV,4,4,15,08,50,177,60,06,81,336,46,27,63,209,83*4C
//		$GPGLL,5430.000,N,01921.029,E,173124.000,A,A*59
//		$GPRMC,173124.000,A,5430.000,N,01921.029,E,10.500,90.0,051121,,,A*65
//		$GPHDT,90.0,T*0C
//		$GPVTG,90.0,T,,M,10.5,N,19.4,K*51
//		$GPZDA,173124.000,05,11,2021,0,0*50