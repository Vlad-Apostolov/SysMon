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

#include "simpleLogger.h"
#include "ping.h"
#include "SysMon.h"
#include "Xively.h"

using namespace std;
using namespace boost::program_options;
using namespace boost::iostreams;
using namespace boost::filesystem;

// RtiProxyClient logging control variables
static bool consoleLogging = false;			// when set to true, enables logging to console
static bool shutdownRPi = false;			// when set to true, shut down Raspberry Pi
static string logLevel = "info";			// RtiProxyClient logging threshold level: trace|debug|info|warning|error|fatal
static string logFileName = "";				// non empty string enables logging into a file
static string pingServerName = "";
static std::size_t pingRetries = 1;
static int rpiSleepTime = 30;
static int spiSleepTime = 5;
static string PvoutputApikey;
static string xivelyAccountId;
static string xivelyDeviceId;
static string xivelyPassword;
int PvoutputSystemId;


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
			("shutdownRPi", "Shutdown Raspberry Pi")
			("logFileName,f", value<string>(&logFileName)->default_value(""), "File name for logging")
			("pingServerName,p", value<string>(&pingServerName), "Server name to ping")
			("pingRetries,r", value<std::size_t>(&pingRetries)->default_value(1), "Max number of ping requests before giving up")
			("rpiSleepTime,s", value<int>(&rpiSleepTime)->default_value(30), "Number of minutes for Raspberry Pi to sleep after shutdown")
			("spiSleepTime", value<int>(&spiSleepTime)->default_value(5), "Number of minutes for Sleepy Pi to sleep")
			("PvoutputApikey,a", value<string>(&PvoutputApikey), "X-Pvoutput-Apikey")
			("PvoutputSystemId,i", value<int>(&PvoutputSystemId), "X-Pvoutput-SystemId")
			("xivelyAccountId,", value<string>(&xivelyAccountId), "xivelyAccountId")
			("xivelyDeviceId,", value<string>(&xivelyDeviceId), "xivelyDeviceId")
			("xivelyPassword,", value<string>(&xivelyPassword), "xivelyPassword")
			("config", value<std::string>(), "Config file");

	    store(parse_command_line(argc, argv, optionsDescription), variablesMap);
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
			if (configStream)
				store(parse_config_file(configStream, optionsDescription), variablesMap);
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

void publishSolarChargerData(SysMon::SolarChargerData& solarChargerData)
{
#define MAX_COMMAND_LENGHT 1000
	tm* timeInfo = localtime((time_t*)&solarChargerData.time);
	int powerGeneration = round(((double)solarChargerData.chargerCurrent * (double)solarChargerData.chargerVoltage)/1000.0);
	int energyGeneration = round((double)powerGeneration * ((double)spiSleepTime/60.0));
	int powerConsumption = round(((double)solarChargerData.loadCurrent * (double)solarChargerData.loadVoltage)/1000.0);
	int energyConsumption = round((double)powerConsumption * ((double)spiSleepTime/60.0));
	double temperature = (double)solarChargerData.cpuTemperature;
	double voltage = (double)solarChargerData.panelVoltage/100.0;
	char command[MAX_COMMAND_LENGHT];
	snprintf(command, MAX_COMMAND_LENGHT,
		"curl -d \"d=%4u%02u%02u\" -d \"t=%02u:%02u\" "
		"-d \"v1=%d\" -d \"v2=%d\" "
		"-d \"v3=%d\" -d \"v4=%d\" "
		"-d \"v5=%3.1f\" -d \"v6=%4.1f\" "
		"-H \"X-Pvoutput-Apikey: %s\" -H \"X-Pvoutput-SystemId: %d\" "
		"http://pvoutput.org/service/r2/addstatus.jsp",
		timeInfo->tm_year+1900, timeInfo->tm_mon+1, timeInfo->tm_mday, timeInfo->tm_hour, timeInfo->tm_min,
		energyGeneration, powerGeneration,
		energyConsumption, powerConsumption,
		temperature, voltage,
		PvoutputApikey.c_str(), PvoutputSystemId);
	system(command);
}

int main(int argc, char *argv[])
{
	std::size_t pingReplies = 0;

	shared_ptr<boost::asio::io_service> io_service(new boost::asio::io_service);
	processingScheduler = io_service;
	signal(SIGINT, stopProcessing);
	parseCommandLine(argc, argv);
	configureLogger();

	try
	{
		pinger ping(processingScheduler, pingServerName.c_str(), pingRetries);
		processingScheduler->run();
		pingReplies = ping.getReplies();
	}
	catch (std::exception& e)
	{
		LOG_ERROR << "Exception: " << e.what();
	}

	LOG_TRACE << "ping replies: " << pingReplies;

	if (pingReplies) {
		Xively::instance().init(xivelyAccountId, xivelyDeviceId, xivelyPassword);
		for(;;) {
			SysMon::SolarChargerData& solarChargerData = SysMon::instance().getSolarChargerData();
			if (solarChargerData.time == 0)
				break;
			publishSolarChargerData(solarChargerData);
			Xively::instance().publish(solarChargerData);
		}
#if 0
		SysMon::SolarChargerData solarChargerTestData;
		solarChargerTestData.chargerCurrent = 12;
		solarChargerTestData.chargerPowerToday = 254;
		solarChargerTestData.chargerTemperature = 3215;
		solarChargerTestData.chargerVoltage = 1450;
		solarChargerTestData.loadCurrent = 8;
		solarChargerTestData.loadVoltage = 1272;
		solarChargerTestData.panelCurrent = 25;
		solarChargerTestData.panelPower = 10000;
		solarChargerTestData.panelVoltage = 10000;
		solarChargerTestData.time = time(NULL);
		publishSolarChargerData(solarChargerTestData);
		Xively::instance().publish(solarChargerTestData);
#endif
		Xively::instance().join();
		parseCommandLine(argc, argv);
	} else
		SysMon::instance().rebootRouter();

	SysMon::instance().setRpiSleepTime(rpiSleepTime);
	SysMon::instance().setSpiSleepTime(spiSleepTime);

	if (shutdownRPi) {
		sync();
		LOG_TRACE << "Shutting down now!";
		reboot(LINUX_REBOOT_CMD_POWER_OFF);
	}

	return EXIT_SUCCESS;
}
