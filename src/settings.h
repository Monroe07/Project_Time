
/*
   Hostname
*/
const char NAME[] = "";	// Name of Device On Network

/*
   NTP SERVERS
*/
static const char ntpServerName[] = "us.pool.ntp.org";
//static const char ntpServerName[] = "time.nist.gov";
//static const char ntpServerName[] = "time-a.timefreq.bldrdoc.gov";
//static const char ntpServerName[] = "time-b.timefreq.bldrdoc.gov";
//static const char ntpServerName[] = "time-c.timefreq.bldrdoc.gov";

/*
   Pin Assignments
*/
int CLK = 5;        // D1
int DIO = 4;        // D2
int adj = 13;       // D7
int configPin = 14; // D5

/*
 * Display Types
 */
// AdaFruit i2c Screen
 #define HT16K33

// DIY-MORE Screen
 //#define TM1637
