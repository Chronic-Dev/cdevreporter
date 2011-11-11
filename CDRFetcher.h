#ifndef __CDRFETCHER_H 
#define __CDRFETCHER_H

#include <wx/wx.h>
#include <libimobiledevice/afc.h>

#include "CDRWorker.h"

#define POST_UPLOAD_URL "http://battleground-d5ikdxmjm5.elasticbeanstalk.com/upload.jsp"

class CDRFetcher : public wxThread
{
private:
	CDRWorker* worker;
public:
	CDRFetcher(CDRWorker* worker)
		: wxThread(wxTHREAD_JOINABLE), worker(worker)
	{}

	wxThread::ExitCode Entry(void);
};

#endif /* __CDRFETCHER_H */
