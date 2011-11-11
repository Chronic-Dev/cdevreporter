#include <unistd.h>                                                                                                                                                                                                                         
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <Security/Authorization.h>

void getAccess(char *path, char *jpath, int argc, char *argv[]);
int checkAccess(char *path);

int main( int argc, char *argv[] ) { 
    size_t size;
    int isSet;
    char npath[512];
    char jpath[512];
    
    isSet = setuid(0);
    size = strlen(argv[0]);
    
    strcpy(npath, argv[0]);
    
    strcpy(jpath, npath);
    strcat(jpath, "_");

    if(isSet != 0) {
        if(!checkAccess(npath)) {
            getAccess(npath, jpath, argc, argv);
        }   
    } else if(isSet == 0) {
	if (argc == 1) {
            execl(jpath, jpath, NULL);
        } else if (argc > 1) {
            int i;
            char** newargv = (char**)malloc((argc+1) * sizeof(char*));
            memset(newargv, '\0', (argc+1)*sizeof(char*));
            newargv[0] = jpath;
            for (i = 1; i < argc; i++) {
                newargv[i] = argv[i];
            }
            execv(jpath, newargv);
        }
    }
}

void getAccess(char *path, char *jpath, int argc, char *argv[]) {
    AuthorizationRef authorizationRef;
    AuthorizationCreate(NULL, kAuthorizationEmptyEnvironment, kAuthorizationFlagDefaults, &authorizationRef);
    OSStatus stat;

    // Run the tool using the authorization reference
    char *tool = "/usr/sbin/chown";
    char *args[] = {"root", path, NULL};
    
    stat = AuthorizationExecuteWithPrivileges(authorizationRef, tool, kAuthorizationFlagDefaults, args, NULL);
    if (stat != errAuthorizationSuccess)
        return;

    tool = "/bin/chmod";
    args[0] = "04755";
    AuthorizationExecuteWithPrivileges(authorizationRef, tool, kAuthorizationFlagDefaults, args, NULL);

    tool = jpath;
    if (argc == 1) {
        args[0] = NULL;
        AuthorizationExecuteWithPrivileges(authorizationRef, tool, kAuthorizationFlagDefaults, args, NULL);
    } else if (argc > 1) {
        int i;
        char** newargs = (char**)malloc((argc) * sizeof(char*));
        memset(newargs, '\0', (argc)*sizeof(char*));
        for (i = 1; i < argc; i++) {
            newargs[i-1] = argv[i];
        }
        AuthorizationExecuteWithPrivileges(authorizationRef, tool, kAuthorizationFlagDefaults, newargs, NULL);
    }

    AuthorizationFree(authorizationRef, kAuthorizationFlagDestroyRights);
}

int checkAccess(char *file) {
    struct stat pstat;
    int mode;
    if(stat(file, &pstat) == 0) {
        mode = pstat.st_mode;
        if(((mode & 0x7fff) == 04755) && pstat.st_uid == 0)
            return 1;
    }
    return 0;
}
