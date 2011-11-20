#ifndef __CDRWORKER_H
#define __CDRWORKER_H

#include <wx/wx.h>
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>

#include "CDRMainWnd.h"

class CDRWorker
{
private:
	void* mainWnd;
	int device_count;
public:
	CDRWorker(void* main);
	~CDRWorker(void);
	void DeviceEventCB(const idevice_event_t *event, void *user_data);
	void checkDevice(void);
	void processStart(void);
	void processStatus(const char* msg);
	void processFinished(const char* error);
};

#endif /* __CDRWORKER_H */
