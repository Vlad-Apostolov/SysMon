/*
 * Xively.cpp
 *
 *  Created on: 24/03/2017
 *      Author: vlad
 */

#include "Xively.h"
#include "simpleLogger.h"

Xively& Xively::instance()
{
	static Xively *inst = NULL;
	if (inst == NULL)
		inst = new Xively();
	return *inst;
}

bool Xively::init(std::string accountId, std::string deviceId, std::string password, std::string channel)
{
	bool result = false;

	_accountId = accountId;
	_deviceId = deviceId;
	_password = password;
	_channel = channel;
	if (xi_initialize(_accountId.c_str(), _deviceId.c_str(), NULL) == XI_STATE_OK) {
		if ((_context = xi_create_context()) >  XI_INVALID_CONTEXT_HANDLE)
			result = true;
		else
			LOG_ERROR << __PRETTY_FUNCTION__ << "Xively failed to create content";
	} else
		LOG_ERROR << __PRETTY_FUNCTION__ << "Xively failed to initialize";
	return result;
}

void Xively::publish(const xi_context_handle_t, const xi_timed_task_handle_t, void*)
{
	if (Xively::instance()._solarChargerDataList.empty())
		return;

	auto& solarChargerData = Xively::instance()._solarChargerDataList.front();
	snprintf(Xively::instance()._message, MAX_MESSAGE_SIZE,
		"time: %d, chargerCurrent: %d, chargerPowerToday: %d, chargerTemperature: %d, chargerVoltage: %d, "
		"loadVoltage: %d, loadCurrent: %d, panelVoltage: %d, panelCurrent: %d, panelPower: %d",
		solarChargerData.time,
		solarChargerData.chargerVoltage,
		solarChargerData.chargerCurrent,
		solarChargerData.chargerPowerToday,
		solarChargerData.chargerTemperature,
		solarChargerData.loadVoltage,
		solarChargerData.loadCurrent,
		solarChargerData.panelVoltage,
		solarChargerData.panelCurrent,
		solarChargerData.panelPower
		);
	Xively::instance()._solarChargerDataList.pop_front();
    xi_publish(Xively::instance()._context,
    		   Xively::instance()._channel.c_str(),
    		   Xively::instance()._message,
    		   XI_MQTT_QOS_AT_LEAST_ONCE,
    		   XI_MQTT_RETAIN_FALSE,
    		   NULL,
    		   NULL);
	if (!Xively::instance()._solarChargerDataList.empty())
		xi_schedule_timed_task(Xively::instance()._context, Xively::publish, 5, 0, NULL);
}

void Xively::join()
{
	if (_context > XI_INVALID_CONTEXT_HANDLE) {
		xi_connect(_context,
				  _deviceId.c_str(),
				   _password.c_str(),
				   CONNECTION_TIMEOUT,
				   KEEPALIVE_TIMEOUT,
				   XI_SESSION_CLEAN,
				   [](xi_context_handle_t context, void* data, xi_state_t state)
		{
			auto& connectionData = *(xi_connection_data_t*)data;
			switch (connectionData.connection_state)
			{
				case XI_CONNECTION_STATE_OPENED:
					publish(Xively::instance()._context, XI_INVALID_TIMED_TASK_HANDLE, NULL);
					LOG_INFO << __PRETTY_FUNCTION__ << "Xively connected";
					break;
				case XI_CONNECTION_STATE_OPEN_FAILED:
					LOG_ERROR << __PRETTY_FUNCTION__ << "Xively connection failed, state " << state;
					Xively::instance().shutdown();
					break;
				case XI_CONNECTION_STATE_CLOSED:
					LOG_INFO << __PRETTY_FUNCTION__ << "Xively connection closed";
					Xively::instance().shutdown();
					break;
				default:
					LOG_ERROR << __PRETTY_FUNCTION__ << "Unexpected connection_state";
					Xively::instance().shutdown();
					break;
			}

		});
		xi_events_process_blocking();
		shutdown();
	}
}

void Xively::shutdown()
{
	if (_context > XI_INVALID_CONTEXT_HANDLE) {
	    xi_delete_context(_context);
	    xi_shutdown();
		LOG_INFO << __PRETTY_FUNCTION__;
	}
}

