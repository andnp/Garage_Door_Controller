/*
 * Test.ino
 *
 * Created: 8/5/2014 1:08:23 PM
 * Author: Andy
 */ 
#include <Arduino.h>

#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192,168,1,178);
IPAddress subnet(255,255,255,0);
IPAddress gateway(192,168,1,254);

IPAddress mailServer(207,58,147,66);
IPAddress timeServer(129,6,15,30);
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

String HTTP_req;
int Garage_status = 1;

EthernetUDP Udp;
EthernetServer server(80);  // create a server at port 80

long Relay_on_time = INFINITY;
long Last_relay_on = 0;

long time;
long checkTime;
const long checkInterval = 300;
long lastMillis;

void setup()
{
    Ethernet.begin(mac, ip, subnet, gateway);  // initialize Ethernet device
    server.begin();           // start to listen for clients
    Serial.begin(9600);       // for debugging
	
	pinMode(7, OUTPUT);
	digitalWrite(7,HIGH);
	Udp.begin(2390);
	updateTime();
}

void loop()
{
    EthernetClient client = server.available();  // try to get client
	//Serial.println(getEpochTime());
    if (client) {  // got client?
       sendFile(client);
    } // end if (client)
	
	if( getEpochTime() >= checkTime ){
		updateTime();
	}
	if(getEpochTime() > Relay_on_time){
		ToggleRelay();
		Serial.println("hello");
	}
}

void sendFile(EthernetClient client){
	 boolean currentLineIsBlank = true;
	 while (client.connected()) {
		 if (client.available()) {   // client data available to read
			 char c = client.read(); // read 1 byte (character) from client
			 HTTP_req += c;
			 // last line of client request is blank and ends with \n
			 // respond to client only after last line received
			 if (c == '\n' && currentLineIsBlank) {
				 Serial.println("have client");
				 // send a standard http response header
				 client.println("HTTP/1.1 200 OK");
				 client.println("Content-Type: text/html");
				 client.println("Connection: close");
				 client.println();
				 client.println(webPage());
				 break;
			 }
			 // every line of text received from the client ends with \r\n
			 if (c == '\n') {
				 // last character on line of received text
				 // starting new line with next character read
				 currentLineIsBlank = true;
			 }
			 else if (c != '\r') {
				 // a text character was received from client
				 currentLineIsBlank = false;
			 }
		 } // end if (client.available())
	 } // end while (client.connected())
	 delay(1);      // give the web browser time to receive the data
	 client.stop(); // close the connection
}

void updateTime(){
	time = readTime();
	checkTime = time + checkInterval;
	lastMillis = millis();
}

long readTime(){
	sendNTPpacket(timeServer); // send an NTP packet to a time server

	// wait to see if a reply is available
	delay(1000);
	if ( Udp.parsePacket() ) {
		// We've received a packet, read the data from it
		Udp.read(packetBuffer,NTP_PACKET_SIZE);  // read the packet into the buffer

		//the timestamp starts at byte 40 of the received packet and is four bytes,
		// or two words, long. First, esxtract the two words:

		unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
		unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
		// combine the four bytes (two words) into a long integer
		// this is NTP time (seconds since Jan 1 1900):
		unsigned long secsSince1900 = highWord << 16 | lowWord;
		Serial.println(secsSince1900);
		// Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
		const unsigned long seventyYears = 2208988800UL;
		// subtract seventy years:
		unsigned long epoch = secsSince1900 - seventyYears;
		return epoch;
	}
}
long getEpochTime(){
	return (time + ((millis() - lastMillis) / 1000));
}

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address){
	// set all bytes in the buffer to 0
	memset(packetBuffer, 0, NTP_PACKET_SIZE);
	// Initialize values needed to form NTP request
	// (see URL above for details on the packets)
	packetBuffer[0] = 0b11100011;   // LI, Version, Mode
	packetBuffer[1] = 0;     // Stratum, or type of clock
	packetBuffer[2] = 6;     // Polling Interval
	packetBuffer[3] = 0xEC;  // Peer Clock Precision
	// 8 bytes of zero for Root Delay & Root Dispersion
	packetBuffer[12]  = 49;
	packetBuffer[13]  = 0x4E;
	packetBuffer[14]  = 49;
	packetBuffer[15]  = 52;

	// all NTP fields have been given values, now
	// you can send a packet requesting a timestamp:
	Udp.beginPacket(address, 123); //NTP requests are to port 123
	Udp.write(packetBuffer,NTP_PACKET_SIZE);
	Udp.endPacket();
}

void sendEmail(){
	EthernetClient mailClient;
	if(mailClient.connect(mailServer,2525)){
		Serial.println("connected");
		mailClient.println("EHLO");
		mailClient.println("AUTH LOGIN");
		mailClient.println("c291bmRndXlhbmR5cEBnbWFpbC5jb20=");
		mailClient.println("YW5kbnA5NzI5NjU=");
		mailClient.println("MAIL FROM:<soundguyandyp@gmail.com>");
		mailClient.println("RCPT TO:<tpatterson@selcoeng.com>");
		mailClient.println("DATA");
		mailClient.println("from:soundguyandyp@gmail.com");
		mailClient.println("to:tpatterson@selcoeng.com");
		mailClient.println("SUBJECT: Automated Garage");
		mailClient.println();
		mailClient.println("The garage door is open");
		mailClient.println("please visit 192.168.1.178 to close");
		mailClient.println(".");
		mailClient.println(".");
		mailClient.println("QUIT");
		} else {
		Serial.println("failed");
	}
}

String webPage(){
	String output = "";
	output += "<!DOCTYPE html>";
	output += "<html>";
	output += "<head>";
	output += "<title>Garage Module</title>";
	output += "</head>";
	output += "<body>";
	output += "<h1>Garage Door Opener</h1>";
	output += "<p>Press to open/close garage door.</p>";
	output += "<form method='get'>";
	output += "<input type='checkbox' name='LED2' value='Garage Door' onclick='submit();'>";
	ProcessCheckbox();
	output += "</form>";
	output += "</body>";
	output += "</html>";
	Serial.print(HTTP_req);
	HTTP_req = "";
	return output;
}

void ProcessCheckbox(){
	if(HTTP_req.indexOf("LED2=Garage+Door") > -1){
		ToggleRelay();
	}
}

void ToggleRelay(){
	if (Garage_status){
		Garage_status = 0;
		} else {
		Garage_status = 1;
	}
	if(Garage_status == 0 && getEpochTime() > Last_relay_on){
		digitalWrite(7, LOW);
		Relay_on_time = getEpochTime() + 1;
		Last_relay_on = getEpochTime() + 5;
		Serial.println(Relay_on_time);
		} else {
		digitalWrite(7, HIGH);
		Relay_on_time = INFINITY;
	}
}
