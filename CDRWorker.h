#ifndef __CDRWORKER_H
#define __CDRWORKER_H

#include <wx/wx.h>
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>

#include "CDRMainWnd.h"

struct device_type_t {
	const char* productType;
	const char* displayName;
};

static struct device_type_t device_types[] = {
	{"iPhone1,1", "iPhone"},
	{"iPod1,1", "iPod Touch"},
	{"iPhone1,2", "iPhone 3G"},
	{"iPod2,1", "iPod touch (2G)"},
	{"iPhone2,1", "iPhone 3GS"},
	{"iPod3,1", "iPod Touch (3G)"},
	{"iPad1,1", "iPad"},
	{"iPhone3,1", "iPhone 4 (GSM)"},
	{"iPod4,1", "iPod Touch (4G)"},
	{"AppleTV2,1", "AppleTV (2G)"},
	{"iPhone3,1", "iPhone 4 (CDMA)"},
	{"iPad2,1", "iPad 2"},
	{"iPad2,2", "iPad 2 (GSM)"},
	{"iPad2,3", "iPad 2 (CDMA"},
	{"iPhone4,1" "iPhone 4S"},
	{NULL, NULL}
};

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
