// Attempt to communicate with E80 via NMEA0183.
// Trying RS485 modules to communicate RS422 with E80 (2 differential pairs)
// The differential pairs in the Raymarine NMEA0183 cable are"
// These are confirmed wire colors from my old AIS->NMEA0183 cable
//		which I cut to perform these experiments.
//
//		RX-		green			Data TO the E80
//		RX+		white
// 		TX-		brown			Data FROM the E80
//		TX+		yellow
//
// List of sentences the E80 purports to send (at 4800 baud)
//		APB		Autopilot Sentence "B"
//		BWC		Bearing and Distance to Waypoint – Latitude, N/S, Longitude, E/W, UTC, Status
//		BWR		Bearing and Distance to Waypoint – Rhumb Line Latitude, N/S, Longitude, E/W, UTC, Status
//		DBT		Depth Below Transducer
//		DPT		Depth with optional transducer offset
//		GGA		Global Positioning System Fix Data. Time, Position and fix related data
//		GLL		Geographic Position – Latitude/Longitude
//		MTW		Water Temperature
//		MWV		Wind Speed and Angle
//		RMA		Recommended Minimum Navigation Information
//		RMB		Recommended Minimum Navigation Information
//		RMC		Recommended Minimum Navigation Information
//		RMD		Recommended Minimum Navigation Information
//		RTE		Routes
//		TTM		Tracked Target Message
//		VHW		Water Speed and Heading
//		VLW		Distance Traveled through Water
//		WPL		Waypoint Location
//		VTG		Track made good and speed over ground
//

// At first I tried the smaller RS485 modules, which required
// 		ModbusMaster library for the MAX485, and pulling the RE/DE
// 		pins LOW to receive, or HIGH to transmit.
//		No joy and pain in the ass.
//
// Second I switched to the larger RS485 module, and have *some* indication
//		it is sending data, but I have never gotten any on, or from, the E80
//		Module				ESP32
//			VCC				3.3v
//			GND				GND
//			RX				RX2 (GPIO16)
// 			TX				TX2 (GPIO17)
//
//		Module				E80 Wires
//			A				white or yellow
//			B				green or brown
//
//		No joy in very possible combination. TX and RX.
//		I also tried common ground from E80 to breadboard.
//
//---------------------------------------------------------------------
// RS232 adapter
//---------------------------------------------------------------------
// I note that the AIS was sending out on a DB9 connector, and many
// comments saying that you can 'single end' the RS422 signals and
// connect it as RS232.  So I have an RS232 to TTTL adapter, based
// on the MAX3232 chip. I removed the DB9 connector from it and
// added some 2.54mm screw terminal headers.  The headers are the
// top row of the female DB9 connector.  With the MPU header pins
// on the left, and the green terminal headers on the right.  T
// the pins, from top down are:
//
//			MPU						RS232
// 			GND						DCD
//			TXD						RXD			was AIS white; E80 white
//			RXD						TXD			E80 yellow
//			VCC						DTR
//									GND			was AIS green; E80 brown worked, trying GREEN
//
//	I *think* that the E80 is transmitting information
//  when in "AIS38400" mode, so the first thing I will
//	try is to see if I can receive information by connecting
//	the RS23w module to a red FT232RL module with 5V jumper
//  with only the E80 brown going to the RS232 GND and the
//  E80 yellow going to TXD.
//
//		  	RS232					FT232
//		  	GND						GND
//			TXD						TXD (switched on FTD232)
//			RXD	(data from E80)		RXD
//		  	VCC						VCC
//
// YAY!!! - I GET THE "$DUAIQ,TXS*3B" messages!
//		with console.pm at 38400 and E80 setup similarly.
//
// 	What about that one message I get at 4800 seemingly
//		when changing simulation mode on the e80?
//		I got "ÖÿÖÿÖÿÖÿÖÿÖÿÖÿÖÿÖÿÖÿÖÿÖÿ$VWVHW,,,284.6,M,,,,*23"
//		when leaving simulation mode!
//
//			"$VWVHW,,,284.6,M,,,,*23" indicates the wind speed and direction.
//			Specifically, it shows a wind direction of 284.6 degrees true and
//			a wind speed of 0 knots relative to the vessel (indicated by the
//			missing values in the second and third fields). The "M" indicates
//			that the wind speed is given in knots"
//
// So now I am wondering about TX and both.
// Is there a voltage difference between brown and green?
//		I presume there is.
//
// 2nd TEST - try using the GREEN as the (common ground)
// 	    and add the WHITE as the AIS had it.
//		No JOY (but I had console.pm at wrong baud rate)
// 3rd TEST - try using E80 12V ground as signal ground
//		black wire from power supply
//		WORKS!!! BEST SOLUTION
// OK, lets try to send some hand made messages from console.pm at 4800.
//		No joy.  Tried same at AIS 38400, no joy.
//		The E80 is 8 bits, no parity, one stop bit, same as console.pm
// Finally, after many tries to send NMEA0183 sentences TO the E80
//		from console.pm, I have never had any luck.
//
//-------------------------------------------------------------------
// SRS161 AIS Receiver
//-------------------------------------------------------------------
// Figuring I now had luck with the RS232 module, I decided to get
// the AIS receiver off of the boat and try hooking it up on the desk.
// Reworked desk power supply to more generic connectors, hooked up
// something for an antenna from many weird "wifi" connectors, hooked
// up the power supply to the AIS, and connected the AIS to the E80
// JUST AS IT WAS ON THE BOAT, with the green wire as the DB9 ground
// and the white wire as the DB9 pin 4 AIS TX, and ... nothing.
//
// Tried "sniffing" the AIS TX by hooking the RS232/console setup
// to the green/white pair as "RX" on the RS232 module. Nothing.
//
// Made 3 new DB9-like pin wires with the goal of just hooking the
// AIS up directly to the RS232 setup for two way coms from console.pm
// at 38400.  Nothing, though at some point the AIS starts echoing
// everything yo send to it.
//
// CONCLUSION: The AIS Receiver is munged and not being helpful.
//
//----------------------------------------------------------
// Thought about
//----------------------------------------------------------
// Trying to talk "seatalk" to the chart plotter.  Its a 12V
// single ended 9 bit, bi-directional protocol. However the E80
// appears to have distinct "Seatalk In" and "Seatalk Out"
// pins on the Seatalk Alarm connector.
//
//		pin1 is left of notch facing PLUG, and they go clockwise
//
//		pin1: GND						screen
//		pin2: +12V						red
//		pin3: Seatalk RX (E80 rx)		black
//		pin4: Seatalk TX (E80 tx)		yellow
//      pin5: Alarm out					white
//		pin6: not used "alarm return"	brown
//
// However, in the "Seatalk Junction Box" I only connected the
// yellow (TX) of the E80 to the (common) yellow of the "bus",
// so either (a) the E80 is using the yellow in "bi-directional mode",
// or (b) I have the concepts of RX and TX reversed on the E80,
// and the E80 in only RECEIVING Seatalk data from the "bus".
// FWIW, the "bus" includes the ST instruments, which *generally*
// appear to treat the seatalk as a one-way network, as each
// ST device has a "seatalk in" and "seatalk out" port.
//
// ** IT LOOKS LIKE THE TEENSY 4.0 is a better choice for a
// Seatalk device, as it natively supports 9 bits in UARTS,
// and we know how reliable the teensy hardware serial ports
// are. I think we would need to invert the 12V yellow signal
// and cut it down to 3.3V but I think I could accomplish that
// with transistors for the simple "listen" case.
