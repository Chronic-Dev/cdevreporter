#include "CDRMainWnd.h"

CDRMainWnd::CDRMainWnd(void)
	: wxFrame(NULL, wxID_ANY, wxT(CDEV_REPORTER_TITLE), wxDefaultPosition, wxSize(WND_WIDTH, WND_HEIGHT), (wxSTAY_ON_TOP | wxDEFAULT_FRAME_STYLE) & ~(wxRESIZE_BORDER | wxMAXIMIZE_BOX))
{
#if defined(__WXMSW__)
	SetIcon(wxICON(AppIcon));
#endif

	wxPanel* panel = new wxPanel(this, wxID_ANY, wxPoint(0, 0), wxSize(WND_WIDTH, WND_HEIGHT));

	wxBoxSizer *vbox = new wxBoxSizer(wxVERTICAL);

	wxStaticText* lbTop = new wxStaticText(panel, wxID_ANY, wxT("Welcome!\n\nThis tool will fetch all crash reports - if present - from your iDevice and submits them to us, the Chronic-Dev Team. This might help us to find new exploits."), wxDefaultPosition, wxDefaultSize, wxST_NO_AUTORESIZE | wxALIGN_LEFT);
	lbTop->Wrap(WND_WIDTH-20);

	lbStatus = new wxStaticText(panel, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize, wxST_NO_AUTORESIZE);

	wxStaticText* lbYo = new wxStaticText(panel, wxID_ANY, wxT("Yes, I am willing to help!"));
	btnStart = new wxButton(panel, 1111, wxT("Do it!"));
	btnStart->Enable(0);
	Connect(1111, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(CDRMainWnd::handleStartClicked));

	wxBoxSizer* hbox = new wxBoxSizer(wxHORIZONTAL);
	hbox->Add(lbYo, 0, wxALIGN_CENTER_VERTICAL | wxALL, 10);
	hbox->Add(btnStart, 0, wxALIGN_CENTER_VERTICAL | wxALL, 10);

	wxStaticText* lbCredits = new wxStaticText(panel, wxID_ANY, wxT("Chronic-Dev Crash Reporter © 2011 Chronic-Dev Team\nby Joshua Hill, Nikias Bassen, and Hanéne Samara"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER | wxST_NO_AUTORESIZE);

	vbox->Add(lbTop, 0, wxEXPAND | wxALL, 10);
	vbox->Add(lbStatus, 1, wxEXPAND | wxALL, 10);
	vbox->Add(hbox, 0, wxCENTER | wxALL, 10);
	vbox->Add(lbCredits, 0, wxCENTER | wxALL, 10);

	panel->SetSizer(vbox);

	Centre();

	this->worker = new CDRWorker((CDRMainWnd*)this);
}

#define THREAD_SAFE(X) \
	if (!wxIsMainThread()) { \
		wxMutexGuiEnter(); \
		X; \
		wxMutexGuiLeave(); \
	} else { \
		X; \
	}

void CDRMainWnd::setButtonEnabled(int enabled)
{
	THREAD_SAFE(this->btnStart->Enable(enabled));
}

void CDRMainWnd::setStatusText(const wxString& text)
{
	THREAD_SAFE(this->lbStatus->SetLabel(text));
}

void CDRMainWnd::handleStartClicked(wxCommandEvent& WXUNUSED(event))
{
	this->setButtonEnabled(0);
	this->worker->processStart();
}

void CDRMainWnd::OnQuit(wxCommandEvent& WXUNUSED(event))
{
	Close(true);
}

