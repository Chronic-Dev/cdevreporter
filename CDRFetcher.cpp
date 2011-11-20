#include "CDRFetcher.h"
#include <cstdio>
#include <vector>
#include <dirent.h>
#include <errno.h>
#include <zlib.h>
#include <curl/curl.h>

#ifdef WIN32
#include <windows.h>
#define sleep(x) Sleep(x*1000);
// win32 is stupid.
#define S_IFLNK S_IFREG
#define S_IFSOCK S_IFREG
#endif

/* mkdir helper */
int __mkdir(const char* path, int mode) /*{{{*/
{
#ifdef WIN32
	return mkdir(path);
#else
	return mkdir(path, mode);
#endif
} /*}}}*/

/* recursively remove path, including path */
static void rmdir_recursive(const char *path) /*{{{*/
{
	if (!path) {
		return;
	}
	DIR* cur_dir = opendir(path);
	if (cur_dir) {
		struct dirent* ep;
		while ((ep = readdir(cur_dir))) {
			if ((strcmp(ep->d_name, ".") == 0) || (strcmp(ep->d_name, "..") == 0)) {
				continue;
			}
			char *fpath = (char*)malloc(strlen(path)+1+strlen(ep->d_name)+1);
			if (fpath) {
				struct stat st;
				strcpy(fpath, path);
				strcat(fpath, "/");
				strcat(fpath, ep->d_name);

				if ((stat(fpath, &st) == 0) && S_ISDIR(st.st_mode)) {
					rmdir_recursive(fpath);
				} else {
					if (remove(fpath) != 0) {
						fprintf(stderr, "could not remove file %s: %s\n", fpath, strerror(errno));
					}
				}
				free(fpath);
			}
		}
		closedir(cur_dir);
	}
	if (rmdir(path) != 0) {
		fprintf(stderr, "could not remove directory %s: %s\n", path, strerror(errno));
	}
} /*}}}*/

/* char** freeing helper function */
static void free_dictionary(char **dictionary) /*{{{*/
{
	int i = 0;

	if (!dictionary)
		return;

	for (i = 0; dictionary[i]; i++) {
		free(dictionary[i]);
	}
	free(dictionary);
} /*}}}*/

/* get file info for afc path */
static afc_error_t afc_get_file_attr(afc_client_t afc, const char* path, struct stat* stbuf) /*{{{*/
{
	int i;
	char** info = NULL;

	afc_error_t ret = afc_get_file_info(afc, path, &info);
	memset(stbuf, 0, sizeof(struct stat));
	if (ret != AFC_E_SUCCESS) {
		return ret;
	} else if (!info) {
		return AFC_E_UNKNOWN_ERROR;
	}

	// get file attributes from info list
	for (i = 0; info[i]; i += 2) {
		if (!strcmp(info[i], "st_size")) {
			stbuf->st_size = atoll(info[i+1]);
		} else if (!strcmp(info[i], "st_blocks")) {
#ifndef WIN32
			stbuf->st_blocks = atoi(info[i+1]);
#endif
		} else if (!strcmp(info[i], "st_ifmt")) {
			if (!strcmp(info[i+1], "S_IFREG")) {
				stbuf->st_mode = S_IFREG;
			} else if (!strcmp(info[i+1], "S_IFDIR")) {
				stbuf->st_mode = S_IFDIR;
			} else if (!strcmp(info[i+1], "S_IFLNK")) {
				stbuf->st_mode = S_IFLNK;
			} else if (!strcmp(info[i+1], "S_IFBLK")) {
				stbuf->st_mode = S_IFBLK;
			} else if (!strcmp(info[i+1], "S_IFCHR")) {
				stbuf->st_mode = S_IFCHR;
			} else if (!strcmp(info[i+1], "S_IFIFO")) {
				stbuf->st_mode = S_IFIFO;
			} else if (!strcmp(info[i+1], "S_IFSOCK")) {
				stbuf->st_mode = S_IFSOCK;
			}
		} else if (!strcmp(info[i], "st_nlink")) {
			stbuf->st_nlink = atoi(info[i+1]);
		} else if (!strcmp(info[i], "st_mtime")) {
			stbuf->st_mtime = (time_t)(atoll(info[i+1]) / 1000000000);
		}
	}
	free_dictionary(info);

	return ret;
} /*}}}*/

/* recursively remove path via afc, NOT including path */
static int rmdir_recursive_afc(afc_client_t afc, const char *path) /*{{{*/
{
	char **dirlist = NULL;
	if (afc_read_directory(afc, path, &dirlist) != AFC_E_SUCCESS) {
		fprintf(stderr, "AFC: could not get directory list for %s\n", path);
		return -1;
	}
	if (dirlist == NULL) {
		return 0;
	}

	char **ptr;
	for (ptr = dirlist; *ptr; ptr++) {
		if ((strcmp(*ptr, ".") == 0) || (strcmp(*ptr, "..") == 0)) {
			continue;
		}
		char **info = NULL;
		char *fpath = (char*)malloc(strlen(path)+1+strlen(*ptr)+1);
		strcpy(fpath, path);
		strcat(fpath, "/");
		strcat(fpath, *ptr);
		if ((afc_get_file_info(afc, fpath, &info) != AFC_E_SUCCESS) || !info) {
			// failed. try to delete nevertheless.
			afc_remove_path(afc, fpath);
			free(fpath);
			free_dictionary(info);
			continue;
		}

		int is_dir = 0;
		int i;
		for (i = 0; info[i]; i+=2) {
			if (!strcmp(info[i], "st_ifmt")) {
				if (!strcmp(info[i+1], "S_IFDIR")) {
					is_dir = 1;
				}
				break;
			}
		}
		free_dictionary(info);

		if (is_dir) {
			rmdir_recursive_afc(afc, fpath);
		}
		afc_remove_path(afc, fpath);
		free(fpath);
	}

	free_dictionary(dirlist);

	return 0;
} /*}}}*/

/* gzip the files directly from afc */
static int gzip_afc_dir_recursive(afc_client_t afc, const char* path, const char* tempdir, std::vector<char*>* filenames) /*{{{*/
{
	char** list = NULL;
	int i = 0;
	uint64_t handle;
	struct stat st;
	char fullname[512];
	char zname[512];
	afc_error_t afc_error;

	afc_error = afc_read_directory(afc, path, &list);
	if(afc_error != AFC_E_SUCCESS) {
		fprintf(stderr, "ERROR: Could not read directory contents on device\n");
		return -1;
	}

	strcpy(fullname, path);
	if (fullname[strlen(fullname)-1] != '/') {
		strcat(fullname, "/");
	}
	int spos = strlen(fullname);

	strcpy(zname, tempdir);
	if (zname[strlen(zname)-1] != '/') {
		strcat(zname, "/");
	}
	int zpos = strlen(zname);

	for (i = 0; list[i] != NULL; i++) {
		char* entry = list[i];
		if (!strcmp(entry, ".") || !strcmp(entry, "..")) continue;
		strcpy(((char*)fullname)+spos, entry);

		if (afc_get_file_attr(afc, fullname, &st) != AFC_E_SUCCESS) continue;
		if (S_ISDIR(st.st_mode)) {
			if (gzip_afc_dir_recursive(afc, fullname, tempdir, filenames) < 0) {
				break;
			}
		} else if (S_ISREG(st.st_mode)) {
			if (st.st_size > 1048576) {
				fprintf(stderr, "skipping file %s (> 1 MB)\n", fullname);
				continue;
			} else if (st.st_size == 0) {
				fprintf(stderr, "skipping empty file %s\n", fullname);
				continue;
			}

			handle = 0;
			afc_error = afc_file_open(afc, fullname, AFC_FOPEN_RDONLY, &handle);
			if(afc_error != AFC_E_SUCCESS) {
				fprintf(stderr, "Unable to open %s (%d)\n", fullname, afc_error);
				continue;
			}

			strcpy(zname+zpos, entry);
			strcat(zname, ".gz");

			gzFile zf = gzopen(zname, "wb");

			uint32_t bytes_read = 0;
			uint32_t total = 0;
			unsigned char data[0x1000];
		
			afc_error = afc_file_read(afc, handle, (char*)data, 0x1000, &bytes_read);
			while ((afc_error == AFC_E_SUCCESS) && (bytes_read > 0)) {
				gzwrite(zf, data, bytes_read);
				total += bytes_read;
				afc_error = afc_file_read(afc, handle, (char*)data, 0x1000, &bytes_read);
			}
			afc_file_close(afc, handle);
			gzclose(zf);
			if ((uint32_t)st.st_size != total) {
				fprintf(stderr, "file size mismatch?! skipping.\n");
				continue;
			}
			filenames->push_back(strdup(zname));
		}
	}
	return 0;
} /*}}}*/

typedef struct {
	int length;
	char* content;
} response_t;

size_t upload_write_callback(char* data, size_t size, size_t nmemb, response_t* response) {
	size_t total = size * nmemb;
	if (total != 0) {
		response->content = (char*)realloc((void*)response->content, response->length + total + 1);
		memcpy(response->content + response->length, data, total);
		response->content[response->length + total] = '\0';
		response->length += total;
	}

	return total;
}

size_t upload_read_callback(char *bufptr, size_t size, size_t nitems, void *userp)
{
	FILE* f = (FILE*)userp;
	//fprintf(stderr, "callback requested %d*%d=%d bytes\n", size, nitems, size*nitems);
	size_t r = fread(bufptr, 1, size*nitems, f);
	//fprintf(stderr, "and we read: %d\n", r);
	return r;
}

static int upload_files(std::vector<char*>* filenames)
{
	unsigned int i = 0;

	curl_global_init(CURL_GLOBAL_ALL);
	response_t* response = NULL;
	CURL* handle = NULL;

	while (i < filenames->size()) {
		handle = curl_easy_init();
		if (!handle) {
			fprintf(stderr, "curl_easy_init() failed\n");
			return -1;
		}

		response = (response_t*)malloc(sizeof(response_t));
		if (response == NULL) {
			fprintf(stderr, "Unable to allocate sufficent memory\n");
			return -1;
		}

		response->length = 0;
		response->content = (char*)malloc(1);

		struct curl_httppost *pd = NULL;
		struct curl_httppost *last = NULL;
		int num = 0;
		char fnam[10];
		while ((num < 10) && (i < filenames->size())) {
			sprintf(fnam, "file%04x", i);
			curl_formadd(&pd, &last, CURLFORM_COPYNAME, fnam, CURLFORM_FILE, filenames->at(i), CURLFORM_CONTENTTYPE, "application/x-gzip", CURLFORM_END);
			num++;
			i++;
		}

		curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, (curl_write_callback)&upload_write_callback);
		curl_easy_setopt(handle, CURLOPT_WRITEDATA, response);
		curl_easy_setopt(handle, CURLOPT_HTTPPOST, pd);
		curl_easy_setopt(handle, CURLOPT_FAILONERROR, 1);
		curl_easy_setopt(handle, CURLOPT_TIMEOUT, 150L);
		curl_easy_setopt(handle, CURLOPT_USERAGENT, "InetURL/1.0");
		curl_easy_setopt(handle, CURLOPT_URL, POST_UPLOAD_URL);

		fprintf(stderr, "sending %d files now\n", num);
		curl_easy_perform(handle);

		curl_formfree(pd);
		curl_easy_cleanup(handle);
		handle = NULL;

		if (strstr(response->content, "SUCCESS") == NULL) {
			if (response->content && response->length>0) {
				fprintf(stderr, "%s\n", response->content);
			}
			free(response->content);
			free(response);
			fprintf(stderr, "failed to upload stuff\n");
			return -1;
		}
		fprintf(stderr, "sent %d files\n", num);
	}
	curl_global_cleanup();

#if 0
	struct stat st;

	if (stat(filename, &st) != 0) {
		fprintf(stderr, "stat(%s) failed", filename);
		return -1;
	}

	FILE* file = fopen(filename, "rb");
	if (!file) {
		fprintf(stderr, "could not open file for reading\n");
		return -1;
	}

	curl_global_init(CURL_GLOBAL_ALL);
	response_t* response = NULL;
	CURL* handle = curl_easy_init();
	if (!handle) {
		fprintf(stderr, "curl_easy_init() failed\n");
		fclose(file);
		return -1;
	}
	struct curl_slist* header = NULL;
	header = curl_slist_append(header, "Cache-Control: no-cache");
	header = curl_slist_append(header, "Content-type: application/octet-stream");

	response = (response_t*)malloc(sizeof(response_t));
	if (response == NULL) {
		fprintf(stderr, "Unable to allocate sufficent memory\n");
		fclose(file);
		return -1;
	}

	response->length = 0;
	response->content = (char*)malloc(1);

	curl_easy_setopt(handle, CURLOPT_READFUNCTION, (curl_read_callback)&upload_read_callback);
	curl_easy_setopt(handle, CURLOPT_READDATA, file);
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, (curl_write_callback)&upload_write_callback);
	curl_easy_setopt(handle, CURLOPT_INFILESIZE_LARGE, st.st_size);
	curl_easy_setopt(handle, CURLOPT_UPLOAD, 1L); 
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, response);
	curl_easy_setopt(handle, CURLOPT_HTTPHEADER, header);
	curl_easy_setopt(handle, CURLOPT_USERAGENT, "InetURL/1.0");
	curl_easy_setopt(handle, CURLOPT_URL, POST_UPLOAD_URL);
	curl_easy_perform(handle);
	fclose(file);
	curl_slist_free_all(header);
	curl_easy_cleanup(handle);

	curl_global_cleanup();

	if (strstr(response->content, "SUCCESS") == NULL) {
		if (response->content && response->length>0) {
			fprintf(stderr, "%s\n", response->content);
		}
		free(response->content);
		free(response);
		fprintf(stderr, "failed to upload stuff\n");
		return -1;
	}
#endif
	return 0;
}

wxThread::ExitCode CDRFetcher::Entry(void)
{
	afc_client_t afc = NULL;
	idevice_t device = NULL;
	lockdownd_client_t lockdownd = NULL;
	idevice_connection_t connection = NULL;
	afc_error_t afc_error = AFC_E_SUCCESS;
	idevice_error_t device_error = IDEVICE_E_SUCCESS;
	lockdownd_error_t lockdownd_error = LOCKDOWN_E_SUCCESS;
	unsigned short port = 0;
	int num_files = 0;
	char buf[256];
	std::vector<char*> filenames;

	char tmpf[256];
	tmpf[0] = '\0';
	char* error = NULL;

	if (!tmpnam(tmpf) || (tmpf[0] == '\0')) {
		error = "ERROR: Could not get temporary filename";
		goto leave;
	}

	if (__mkdir(tmpf, 0755) != 0) {
		error = "ERROR: Could not make temporary directory";
		goto leave;
	}

	device_error = idevice_new(&device, NULL);
	if (device_error != IDEVICE_E_SUCCESS) {
		error = "ERROR: opening device failed";
		goto cleanup;
	}

	lockdownd_error = lockdownd_client_new_with_handshake(device, &lockdownd, "idevicecrashreport");
	if (lockdownd_error != LOCKDOWN_E_SUCCESS) {
		error = "ERROR: Lockdown connection failed"; 
		goto cleanup;
	}

	port = 0;
	lockdownd_error = lockdownd_start_service(lockdownd, "com.apple.crashreportmover", &port);
	if (lockdownd_error != LOCKDOWN_E_SUCCESS) {
		error = "ERROR: Could not start crash report mover service";
		goto cleanup;
	}

	device_error = idevice_connect(device, port, &connection);
	if(device_error != IDEVICE_E_SUCCESS) {
		error = "ERROR: Could connect to move crash reports";
		goto cleanup;
	}
	idevice_disconnect(connection);

	worker->processStatus("Looking for crash reports...");

	/* just in case we have a slow device */
	sleep(2);

	port = 0;
	lockdownd_error = lockdownd_start_service(lockdownd, "com.apple.crashreportcopymobile", &port);
	if (lockdownd_error != LOCKDOWN_E_SUCCESS) {
		error = "ERROR: Could not start crash report copy service";
		goto cleanup;
	}
	lockdownd_client_free(lockdownd);
	lockdownd = NULL;

	afc = NULL;
	afc_error = afc_client_new(device, port, &afc);
	if (afc_error != AFC_E_SUCCESS) {
		error = "ERROR: Could not start AFC service";
		goto cleanup;
	}

	if (gzip_afc_dir_recursive(afc, "/", tmpf, &filenames) < 0) {
		error = "ERROR: Could not copy crash reports";
		goto cleanup;
	}

	num_files = filenames.size();

	// TODO: remove crash reports from device
	
	afc_client_free(afc);
	afc = NULL;
	idevice_free(device);
	device = NULL;

	sprintf(buf, "Got %d crash reports. Uploading now...\n", num_files);
	worker->processStatus(buf);

	if (upload_files(&filenames) < 0) {
		error = "ERROR: Could not upload crash reports!";
		goto cleanup;
	}

cleanup:
	if (afc) {
		afc_client_free(afc);
	}
	if (lockdownd) {
		lockdownd_client_free(lockdownd);
	}
	if (device) {
		idevice_free(device);
	}
	rmdir_recursive(tmpf);
	filenames.clear();

leave:
	worker->processFinished(error);
	return 0;
}
