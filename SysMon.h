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
	static SysMon& instance();
	void rebootRouter();

private:
#define ARDUINO_I2C_SLAVE_ADDRESS	100
#define MAX_MESSAGE_LENGHT			10

#define PDU_ROUTER_ON				0x0001
#define PDU_CAM1_ON					0x0002
#define PDU_CAM2_ON					0x0004
#define PDU_CAM3_ON					0x0008
#define PDU_CAM4_ON					0x0010

	enum MessageTag {
		TAG_PDU_CONTROL,
		TAG_RPI_SLEEP_TIME,
	};

	SysMon() :
		_pduControl(PDU_ROUTER_ON | PDU_CAM1_ON | PDU_CAM2_ON | PDU_CAM3_ON | PDU_CAM4_ON)
	{

	}
	virtual ~SysMon() {}

	void sendMessage(const char* message);

	uint16_t _pduControl;
};

#endif /* SYSMON_H_ */
