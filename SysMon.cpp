/*
 * SysMon.cpp
 *
 *  Created on: 12/03/2017
 *      Author: vlad
 */

#include <string>
#include <sstream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <iomanip>

#include "SysMon.h"
#include "simpleLogger.h"

SysMon& SysMon::instance()
{
	static SysMon inst;
	return inst;
}

void SysMon::setRpiSleepTime(int minutes)
{
	// message format "$XX,XXXX\0"
	std::stringstream os;
	os << '$' << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << TAG_RPI_SLEEP_TIME << ',';
	os << std::uppercase << std::setfill('0') << std::setw(4) << std::hex << minutes << '\0';
	const char* message = os.str().c_str();
	sendMessage(message);
}

void SysMon::setSpiSleepTime(int minutes)
{
	// message format "$XX,XXXX\0"
	std::stringstream os;
	os << '$' << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << TAG_SPI_SLEEP_TIME << ',';
	os << std::uppercase << std::setfill('0') << std::setw(4) << std::hex << minutes << '\0';
	const char* message = os.str().c_str();
	sendMessage(message);
}

void SysMon::rebootRouter()
{
	_pduControl &= ~PDU_RELAY7_ON;
	setPdu();
	_pduControl |= PDU_RELAY7_ON;
	setPdu();
}

void SysMon::setPdu()
{
	// message format "$XX,XXXX\0"
	std::stringstream os;
	os << '$' << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << TAG_PDU_CONTROL << ',';
	os << std::uppercase << std::setfill('0') << std::setw(4) << std::hex << _pduControl << '\0';
	const char* message = os.str().c_str();
	sendMessage(message);
}

void SysMon::sendMessage(const char* message)
{
	char *filename = (char*)"/dev/i2c-1";
	int i2cFd;
	int length = strlen(message) + 1;

	if ((i2cFd = open(filename, O_RDWR | O_SYNC)) < 0) {
		LOG_ERROR << "Failed to open the i2c bus";
		return;
	}

	if (ioctl(i2cFd, I2C_SLAVE, ARDUINO_I2C_SLAVE_ADDRESS) < 0) {
		LOG_ERROR << "Failed to acquire i2c bus access";
		goto sendMessageEnd;
	}

	if (write(i2cFd, message, length) != length) {
		LOG_ERROR << "Failed to write to the i2c bus";
	}

	fsync(i2cFd);

sendMessageEnd:
	close(i2cFd);
	sleep(1);	// wait for i2c transfer to finish (fsync and O_SYNC don't work)
}

SysMon::SolarChargerData& SysMon::getSolarChargerData()
{
	char *filename = (char*)"/dev/i2c-1";
	int i2cFd;
	int length = sizeof(SolarChargerData);
	memset(&_solarChargerData, 0, length);

	if ((i2cFd = open(filename, O_RDWR)) < 0) {
		LOG_ERROR << "Failed to open the i2c bus";
		return _solarChargerData;
	}

	if (ioctl(i2cFd, I2C_SLAVE, ARDUINO_I2C_SLAVE_ADDRESS) < 0) {
		LOG_ERROR << "Failed to acquire i2c bus access";
		goto getSolarChargerDataEnd;
	}

	if (read(i2cFd, &_solarChargerData, length) != length) {
		LOG_ERROR << "Failed to read from the i2c bus";
	}
getSolarChargerDataEnd:
	close(i2cFd);
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

