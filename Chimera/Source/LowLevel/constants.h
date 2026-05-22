//created by Mark O. Brown
#pragma once

#include "GeneralUtilityFunctions/my_str.h"
#include <string>
#include <vector>
#include <array>
#include <utility>

// running in safemode means that the program doesn't actually try to connect to physical devices. Generally, it will 
// follow the exact coding logic, but at the exact point where it would normally communicate with a device, it will 
// instead simply skip this step. It might generate example data where useful / necessary (e.g. after querying a
// camera system for a picture). It can be used to build and debug other aspects of the program, such as the gui, 
// coding logic, etc.
 
#define MASTER_COMPUTER

#ifdef MASTER_COMPUTER
	//constexpr bool DOFTDI_SAFEMODE = true;
	constexpr bool DDS_SAFEMODE = true;
	constexpr bool ANDOR_SAFEMODE = true;
	const std::pair<unsigned, unsigned> ANDOR_TRIGGER_LINE = std::make_pair(1 - 1, 5); // used for QtAndorWindow::abortCameraRun to give the last trigger and also for consistensy check
	//constexpr bool ANALOG_IN_SAFEMODE = true;
	#ifdef _DEBUG
		constexpr bool PYTHON_SAFEMODE = true;
	#else
		constexpr bool  PYTHON_SAFEMODE = true;
	#endif
	//constexpr bool DAQMX_SAFEMODE = true;
	//constexpr bool ANALOG_OUT_SAFEMODE = true;

	constexpr auto CODE_ROOT = "C:\\Users\\yb2\\Chimera-Control";

	const std::string PLOT_FILES_SAVE_LOCATION = str (CODE_ROOT) + "\\Plotting";
	const std::string DATA_ANALYSIS_CODE_LOCATION = "C:\\Users\\yb2\\Code\\Data_Analysis_Control\\";
	const std::string DEFAULT_SCRIPT_FOLDER_PATH = str (CODE_ROOT) + "\\Default-Scripts\\";
	const std::string ACTUAL_CODE_FOLDER_PATH = str (CODE_ROOT) + "\\Chimera\\";
	const std::string PROFILES_PATH = str (CODE_ROOT) + "\\Profiles\\";
	const std::string CONFIGURATION_PATH = str(CODE_ROOT) + "\\Configurations\\";

	//const std::string DATA_SAVE_LOCATION = "J:\\Data Repository\\New Data Repository\\";
	const std::string DATA_SAVE_LOCATION = "B:\\Data\\Chimera_Data";

	const std::string MUSIC_LOCATION = str (CODE_ROOT) + "\\Final Fantasy VII - Victory Fanfare [HQ].mp3";
	const std::string FUNCTIONS_FOLDER_LOCATION = str (CODE_ROOT) + "\\Functions\\";
	const std::string MASTER_CONFIGURATION_FILE_ADDRESS (str (CODE_ROOT) + "\\Master-Configuration.txt");
	const std::string CAMERA_CAL_ROUTINE_ADDRESS = PROFILES_PATH + "Hotkey Experiments\\Camera";
	const std::string MOT_ROUTINES_ADDRESS = PROFILES_PATH + "Hotkey Experiments\\MOT";
	// location where wave data can be outputted for analyzing with another computer.


	// Zynq realated/ RIO FPGA related -- this version used a NI PCIE 7820. To migrate to ZYNQ see Zhenpu's original Repo
	const double ZYNQ_DEADTIME = 0.1; // give zynq 0.1ms to avoid sending zero at t=0 for ttl
	
	const bool ZYNQ_SAFEMODE = true;
	const bool RIO_SAFEMODE = false;
	const auto ZYNQ_ADDRESS = "10.10.0.2";
	const auto ZYNQ_PORT = "8080";
	const int ZYNQ_MAX_BUFF = 64;
	const int DIO_LEN_BYTE_BUF = 28;
	const int DAC_LEN_BYTE_BUF = 44;
	const int DDS_LEN_BYTE_BUF = 46;

	const double DIO_TIME_RESOLUTION = 2.5e-5; // in ms, 25ns
	const double DAC_TIME_RESOLUTION = 0.003125; // in ms, 3.125us for 320kHz update rate
	//const int DAC_RAMP_MAX_PTS = 0xffff; // 65535 ???
	const double DDS_TIME_RESOLUTION = 1.6; // in ms
	const double DDS_MAX_AMP = 1.25; // in mW
	const std::pair<unsigned, unsigned> DIO_REWIND = std::make_pair(8 - 1, 7); // used for the long time run rewind, see DoCore::checkLongTimeRun. THIS IS NOT USED FOR NOW
	const std::array<unsigned short, 2> DAC_REWIND = { 15, 31 }; // used for the long time run rewind, see AoCore::formatDacForFPGA

	//OffsetLock 
	const std::vector<bool> OFFSETLOCK_SAFEMODE = std::vector<bool>{ true,true,true };
	const std::vector<std::string> OL_COM_PORT = { "COM3", "COM7", "COM12"};
	const double OL_TIME_RESOLUTION = 0.02; //in ms
	const std::vector<std::pair<unsigned, unsigned>> OL_TRIGGER_LINE
		= { std::make_pair(3 - 1,6),std::make_pair(3 - 1,7),
			std::make_pair(4 - 1,6),std::make_pair(4 - 1,7),
			std::make_pair(5 - 1,6) }; /*the first is the label on the box minus 1, has minus'd 1 explicitly */
	const double OL_TRIGGER_TIME = 0.01; //in ms i.e. 50us

	//#define DDS_FPGA_ADDRESS "FT1I6IBSB"; //Device Serial: FT1I6IBS, Use FT1I6IBSB in C++ to select Channel B

	//ArbGens
    const bool UWAVE_SAFEMODE = true;
    const bool UWAVE_SAFEMODE_SIG = false;
    const int numArbGen = 2;	// Siglent0 (192.168.105.52) and Siglent1 (192.168.105.54)
    const std::array<std::string, 2> UWAVE_SIGLENT_ADDRESSES = {
    "TCPIP0::192.168.105.52::inst0::INSTR",
    "TCPIP0::192.168.105.53::inst0::INSTR"
};
    const std::pair<unsigned, unsigned> UWAVE_SIGLENT_TRIGGER_LINE = std::make_pair(7 - 1, 1); /*the first is the label on the box minus 1, has minus'd 1 explicitly */
    const std::string RAMP_LOCATION = str(CODE_ROOT) + "\\Ramp_Files\\";

	//Analog in 
	const bool AI_SAFEMODE = true;
	const std::string AI_SOCKET_ADDRESS = "10.10.0.10";
	const std::string AI_SOCKET_PORT = "80";

	//Wieserlabs DDS
	const bool WIESERLABS_SAFEMODE = true;
	const std::string WIESERLABS_IPADDRESS = "192.168.105.5";
	const int WIESERLABS_IPPORT = 26000;
	// Per-channel hardware triggers on FlexDDS-NG: both channels use BNC_IN_A
	const std::pair<unsigned, unsigned> WIESERLABS_CH0_TRIGGER_LINE = std::make_pair(1, 0); /*BNC_IN_A: Row 1, Column 0*/
	const std::pair<unsigned, unsigned> WIESERLABS_CH1_TRIGGER_LINE = std::make_pair(1, 0); /*BNC_IN_A: Row 1, Column 0*/
	const std::array<bool, 6> SLOT_CONNECTED = { true, false, false, false, false, false };

	//Mako camera
	const unsigned MAKO_NUMBER = 4;
	const std::array<bool, MAKO_NUMBER> MAKO_SAFEMODE = { false,false,true,true};
	const std::array<std::string, MAKO_NUMBER> MAKO_DELIMS = { "MAKO1_CAM"/*MOT G125*/, "MAKO2_CAM"/*MOT G319*/, "MAKO3_CAM"/*420 MON*/, "MAKO4_CAM"/*1013 MON*/};
	const std::array<std::string, MAKO_NUMBER> MAKO_IPADDRS = { "192.168.105.6", "192.168.105.2","10.10.0.12","10.10.0.11" };
	const std::vector<std::pair<unsigned, unsigned>> MAKO_TRIGGER_LINE
		= {std::make_pair(3 - 1,5), std::make_pair(1 - 1,2),std::make_pair(5 - 1,3),std::make_pair(5 - 1,7) };
	/*the first is the label on the box minus 1, has minus'd 1 explicitly and this is not used in the code, just a reminder*/

	//GIGAMOOG
	const bool GIGAMOOG_SAFEMODE = true;
	const std::string GIGAMOOG_IPADDRESS = "192.168.7.179";
	const int GIGAMOOG_IPPORT = 804;
	//const std::string GIGAMOOG_PORT = "COM5";
	//const int GIGAMOOG_BAUDRATE = 115200;
	const double GM_TRIGGER_TIME = 0.0005; //in ms i.e. 0.5us
	const std::vector<std::pair<unsigned, unsigned>> GM_TRIGGER_LINE
		= { std::make_pair(1 - 1,7),std::make_pair(8 - 1,1) }; //load and move
	/*the first is the label on the box minus 1, has minus'd 1 explicitly and this is not used in the code, just a reminder*/

	//Microwave Windfreak
	const bool MICROWAVE_SAFEMODE = true;
	const std::string MICROWAVE_PORT = "COM9";
	const std::pair<unsigned, unsigned> MW_TRIGGER_LINE = std::make_pair(4 - 1, 2); /*the first is the label on the box minus 1, has minus'd 1 explicitly */

	//PicoScrew
	const bool PICOSCREW_SAFEMODE = true;
	const std::string PICOSCREW_KEY = "8742 107036";
	const unsigned PICOSCREW_NUM = 4;
	const std::array<bool, PICOSCREW_NUM> PICOSCREW_CONNECTED = { true,true,false,false };

	//20-bit static DAC
	const bool STATICAO_SAFEMODE = true;
	const std::string STATICAO_IPADDRESS = "192.168.7.165";
	const int STATICAO_IPPORT = 804;

	//static PLL
	const unsigned int STATICDDS_NUMBER = 4; // Set to the number of StaticDDS systems you want
	const bool STATICDDS_SAFEMODE = false;
	const std::array<std::string, STATICDDS_NUMBER> STATICDDS_PORT = {"COM4", "COM5", "COM6", "COM8" };
	const std::array<unsigned int, STATICDDS_NUMBER> STATICDDS_BAUDRATE = {9600, 9600, 9600, 9600};

	//Elliptec rotation stage
	const bool ELLIPTEC_SAFEMODE = true;
	const std::string ELLIPTEC_PORT = "COM14";

	//Temperature Monitor
	const bool TEMPMON_SAFEMODE = false;
	// We will show two temperatures: science table and laser table
	const unsigned TEMPMON_NUMBER = 2;
	const std::array<std::string, TEMPMON_NUMBER> TEMPMON_ID{ 
		"yb2-science-table", "yb2-laser-table" };
	// Legacy InfluxQL syntax placeholders (unused in Flux path)
	const std::array<std::string, TEMPMON_NUMBER> TEMPMON_SYNTAX{ 
		"", "" };

	// InfluxDB 2.x (Flux) configuration
	const std::string INFLUX2_URL = "http://192.168.105.4:8086";
	const std::string INFLUX2_ORG = "yb2";
	const std::string INFLUX2_BUCKET = "lab-vitals";
	const std::string INFLUX2_TOKEN = "TZ3vx6Bo41wGIqK-Yyf7ZBBWq4NSXyREPtWI7cEKrWAXOP-l9HIzOwGJtDsTyQZTKEW0iz3oHinwhIU1WFyMiQ==";
	const std::string INFLUX2_MEASUREMENT = "ubibot";
	const std::string INFLUX2_FIELD = "temperature";
	const std::string INFLUX2_TAG_KEY = "channel";

#endif
/// Random other Constants
constexpr double PI = 3.14159265358979323846264338327950288;

#ifdef CHANGES_COMPUTER
//constexpr bool DOFTDI_SAFEMODE = true;
constexpr bool DDS_SAFEMODE = true;
constexpr bool ANDOR_SAFEMODE = true;
const std::pair<unsigned, unsigned> ANDOR_TRIGGER_LINE = std::make_pair(1 - 1, 5); // used for QtAndorWindow::abortCameraRun to give the last trigger and also for consistensy check
//constexpr bool ANALOG_IN_SAFEMODE = true;
#ifdef _DEBUG
constexpr bool PYTHON_SAFEMODE = true;
#else
constexpr bool  PYTHON_SAFEMODE = true;
#endif
//constexpr bool DAQMX_SAFEMODE = true;
//constexpr bool ANALOG_OUT_SAFEMODE = true;

constexpr auto CODE_ROOT = "C:\\Users\\ronak\\desktop\\Chimera-Control";

const std::string PLOT_FILES_SAVE_LOCATION = str(CODE_ROOT) + "\\Plotting";
const std::string DATA_ANALYSIS_CODE_LOCATION = "C:\\Users\\ronak\\Code\\Data_Analysis_Control\\";
const std::string DEFAULT_SCRIPT_FOLDER_PATH = str(CODE_ROOT) + "\\Default-Scripts\\";
const std::string ACTUAL_CODE_FOLDER_PATH = str(CODE_ROOT) + "\\Chimera\\";
const std::string PROFILES_PATH = str(CODE_ROOT) + "\\Profiles\\";
const std::string CONFIGURATION_PATH = str(CODE_ROOT) + "\\Configurations\\";

//const std::string DATA_SAVE_LOCATION = "J:\\Data Repository\\New Data Repository\\";
const std::string DATA_SAVE_LOCATION = "C:\\Users\\ronak\\Desktop\\chimera_data";

const std::string MUSIC_LOCATION = str(CODE_ROOT) + "\\Final Fantasy VII - Victory Fanfare [HQ].mp3";
const std::string FUNCTIONS_FOLDER_LOCATION = str(CODE_ROOT) + "\\Functions\\";
const std::string MASTER_CONFIGURATION_FILE_ADDRESS(str(CODE_ROOT) + "\\Master-Configuration.txt");
const std::string CAMERA_CAL_ROUTINE_ADDRESS = PROFILES_PATH + "Hotkey Experiments\\Camera";
const std::string MOT_ROUTINES_ADDRESS = PROFILES_PATH + "Hotkey Experiments\\MOT";
// location where wave data can be outputted for analyzing with another computer.


// Zynq realated/ RIO FPGA related -- this version used a NI PCIE 7820. To migrate to ZYNQ see Zhenpu's original Repo
const double ZYNQ_DEADTIME = 0.1; // give zynq 0.1ms to avoid sending zero at t=0 for ttl

const bool ZYNQ_SAFEMODE = true;
const bool RIO_SAFEMODE = true;
const auto ZYNQ_ADDRESS = "10.10.0.2";
const auto ZYNQ_PORT = "8080";
const int ZYNQ_MAX_BUFF = 64;
const int DIO_LEN_BYTE_BUF = 28;
const int DAC_LEN_BYTE_BUF = 44;
const int DDS_LEN_BYTE_BUF = 46;

const double DIO_TIME_RESOLUTION = 2.5e-5; // in ms, 25ns
const double DAC_TIME_RESOLUTION = 0.003125; // in ms, 3.125us for 320kHz update rate
//const int DAC_RAMP_MAX_PTS = 0xffff; // 65535 ???
const double DDS_TIME_RESOLUTION = 1.6; // in ms
const double DDS_MAX_AMP = 1.25; // in mW
const std::pair<unsigned, unsigned> DIO_REWIND = std::make_pair(8 - 1, 7); // used for the long time run rewind, see DoCore::checkLongTimeRun. THIS IS NOT USED FOR NOW
const std::array<unsigned short, 2> DAC_REWIND = { 15, 31 }; // used for the long time run rewind, see AoCore::formatDacForFPGA

//OffsetLock 
const std::vector<bool> OFFSETLOCK_SAFEMODE = std::vector<bool>{ true,true,true };
const std::vector<std::string> OL_COM_PORT = { "COM3", "COM7", "COM12" };
const double OL_TIME_RESOLUTION = 0.02; //in ms
const std::vector<std::pair<unsigned, unsigned>> OL_TRIGGER_LINE
= { std::make_pair(3 - 1,6),std::make_pair(3 - 1,7),
	std::make_pair(4 - 1,6),std::make_pair(4 - 1,7),
	std::make_pair(5 - 1,6) }; /*the first is the label on the box minus 1, has minus'd 1 explicitly */
const double OL_TRIGGER_TIME = 0.01; //in ms i.e. 50us

//#define DDS_FPGA_ADDRESS "FT1I6IBSB"; //Device Serial: FT1I6IBS, Use FT1I6IBSB in C++ to select Channel B

//ArbGens
const bool UWAVE_SAFEMODE = true;
const bool UWAVE_SAFEMODE_SIG = true;
const int numArbGen = 2;	// Siglent0 (192.168.105.52) and Siglent1 (192.168.105.54)
const std::array<std::string, 2> UWAVE_SIGLENT_ADDRESSES = {
    "TCPIP0::192.168.105.52::inst0::INSTR",
    "TCPIP0::192.168.105.54::inst0::INSTR"
};
const std::pair<unsigned, unsigned> UWAVE_SIGLENT_TRIGGER_LINE = std::make_pair(7 - 1, 1); /*the first is the label on the box minus 1, has minus'd 1 explicitly */
const std::string RAMP_LOCATION = str(CODE_ROOT) + "\\Ramp_Files\\";

//Analog in 
const bool AI_SAFEMODE = true;
const std::string AI_SOCKET_ADDRESS = "10.10.0.10";
const std::string AI_SOCKET_PORT = "80";

//Wieserlabs DDS
const bool WIESERLABS_SAFEMODE = true;
const std::string WIESERLABS_IPADDRESS = "192.168.105.5";
const int WIESERLABS_IPPORT = 26000;
// Per-channel hardware triggers on FlexDDS-NG: both channels use BNC_IN_A
const std::pair<unsigned, unsigned> WIESERLABS_CH0_TRIGGER_LINE = std::make_pair(1, 0); /*BNC_IN_A: Row 1, Column 0*/
const std::pair<unsigned, unsigned> WIESERLABS_CH1_TRIGGER_LINE = std::make_pair(1, 0); /*BNC_IN_A: Row 1, Column 0*/
const std::array<bool, 6> SLOT_CONNECTED = { true, false, false, false, false, false };

//Mako camera
const unsigned MAKO_NUMBER = 4;
const std::array<bool, MAKO_NUMBER> MAKO_SAFEMODE = { true,true,true,true };
const std::array<std::string, MAKO_NUMBER> MAKO_DELIMS = { "MAKO1_CAM"/*MOT G125*/, "MAKO2_CAM"/*MOT G319*/, "MAKO3_CAM"/*420 MON*/, "MAKO4_CAM"/*1013 MON*/ };
const std::array<std::string, MAKO_NUMBER> MAKO_IPADDRS = { "192.168.105.6", "192.168.105.2","10.10.0.12","10.10.0.11" };
const std::vector<std::pair<unsigned, unsigned>> MAKO_TRIGGER_LINE
= { std::make_pair(1 - 1,2),std::make_pair(3 - 1,5),std::make_pair(5 - 1,3),std::make_pair(5 - 1,7) };
/*the first is the label on the box minus 1, has minus'd 1 explicitly and this is not used in the code, just a reminder*/

//GIGAMOOG
const bool GIGAMOOG_SAFEMODE = true;
const std::string GIGAMOOG_IPADDRESS = "192.168.7.179";
const int GIGAMOOG_IPPORT = 804;
//const std::string GIGAMOOG_PORT = "COM5";
//const int GIGAMOOG_BAUDRATE = 115200;
const double GM_TRIGGER_TIME = 0.0005; //in ms i.e. 0.5us
const std::vector<std::pair<unsigned, unsigned>> GM_TRIGGER_LINE
= { std::make_pair(1 - 1,7),std::make_pair(8 - 1,1) }; //load and move
/*the first is the label on the box minus 1, has minus'd 1 explicitly and this is not used in the code, just a reminder*/

//Microwave Windfreak
const bool MICROWAVE_SAFEMODE = true;
const std::string MICROWAVE_PORT = "COM9";
const std::pair<unsigned, unsigned> MW_TRIGGER_LINE = std::make_pair(4 - 1, 2); /*the first is the label on the box minus 1, has minus'd 1 explicitly */

//PicoScrew
const bool PICOSCREW_SAFEMODE = true;
const std::string PICOSCREW_KEY = "8742 107036";
const unsigned PICOSCREW_NUM = 4;
const std::array<bool, PICOSCREW_NUM> PICOSCREW_CONNECTED = { true,true,false,false };

//20-bit static DAC
const bool STATICAO_SAFEMODE = true;
const std::string STATICAO_IPADDRESS = "192.168.7.165";
const int STATICAO_IPPORT = 804;

//static PLL
const unsigned int STATICDDS_NUMBER = 4; // Set to the number of StaticDDS systems you want
const bool STATICDDS_SAFEMODE = true;
const std::array<std::string, STATICDDS_NUMBER> STATICDDS_PORT = { "COM4", "COM5", "COM6", "COM8" };
const std::array<unsigned int, STATICDDS_NUMBER> STATICDDS_BAUDRATE = { 9600, 9600, 9600, 9600 };

//Elliptec rotation stage
const bool ELLIPTEC_SAFEMODE = true;
const std::string ELLIPTEC_PORT = "COM14";

//Temperature Monitor
const bool TEMPMON_SAFEMODE = true;
// We will show two temperatures: science table and laser table
const unsigned TEMPMON_NUMBER = 2;
const std::array<std::string, TEMPMON_NUMBER> TEMPMON_ID{
	"yb2-science-table", "yb2-laser-table" };
// Legacy InfluxQL syntax placeholders (unused in Flux path)
const std::array<std::string, TEMPMON_NUMBER> TEMPMON_SYNTAX{
	"", "" };

// InfluxDB 2.x (Flux) configuration
const std::string INFLUX2_URL = "http://192.168.105.4:8086";
const std::string INFLUX2_ORG = "yb2";
const std::string INFLUX2_BUCKET = "lab-vitals";
const std::string INFLUX2_TOKEN = "TZ3vx6Bo41wGIqK-Yyf7ZBBWq4NSXyREPtWI7cEKrWAXOP-l9HIzOwGJtDsTyQZTKEW0iz3oHinwhIU1WFyMiQ==";
const std::string INFLUX2_MEASUREMENT = "ubibot";
const std::string INFLUX2_FIELD = "temperature";
const std::string INFLUX2_TAG_KEY = "channel";

#endif


