/*
 * SysMon.h
 *
 *  Created on: 12/03/2017
 *      Author: vlad
 */

#ifndef SYSMON_H_
#define SYSMON_H_

#include <stdint.h>

class SysMon {
public:
	struct SolarChargerData {
		uint32_t time;
		uint32_t panelPower;
		uint16_t panelVoltage;
		uint16_t panelCurrent;
		uint16_t chargerVoltage;
		uint16_t chargerCurrent;
		uint16_t chargerPowerToday;
		uint16_t loadCurrent;
		int16_t cpuTemperature;
	};

	static SysMon& instance();
	bool setRpiSleepTime(int minutes);
	bool setSpiSleepTime(int minutes);
	bool setSpiSystemTime();
	void turnOnRelay(uint16_t relay) { _pduControl |= relay; }
	bool setPdu();
	SolarChargerData& getSolarChargerData();
	bool rebootRouter();
	int8_t getCpuTemperature();

private:
#define ARDUINO_I2C_SLAVE_ADDRESS	55
#define MESSAGE_LENGHT				7

#define PDU_RELAY1_ON				0x0001
#define PDU_RELAY2_ON				0x0002
#define PDU_RELAY3_ON				0x0004
#define PDU_RELAY4_ON				0x0008
#define PDU_RELAY5_ON				0x0010
#define PDU_RELAY6_ON				0x0020
#define PDU_RELAY7_ON				0x0040
#define PDU_RELAY8_ON				0x0080
#define PDU_EXTERNAL_POWER_ON		0x0100
#define PDU_RPI_ON					0x0200

	enum MessageTag {
		TAG_PDU_CONTROL,
		TAG_RPI_SLEEP_TIME,
		TAG_SPI_SLEEP_TIME,
		TAG_SPI_SYSTEM_TIME
	};

	SysMon() :
		_pduControl(PDU_RPI_ON),
		_i2cFd(-1)
	{

	}
	virtual ~SysMon();

	bool sendMessage(const char* message);

	uint16_t _pduControl;
	int _i2cFd;
	SolarChargerData _solarChargerData;
};

#endif /* SYSMON_H_ */
