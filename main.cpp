#include <wx/wx.h>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <vector>

#ifdef __APPLE__
#include <Security/Authorization.h>
#endif

#ifdef WIN32
#include <windows.h>
#endif

#include "CDRMainWnd.h"

#if defined(__APPLE__) || defined(WIN32)
const char ORIG_HOST[] = "iphonesubmissions.apple.com";
const char REDIR_IP[] = "127.0.0.1";

static void Tokenize(const std::string& str, std::vector<std::string>& tokens, const std::string& delimiters = " ") /*{{{*/
{
	std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
	std::string::size_type pos = str.find_first_of(delimiters, lastPos);

	while (std::string::npos != pos || std::string::npos != lastPos) {
		tokens.push_back(str.substr(lastPos, pos - lastPos));
		lastPos = str.find_first_not_of(delimiters, pos);
		pos = str.find_first_of(delimiters, lastPos);
	}
} /*}}}*/

static void checkHostsFile(const char* hosts_file) /*{{{*/
{
	std::ifstream ifs;
	ifs.open(hosts_file);
	if (ifs.fail()) {
		fprintf(stderr, "Could not open '%s' for reading\n", hosts_file);
		return;
	}

	std::vector<std::string> lines;
	std::string temp;
	while (getline(ifs, temp)) {
		lines.push_back(temp);
	}
	ifs.close();

	std::vector<std::string> newlines;
	bool write_required = false;
	bool found_it = false;
	unsigned int i;
	for (i = 0; i < lines.size(); i++) {
		std::string line = lines.at(i);
		if (line.compare(0, 1, "#") == 0) {
			// comment line
			if (line.find(ORIG_HOST) == std::string::npos) {
				// no match. add the line back
				newlines.push_back(line);
			} else {
				// found comment line with ORIG_HOST, don't add it, we'll add a new one later
				write_required = true;
			}
		} else {
			// normal line
			std::vector<std::string> tokens;
			Tokenize(line, tokens, "\t ");
			if (tokens.size() > 1) {
				if (tokens.at(1).compare(ORIG_HOST) == 0) {
					if (tokens.at(0).compare(REDIR_IP) == 0) {
						// entry already present!
						fprintf(stderr, "redirection of %s to %s already present. no change required.\n", tokens.at(1).c_str(), tokens.at(0).c_str());
						found_it = true;
					} else {
						// reset the ip to our redirection ip
						std::string str;
						str += REDIR_IP;
						str += "\t";
						str += ORIG_HOST;
						newlines.push_back(str);

						write_required = true;
						found_it = true;
					}
				} else {
					// just add the line back.
					newlines.push_back(line);
				}
			} else {
				// just add the line back.
				newlines.push_back(line);
			}
		}
	}

	if (!found_it) {
		// not found, add new line
		std::string str;
		str += REDIR_IP;
		str += "\t";
		str += ORIG_HOST;
		newlines.push_back(str);
		write_required = true;
	}
	if (write_required) {
		// write it
		std::ofstream ofs;
		ofs.open(hosts_file);
		if (ofs.is_open()) {
			for (i = 0; i < newlines.size(); i++) {
				ofs << newlines.at(i) << std::endl;
			}
			ofs.close();
		} else {
			fprintf(stderr, "Could not open '%s' for writing\n", hosts_file);
		}
	}
} /*}}}*/
#endif

class CDevReporter : public wxApp
{
	virtual bool OnInit();
};

bool CDevReporter::OnInit()
{
#if defined(__APPLE__) || defined(WIN32)
# if defined(__APPLE__)
	const char HOSTS_FILE[] = "/etc/hosts";
	const char HOSTS_UMBRELLA[] = "/etc/hosts.umbrella";
# endif
# if defined(WIN32)
	char HOSTS_FILE[512];
	char HOSTS_UMBRELLA[512];
	GetSystemDirectoryA(HOSTS_FILE, 512);
	strcat(HOSTS_FILE, "\\drivers\\etc\\hosts");
	strcpy(HOSTS_UMBRELLA, HOSTS_FILE);
	strcat(HOSTS_UMBRELLA, ".umbrella");

	DWORD attr1 = GetFileAttributesA(HOSTS_FILE);
	if (attr1 != INVALID_FILE_ATTRIBUTES) {
		SetFileAttributesA(HOSTS_FILE, FILE_ATTRIBUTE_NORMAL);
	}

	DWORD attr2 = GetFileAttributesA(HOSTS_UMBRELLA);
	if (attr2 != INVALID_FILE_ATTRIBUTES) {
		SetFileAttributesA(HOSTS_UMBRELLA, FILE_ATTRIBUTE_NORMAL);
	}
# endif

	checkHostsFile(HOSTS_FILE);
	checkHostsFile(HOSTS_UMBRELLA);

# if defined(__APPLE__)
	// now flush dns cache
	AuthorizationRef authorizationRef;
	AuthorizationCreate(NULL, kAuthorizationEmptyEnvironment, kAuthorizationFlagDefaults, &authorizationRef);

	char *tool = "/usr/bin/dscacheutil";
	char *args[] = {"-flushcache", NULL};

	AuthorizationExecuteWithPrivileges(authorizationRef, tool, kAuthorizationFlagDefaults, args, NULL);
	AuthorizationFree(authorizationRef, kAuthorizationFlagDestroyRights);
# endif
# if defined(WIN32)
	if (attr1 != INVALID_FILE_ATTRIBUTES) {
		SetFileAttributesA(HOSTS_FILE, attr1);
	}
	if (attr2 != INVALID_FILE_ATTRIBUTES) {
		SetFileAttributesA(HOSTS_UMBRELLA, attr2);
	}

	// now flush dns cache
	WinExec("ipconfig /flushdns", SW_HIDE);
# endif
#else
#warning hosts file checking/writing disabled
#endif

	CDRMainWnd* mainWnd = new CDRMainWnd();
	mainWnd->Show(true);
	SetTopWindow(mainWnd);

	return true;
}

IMPLEMENT_APP(CDevReporter)
