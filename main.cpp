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

#include "simpleLogger.h"
#include "ping.h"

using namespace std;
using namespace boost::program_options;
using namespace boost::iostreams;
using namespace boost::filesystem;

// RtiProxyClient logging control variables
static bool consoleLogging = false;			// when set to true, enables logging to console
static string logLevel = "info";			// RtiProxyClient logging threshold level: trace|debug|info|warning|error|fatal
static string logFileName = "";				// non empty string enables logging into a file
static string pingServerName = "";
static variables_map variablesMap;
static options_description optionsDescription{"Options"};
static std::size_t pingRetries = 1;


static shared_ptr<boost::asio::io_service> processingScheduler;				// processing scheduler

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
	try	{
		optionsDescription.add_options()
			("help,h", "Help screen")
			("logLevel,l", value<string>(&logLevel)->default_value("info"), "Logging severity threshold: trace|debug|info|warning|error|fatal")
			("consoleLog,c", "Enable console logging")
			("logFileName,f", value<string>(&logFileName)->default_value(""), "File name for logging")
			("pingServerName,p", value<string>(&pingServerName)->required(), "Server name to ping")
			("pingRetries,r", value<std::size_t>(&pingRetries)->default_value(1), "Max number of ping requests before giving up");


		command_line_parser commandLineParser{argc, argv};
		commandLineParser.options(optionsDescription);
		parsed_options parsedOptions = commandLineParser.run();

		store(parsedOptions, variablesMap);
		notify(variablesMap);

		if (variablesMap.count("help")) {
			cout << optionsDescription << endl;
			exit(EXIT_SUCCESS);
		}

		if (variablesMap.count("consoleLog"))
			consoleLogging = true;
	} catch (const boost::program_options::required_option& e) {
        if (variablesMap.count("help")) {
            cout << optionsDescription << endl;
			exit(EXIT_FAILURE);
        } else {
            throw e;
        }
    }
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
		std::cerr << "Exception: " << e.what() << std::endl;
	}

	LOG_TRACE << "ping replies: " << pingReplies;
	return EXIT_SUCCESS;
}
