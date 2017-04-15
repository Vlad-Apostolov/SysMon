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
		uint16_t chargerVoltage;
		uint16_t chargerCurrent;
		uint16_t chargerPowerToday;
		uint16_t loadVoltage;
		uint16_t loadCurrent;
		uint16_t panelVoltage;
		uint16_t panelCurrent;
		int16_t cpuTemperature;
	};

	static SysMon& instance();
	void setRpiSleepTime(int minutes);
	void setSpiSleepTime(int minutes);
	void turnOnRelay(uint16_t relay) { _pduControl |= relay; }
	void setPdu();
	SolarChargerData& getSolarChargerData();
	void rebootRouter();
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
#define PDU_ROUTER_ON				0x0100
#define PDU_RPI_ON					0x0200

	enum MessageTag {
		TAG_PDU_CONTROL,
		TAG_RPI_SLEEP_TIME,
		TAG_SPI_SLEEP_TIME
	};

	SysMon() :
		_pduControl(PDU_RPI_ON | PDU_ROUTER_ON)
	{

	}
	virtual ~SysMon() {}

	void sendMessage(const char* message);

	uint16_t _pduControl;
	SolarChargerData _solarChargerData;
};

#endif /* SYSMON_H_ */
