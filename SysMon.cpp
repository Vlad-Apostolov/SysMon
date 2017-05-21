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
#include <wiringPi.h>

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

bool SysMon::rebootRouter(time_t routerPowerOffTime)
{
	auto lastPduControl = _pduControl;
	_pduControl |= PDU_RELAY7_ON;
	setPdu();
	sleep(routerPowerOffTime);
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
	waitForI2cToFinish();
	int length = strlen(message) + 1;
	if (write(_i2cFd, message, length) != length) {
		LOG_ERROR << "Failed to write to the i2c bus";
		return false;
	}
	return true;
}

SysMon::SolarChargerData& SysMon::getSolarChargerData()
{
	int length = sizeof(SolarChargerData);
	memset(&_solarChargerData, 0, length);
	if (_i2cFd >= 0) {
		waitForI2cToFinish();
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

void SysMon::waitForI2cToFinish()
{
#define	WAIT_MS		100
	int timeout = WAIT_MS;		// wait for I2C SCL line to stabilize high
	while(true) {
		if (!digitalRead(3))	// Raspberry Pi GPIO 3 is the I2C SCL line
			timeout = WAIT_MS;
		else if (timeout)
			timeout--;
		else
			break;				// I2C SCL line has been high for 10 ms. The I2C transaction has finished.
		usleep(1000);			// sleep for 1 ms
	}
}

SysMon::~SysMon()
{
	close(_i2cFd);
}
