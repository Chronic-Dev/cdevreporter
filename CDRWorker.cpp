#include <cstdio>
#include "CDRWorker.h"
#include "CDRFetcher.h"
#include "device_types.h"

static int detection_blocked = 0;
static CDRWorker* self;

static const char* getDeviceName(const char* productType)
{
	int i = 0;
	while (device_types[i].productType) {
		if (strcmp(device_types[i].productType, productType) == 0) {
			return device_types[i].displayName;
		}
		i++;
	}
	return "(Unknown Device)";
}

static void device_event_cb(const idevice_event_t* event, void* userdata)
{
	if (!detection_blocked) {
		self->DeviceEventCB(event, userdata);
	}
}

CDRWorker::CDRWorker(void* main)
	: mainWnd(main), device_count(0)
{
	self = this;

	int num = 0;
	char **devices = NULL;
	idevice_get_device_list(&devices, &num);
	idevice_device_list_free(devices);
	if (num == 0) {
		this->checkDevice();
	}

	idevice_event_subscribe(&device_event_cb, NULL);
}

CDRWorker::~CDRWorker(void)
{
	idevice_event_unsubscribe();
}

void CDRWorker::DeviceEventCB(const idevice_event_t *event, void *user_data)
{
	if (event->event == IDEVICE_DEVICE_ADD) {
		this->device_count++;
		this->checkDevice();
	} else if (event->event == IDEVICE_DEVICE_REMOVE) {
		this->device_count--;
		this->checkDevice();
	}
}

void CDRWorker::checkDevice()
{
	CDRMainWnd* reporter = (CDRMainWnd*)this->mainWnd;

	if (this->device_count == 0) {
		reporter->setButtonEnabled(0);
		reporter->setStatusText(wxT("Plug in your iDevice to begin."));
	} else if (this->device_count == 1) {
		idevice_t dev = NULL;
		idevice_error_t ierr = idevice_new(&dev, NULL);
		if (ierr != IDEVICE_E_SUCCESS) {
			wxString str;
			str.Printf(wxT("Error detecting device type (idevice error %d)"), ierr);
			reporter->setStatusText(str);
			return;
		}

		lockdownd_client_t client = NULL;
		lockdownd_error_t lerr = lockdownd_client_new_with_handshake(dev, &client, "cdevreporter");
		if (lerr != LOCKDOWN_E_SUCCESS) {
			idevice_free(dev);
			wxString str;
			str.Printf(wxT("Error detecting device (lockdown error %d"), lerr);
			reporter->setStatusText(str);
			return;
		}

		plist_t node = NULL;
		char* productType = NULL;
		char* productVersion = NULL;
		char* buildVersion = NULL;

		node = NULL;
		lerr = lockdownd_get_value(client, NULL, "ProductType", &node);
		if (node) {
			plist_get_string_val(node, &productType);
			plist_free(node);
		} 
		if ((lerr != LOCKDOWN_E_SUCCESS) || !productType) {
			lockdownd_client_free(client);
			idevice_free(dev);
			wxString str;
			str.Printf(wxT("Error getting product type (lockdown error %d"), lerr);
			reporter->setStatusText(str);
			return;
		}

		node = NULL;	
		lerr = lockdownd_get_value(client, NULL, "ProductVersion", &node);
		if (node) {
			plist_get_string_val(node, &productVersion);
			plist_free(node);
		}
		if ((lerr != LOCKDOWN_E_SUCCESS) || !productVersion) {
			free(productType);
			lockdownd_client_free(client);
			idevice_free(dev);
			wxString str;
			str.Printf(wxT("Error getting product version (lockdownd error %d"), lerr);
			reporter->setStatusText(str);
			return;
		}	

		node = NULL;
		lerr = lockdownd_get_value(client, NULL, "BuildVersion", &node); 
		if (node) {
			plist_get_string_val(node, &buildVersion);
			plist_free(node);
		}
		if ((lerr != LOCKDOWN_E_SUCCESS) || !buildVersion) {
			free(productType);
			free(productVersion);
			lockdownd_client_free(client);
			idevice_free(dev);
			wxString str;
			str.Printf(wxT("Error getting build version (lockdownd error %d"), lerr);
			reporter->setStatusText(str);
			return;
		}

		reporter->setButtonEnabled(1);

		wxString str;
		str.Printf(wxT("%s with iOS %s (%s) detected. Click the button to begin."), wxString(getDeviceName(productType), wxConvUTF8).c_str(), wxString(productVersion, wxConvUTF8).c_str(), wxString(buildVersion, wxConvUTF8).c_str());
		reporter->setStatusText(str);
	} else {
		reporter->setButtonEnabled(0);
		reporter->setStatusText(wxT("Please attach only one device."));
	}
}

void CDRWorker::processStart(void)
{
	CDRMainWnd* reporter = (CDRMainWnd*)this->mainWnd;

	detection_blocked = 1;
	reporter->setStatusText(wxT("Hold on..."));

	CDRFetcher* fetcher = new CDRFetcher(this);
	fetcher->Create();
	fetcher->Run();
}

void CDRWorker::processStatus(const char* msg)
{
	CDRMainWnd* reporter = (CDRMainWnd*)this->mainWnd;
	wxString str = wxString(msg, wxConvUTF8);
	reporter->setStatusText(str);
}

void CDRWorker::processFinished(const char* error)
{
	CDRMainWnd* reporter = (CDRMainWnd*)this->mainWnd;

	detection_blocked = 0;

	if (error) {
		wxString str = wxString(error, wxConvUTF8);
		reporter->setStatusText(str);
		reporter->setButtonEnabled(1);
	} else {
		reporter->setStatusText(wxT("SUCCESS\nThanks for your submission! You can unplug your device now."));
	}
}
