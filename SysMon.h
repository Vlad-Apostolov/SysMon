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
		uint16_t chargerVoltage;
		uint16_t chargerCurrent;
		uint16_t chargerPowerToday;
		int16_t chargerTemperature;
		uint16_t loadVoltage;
		uint16_t loadCurrent;
		uint16_t panelVoltage;
		uint16_t panelCurrent;
		uint32_t panelPower;
	};

	static SysMon& instance();
	void setRpiSleepTime(int minutes);
	void setSpiSleepTime(int minutes);
	SolarChargerData& getSolarChargerData();
	void rebootRouter();

private:
#define ARDUINO_I2C_SLAVE_ADDRESS	55
#define MESSAGE_LENGHT				7

#define PDU_ROUTER_ON				0x0001
#define PDU_CAM1_ON					0x0002
#define PDU_CAM2_ON					0x0004
#define PDU_CAM3_ON					0x0008
#define PDU_CAM4_ON					0x0010

	enum MessageTag {
		TAG_PDU_CONTROL,
		TAG_RPI_SLEEP_TIME,
		TAG_SPI_SLEEP_TIME
	};

	SysMon() :
		_pduControl(PDU_ROUTER_ON | PDU_CAM1_ON | PDU_CAM2_ON | PDU_CAM3_ON | PDU_CAM4_ON)
	{

	}
	virtual ~SysMon() {}

	void sendMessage(const char* message);

	uint16_t _pduControl;
	SolarChargerData _solarChargerData;
};

#endif /* SYSMON_H_ */
