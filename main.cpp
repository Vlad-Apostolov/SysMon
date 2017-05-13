/*
 * main.cpp
 *
 *  Created on: 27/02/2017
 *      Author: vlad
 */

#include <signal.h>
#include <map>
#include <boost/thread.hpp>
#include <boost/program_options.hpp>
#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/filesystem.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <iostream>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <unistd.h>
#include <linux/reboot.h>
#include <sys/reboot.h>
#include <wiringPi.h>

#include "simpleLogger.h"
#include "ping.h"
#include "SysMon.h"
#include "Xively.h"

using namespace std;
using namespace boost::program_options;
using namespace boost::iostreams;
using namespace boost::filesystem;

// RtiProxyClient logging control variables
static bool consoleLogging = true;			// when set to true, enables logging to console
static bool shutdownRPi = false;			// when set to true, shut down Raspberry Pi
static string logLevel = "info";			// RtiProxyClient logging threshold level: trace|debug|info|warning|error|fatal
static string logFileName = "";				// non empty string enables logging into a file
static string pingServerName = "";
static time_t webConnectionCheckTime;
static time_t routerPowerOffTime;
static int rpiSleepTime;
static int spiSleepTime;
static string PvoutputApikey;
static string xivelyAccountId;
static string xivelyDeviceId;
static string xivelyPassword;
static int PvoutputSystemId;
static bool relay1 = false;
static bool relay2 = false;
static bool relay3 = false;
static bool relay4 = false;
static bool relay5 = false;
static bool relay6 = false;
static bool relay7 = false;
static bool relay8 = false;


static shared_ptr<boost::asio::io_service> processingScheduler;	// processing scheduler

static void stopProcessing(int dummy)
{
	LOG_TRACE << __PRETTY_FUNCTION__;
	processingScheduler->stop();
}

static void configureLogger()
{
	using namespace boost::log::trivial;
	const char* logFile = logFileName.empty() ? NULL : logFileName.c_str();
	severity_level level;

	if (logLevel == "trace")
		level = trace;
	else if (logLevel == "debug")
		level = debug;
	else if (logLevel == "info")
		level = info;
	else if (logLevel == "warning")
		level = warning;
	else if (logLevel == "error")
		level = boost::log::trivial::error;
	else if (logLevel == "fatal")
		level = fatal;
	else {
		cout << "Unexpected logging level!" << endl;
		exit(EXIT_FAILURE);
	}

	configureLogger(level, consoleLogging, logFile);
}

static void parseCommandLine(int argc, char *argv[])
{
	options_description optionsDescription{"Options"};
	variables_map variablesMap;
	try	{
		optionsDescription.add_options()
			("help,h", "Help screen")
			("logLevel,l", value<string>(&logLevel)->default_value("info"), "Logging severity threshold: trace|debug|info|warning|error|fatal")
			("consoleLog,c", "Enable console logging")
			("shutdownRPi", value<bool>(&shutdownRPi)->default_value(false), "Shutdown Raspberry Pi")
			("logFileName,f", value<string>(&logFileName)->default_value(""), "File name for logging")
			("pingServerName,p", value<string>(&pingServerName)->default_value("google.com"), "Server name to ping")
			("webConnectionCheckTime,w", value<time_t>(&webConnectionCheckTime)->default_value(60), "Max number of seconds to check for connection to Internet")
			("routerPowerOffTime,r", value<time_t>(&routerPowerOffTime)->default_value(60), "Number of seconds to keep the router power off")
			("rpiSleepTime,s", value<int>(&rpiSleepTime)->default_value(30), "Number of minutes for Raspberry Pi to sleep after shutdown")
			("spiSleepTime", value<int>(&spiSleepTime)->default_value(5), "Number of minutes for Sleepy Pi to sleep")
			("PvoutputApikey,a", value<string>(&PvoutputApikey), "X-Pvoutput-Apikey")
			("PvoutputSystemId,i", value<int>(&PvoutputSystemId), "X-Pvoutput-SystemId")
			("xivelyAccountId,", value<string>(&xivelyAccountId), "xivelyAccountId")
			("xivelyDeviceId,", value<string>(&xivelyDeviceId), "xivelyDeviceId")
			("xivelyPassword,", value<string>(&xivelyPassword), "xivelyPassword")
			("relay1", value<bool>(&relay1)->default_value(false), "Turn relay 1 on/off")
			("relay2", value<bool>(&relay2)->default_value(false), "Turn relay 2 on/off")
			("relay3", value<bool>(&relay3)->default_value(false), "Turn relay 3 on/off")
			("relay4", value<bool>(&relay4)->default_value(false), "Turn relay 4 on/off")
			("relay5", value<bool>(&relay5)->default_value(false), "Turn relay 5 on/off")
			("relay6", value<bool>(&relay6)->default_value(false), "Turn relay 6 on/off")
			("relay7", value<bool>(&relay7)->default_value(false), "Turn relay 7 on/off")
			("relay8", value<bool>(&relay8)->default_value(false), "Turn relay 8 on/off")
			("config", value<std::string>(), "Config file");

		command_line_parser parser{argc, argv};
		parser.options(optionsDescription).allow_unregistered();
		parsed_options parsedOptions = parser.run();
	    store(parsedOptions, variablesMap);
		if (variablesMap.count("help")) {
			cout << optionsDescription << endl;
			exit(EXIT_SUCCESS);
		}
		if (variablesMap.count("consoleLog"))
			consoleLogging = true;
		if (variablesMap.count("shutdownRPi"))
			shutdownRPi = true;
		if (variablesMap.count("config")) {
			std::ifstream configStream {variablesMap["config"].as<std::string>().c_str()};
			if (configStream) {
				store(parse_config_file(configStream, optionsDescription), variablesMap);
				configStream.close();
			}
		}
		notify(variablesMap);
	} catch (const boost::program_options::required_option& e) {
        if (variablesMap.count("help")) {
            cout << optionsDescription << endl;
			exit(EXIT_FAILURE);
        } else {
            throw e;
        }
    }
}

static void publishSolarChargerData(SysMon::SolarChargerData& solarChargerData)
{
	tm* timeInfo = localtime((time_t*)&solarChargerData.time);
	int energyGenerationWh = solarChargerData.energyYieldToday * 10;
	int powerGenerationW = round(solarChargerData.panelPower/100.00);
	int energyConsumptionWh = solarChargerData.consumedToday * 10;
	int powerConsumptionW = ((uint32_t)solarChargerData.chargerVoltage * (uint32_t)solarChargerData.chargerCurrent)/1000;
	int spiTemperatureC = solarChargerData.cpuTemperature;
	double panelVoltageV = solarChargerData.panelVoltage/100.00;
	double batteryVoltageV = solarChargerData.chargerVoltage/100.0;
	int chargerStatus = solarChargerData.deviceState;
	int energyConsumptionYesterdayWh = solarChargerData.consumedYesterday * 10;
	int rpiTemperatureC = SysMon::instance().getCpuTemperature();

#define MAX_COMMAND_LENGHT 1000
	char command[MAX_COMMAND_LENGHT];
	snprintf(command, MAX_COMMAND_LENGHT,
		"curl -d \"d=%4u%02u%02u\" -d \"t=%02u:%02u\" "	// timeInfo
		"-d \"v1=%d\" -d \"v2=%d\" "					// energyGenerationWh, powerGenerationW
		"-d \"v3=%d\" -d \"v4=%d\" "					// energyConsumptionWh, powerConsumptionW
		"-d \"v5=%d\" -d \"v6=%4.1f\" "					// spiTemperatureC, panelVoltageV
		"-d \"v7=%4.1f\" -d \"v8=%d\" "					// batteryVoltageV, chargerStatus
		"-d \"v9=%d\" -d \"v10=%d\" "					// energyConsumptionYesterdayWh, rpiTemperatureC
		"-d \"v11=%d\" -d \"v12=%d\" "					// solarChargerData.chargerCurrent, solarChargerData.loadCurrent
		"-H \"X-Pvoutput-Apikey: %s\" -H \"X-Pvoutput-SystemId: %d\" "
		"http://pvoutput.org/service/r2/addstatus.jsp",
		timeInfo->tm_year+1900, timeInfo->tm_mon+1, timeInfo->tm_mday, timeInfo->tm_hour, timeInfo->tm_min,
		energyGenerationWh, powerGenerationW,
		energyConsumptionWh, powerConsumptionW,
		spiTemperatureC, panelVoltageV,
		batteryVoltageV, chargerStatus,
		energyConsumptionYesterdayWh, rpiTemperatureC,
		solarChargerData.chargerCurrent, solarChargerData.loadCurrent,
		PvoutputApikey.c_str(), PvoutputSystemId);
	LOG_INFO << command;
	system(command);
}

static void downloadFromSPi(int argc, char *argv[])
{
	Xively::instance().init(xivelyAccountId, xivelyDeviceId, xivelyPassword);
	for(;;) {
		SysMon::SolarChargerData& solarChargerData = SysMon::instance().getSolarChargerData();
		if (solarChargerData.time == 0)
			break;
		publishSolarChargerData(solarChargerData);
		Xively::instance().publish(solarChargerData);
	}
	Xively::instance().join();
	parseCommandLine(argc, argv);
}

static void setRelays()
{
	if (relay1)
		SysMon::instance().turnOnRelay(PDU_RELAY1_ON);
	if (relay2)
		SysMon::instance().turnOnRelay(PDU_RELAY2_ON);
	if (relay3)
		SysMon::instance().turnOnRelay(PDU_RELAY3_ON);
	if (relay4)
		SysMon::instance().turnOnRelay(PDU_RELAY4_ON);
	if (relay5)
		SysMon::instance().turnOnRelay(PDU_RELAY5_ON);
	if (relay6)
		SysMon::instance().turnOnRelay(PDU_RELAY6_ON);
	if (relay7)
		SysMon::instance().turnOnRelay(PDU_RELAY7_ON);
	if (relay8)
		SysMon::instance().turnOnRelay(PDU_RELAY8_ON);
}

static int uploadToSPi()
{
	SysMon::instance().setPdu();
	SysMon::instance().setRpiSleepTime(rpiSleepTime);
	SysMon::instance().setSpiSleepTime(spiSleepTime);
	SysMon::instance().setSpiSystemTime();

	// if SDA line is held low - don't shutdown Raspberry Pi
	int spiData = digitalRead(2);	// Raspberry Pi GPIO 2 is the I2C SDA line
	return spiData;
}

int main(int argc, char *argv[])
{
	signal(SIGINT, stopProcessing);
	configureLogger();
	if (wiringPiSetupGpio() == -1) {
		LOG_ERROR << "wiringPiSetupGpio initialisation failed";
		return EXIT_FAILURE;
	}

	parseCommandLine(argc, argv);
	setRelays();

	shared_ptr<boost::asio::io_service> io_service(new boost::asio::io_service);
	processingScheduler = io_service;
	std::size_t pingReplies = 0;
	bool firstReboot = true;
	while (true) {	// check Internet connection
		time_t upTime = time(NULL);
		time_t now = upTime;
		while ((now - upTime) <= webConnectionCheckTime) {
			try
			{
				pinger ping(processingScheduler, pingServerName.c_str(), 1);
				processingScheduler->run();
				pingReplies = ping.getReplies();
				if (pingReplies)
					break;
			}
			catch (std::exception& e)
			{
				LOG_ERROR << "Exception: " << e.what();
			}
			sleep(1);
			now = time(NULL);
		}
		if (pingReplies)
			break;
		if (firstReboot) {
			firstReboot = false;
			SysMon::instance().rebootRouter(10);	// try short (10 seconds) router power off time
		} else
			SysMon::instance().rebootRouter(routerPowerOffTime);
	}

	system("/etc/init.d/ntp stop; ntpd -q -g; /etc/init.d/ntp start");	// synchronize local clock with Internet
	downloadFromSPi(argc, argv);

	int confirmRPiShutdown = uploadToSPi();
	sync();

	if (shutdownRPi && confirmRPiShutdown) {
		system("i2cset -y 1 0x24 0xFF");	// switch the SPi Software Jumper OFF
		LOG_TRACE << "Shutting down now!";
		system("sudo shutdown -h now");
	} else
		system("i2cset -y 1 0x24 0xFD");	// switch the SPi Software Jumper ON

	return EXIT_SUCCESS;
}
