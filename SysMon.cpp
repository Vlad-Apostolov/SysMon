/*
 * SysMon.cpp
 *
 *  Created on: 12/03/2017
 *      Author: vlad
 */

#include <string>
#include <sstream>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <iomanip>

#include "SysMon.h"
#include "simpleLogger.h"

SysMon& SysMon::instance()
{
	static SysMon inst;
	static bool firsCall = true;

	if (firsCall) {
		firsCall = false;
		char *filename = (char*)"/dev/i2c-1";

		if ((inst._i2cFd = open(filename, O_RDWR | O_SYNC)) < 0)
			LOG_ERROR << "Failed to open the i2c bus";
		else if (ioctl(inst._i2cFd, I2C_SLAVE, ARDUINO_I2C_SLAVE_ADDRESS) < 0)
			LOG_ERROR << "Failed to acquire i2c bus access";
	}
	return inst;
}

bool SysMon::setRpiSleepTime(int minutes)
{
	// message format "$XX,XXXXXXXX\0"
	std::stringstream os;
	os << '$' << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << TAG_RPI_SLEEP_TIME << ',';
	os << std::uppercase << std::setfill('0') << std::setw(8) << std::hex << minutes << '\0';
	const char* message = os.str().c_str();
	return sendMessage(message);
}

bool SysMon::setSpiSleepTime(int minutes)
{
	// message format "$XX,XXXXXXXX\0"
	std::stringstream os;
	os << '$' << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << TAG_SPI_SLEEP_TIME << ',';
	os << std::uppercase << std::setfill('0') << std::setw(8) << std::hex << minutes << '\0';
	const char* message = os.str().c_str();
	return sendMessage(message);
}

bool SysMon::setSpiSystemTime()
{
	time_t now = time(NULL);
	// message format "$XX,XXXXXXXX\0"
	std::stringstream os;
	os << '$' << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << TAG_SPI_SYSTEM_TIME << ',';
	os << std::uppercase << std::setfill('0') << std::setw(8) << std::hex << now << '\0';
	const char* message = os.str().c_str();
	return sendMessage(message);
}

bool SysMon::rebootRouter()
{
#define ROUTER_POWER_OFF_TIME	10	// in seconds
	auto lastPduControl = _pduControl;
	_pduControl |= PDU_RELAY7_ON;
	setPdu();
	sleep(ROUTER_POWER_OFF_TIME);
	_pduControl = lastPduControl;
	return setPdu();
}

bool SysMon::setPdu()
{
	// message format "$XX,XXXX\0"
	std::stringstream os;
	os << '$' << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << TAG_PDU_CONTROL << ',';
	os << std::uppercase << std::setfill('0') << std::setw(8) << std::hex << _pduControl << '\0';
	const char* message = os.str().c_str();
	return sendMessage(message);
}

bool SysMon::sendMessage(const char* message)
{
	if (_i2cFd < 0)
		return false;
	int length = strlen(message) + 1;
	if (write(_i2cFd, message, length) != length) {
		LOG_ERROR << "Failed to write to the i2c bus";
		return false;
	}
	fsync(_i2cFd);
	sleep(1);	// wait for i2c transfer to finish (fsync and O_SYNC don't work)
	return true;
}

SysMon::SolarChargerData& SysMon::getSolarChargerData()
{
	int length = sizeof(SolarChargerData);
	memset(&_solarChargerData, 0, length);
	if (_i2cFd >= 0) {
		if (read(_i2cFd, &_solarChargerData, length) != length)
			LOG_ERROR << "Failed to read from the i2c bus";
	}
	return _solarChargerData;
}

int8_t SysMon::getCpuTemperature()
{
	FILE *temperatureFile;
	temperatureFile = fopen ("/sys/class/thermal/thermal_zone0/temp", "r");
	if (temperatureFile == NULL)
		return 0;
	double temperature;
	fscanf (temperatureFile, "%lf", &temperature);
	fclose (temperatureFile);
	temperature /= 1000;
	return round(temperature);
}

SysMon::~SysMon()
{
	close(_i2cFd);
}
