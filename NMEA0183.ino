//---------------------------------------------
// NMEA0183.ino
//---------------------------------------------


#include <myDebug.h>
#include "nmea0183.h"


#define WITH_SIMULATOR	1
	// otherwise, send test sequences


#if WITH_SIMULATOR
	extern void simulator_run();
#else
	int seq_num = 0;
	const char *gps_sequence[] = {
		"$GPGGA,092750.000,5321.6802,N,00630.3372,W,1,8,1.03,61.7,M,55.2,M,,*76",
		"$GPGSA,A,3,10,07,05,02,29,04,08,13,,,,,1.72,1.03,1.38*0A",
		"$GPGSV,3,1,11,10,63,137,17,07,61,098,15,05,59,290,20,08,54,157,30*70",
		"$GPGSV,3,2,11,02,39,223,19,13,28,070,17,26,23,252,,04,14,186,14*79",
		"$GPGSV,3,3,11,29,09,301,24,16,09,020,,36,,,*76",
		"$GPRMC,092750.000,A,5321.6802,N,00630.3372,W,0.02,31.66,280511,,,A*43",
		"$GPGGA,092751.000,5321.6802,N,00630.3371,W,1,8,1.03,61.7,M,55.3,M,,*75",
		"$GPGSA,A,3,10,07,05,02,29,04,08,13,,,,,1.72,1.03,1.38*0A",
		"$GPGSV,3,1,11,10,63,137,17,07,61,098,15,05,59,290,20,08,54,157,30*70",
		"$GPGSV,3,2,11,02,39,223,16,13,28,070,17,26,23,252,,04,14,186,15*77",
		"$GPGSV,3,3,11,29,09,301,24,16,09,020,,36,,,*76",
		"$GPRMC,092751.000,A,5321.6802,N,00630.3371,W,0.06,31.66,280511,,,A*45",
		0
	};
#endif


void setup()
{
	Serial.begin(921600);
	delay(1000);
	Serial.println("WTF");
	display(0,"NMEA0183 setup() started",0);

	NMEA_SERIAL.begin(38400, SERIAL_8N1, RXD2, TXD2);

	display(0,"NMEA0183 setup() completed",0);
}


void loop()
{
	#if WITH_SIMULATOR
		simulator_run();
	#else
		#if 1	// send next static test sequence
			uint32_t now = millis();
			static uint32_t last_send = 0;
			if (now - last_send > 500)
			{
				last_send = now;
				// display(0,"seq_num=%d",seq_num);

				const char *seq = gps_sequence[seq_num++];
				if (!seq)
				{
					// display(0,"done at seq_num=%d",seq_num);
					seq_num = 0;
				}
				else
				{
					display(0,"%-4d --> %s",seq_num,seq);
					NMEA_SERIAL.println(seq);
				}
			}
		#endif
	#endif
	

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
