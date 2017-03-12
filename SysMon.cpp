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

void SysMon::setSleepTime(int minutes)
{
	// message format "$XX,XX\0"
	std::stringstream os;
	os << '$' << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << TAG_RPI_SLEEP_TIME << ',';
	os << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << minutes << '\0';
	const char* message = os.str().c_str();
	sendMessage(message);
}

void SysMon::rebootRouter()
{
	_pduControl &= ~PDU_ROUTER_ON;
	// message format "$XX,XX\0"
	std::stringstream os;
	os << '$' << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << TAG_PDU_CONTROL << ',';
	os << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << _pduControl << '\0';
	const char* message = os.str().c_str();
	sendMessage(message);
	_pduControl |= PDU_ROUTER_ON;
	os.str("");
	os.clear();
	os << '$' << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << TAG_PDU_CONTROL << ',';
	os << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << _pduControl << '\0';
	message = os.str().c_str();
	sendMessage(message);
}

void SysMon::sendMessage(const char* message)
{
	char *filename = (char*)"/dev/i2c-1";
	int i2cFd;

	if ((i2cFd = open(filename, O_RDWR)) < 0) {
		LOG_ERROR << "Failed to open the i2c bus";
		return;
	}

	if (ioctl(i2cFd, I2C_SLAVE, ARDUINO_I2C_SLAVE_ADDRESS) < 0) {
		LOG_ERROR << "Failed to acquire i2c bus access";
		return;
	}

	int length = strlen(message) + 1;
	if (write(i2cFd, message, length) != length) {
		LOG_ERROR << "Failed to write to the i2c bus";
	}
	close(i2cFd);
}

