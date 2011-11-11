#ifndef __CDRMAINWND_H
#define __CDRMAINWND_H

#include <wx/wx.h>

#include "CDRWorker.h"

#define CDEV_REPORTER_TITLE "Chronic-Dev Crash Reporter"
#define WND_WIDTH 480
#define WND_HEIGHT 300

class CDRWorker;

class CDRMainWnd : public wxFrame
{
private:
	wxStaticText* lbStatus;
	wxButton* btnStart;
	CDRWorker* worker;

public:
	CDRMainWnd(void);

	void setButtonEnabled(int enabled);
	void setStatusText(const wxString& text);

	void handleStartClicked(wxCommandEvent& event);
	void OnQuit(wxCommandEvent& event);
};

enum {
	ID_QUIT = 1,
	ID_ABOUT,
};

#endif /* __CDRMAINWND_H */

