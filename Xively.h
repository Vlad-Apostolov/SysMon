/*
 * Xively.h
 *
 *  Created on: 24/03/2017
 *      Author: vlad
 */

#ifndef XIVELY_H_
#define XIVELY_H_

#include <string>
#include <list>
#include <vector>

#include "xively.h"
#include "SysMon.h"

class Xively {
public:
	enum MessageChannel {
		MC_1,
		MC_2,
		MC_3,
		MC_LOG,
		MC_COUNT
	};
	static Xively& instance(const char* accountId, const char* deviceId, const char* password);
	static Xively& instance();
	bool init(std::string accountId, std::string deviceId, std::string password);
	void publish(SysMon::SolarChargerData& solarChargerData) { _solarChargerDataList.push_back(solarChargerData); }
	void join();

private:
#define CONNECTION_TIMEOUT	10
#define KEEPALIVE_TIMEOUT	20
#define MAX_MESSAGE_SIZE	1024
	void connect();
	void subscribe();
	static void publish(const xi_context_handle_t, const xi_timed_task_handle_t, void*);
	void shutdown();
	Xively() :
		_context(XI_INVALID_CONTEXT_HANDLE)
	{
	}
	virtual ~Xively() {}

	xi_context_handle_t _context;
	std::string _accountId;
	std::string _deviceId;
	std::string _password;
	std::list<SysMon::SolarChargerData> _solarChargerDataList;
	char _message[MAX_MESSAGE_SIZE];
	std::vector<std::string> _channel;
};

#endif /* XIVELY_H_ */
