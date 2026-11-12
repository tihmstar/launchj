#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <termios.h>
#include <sys/clonefile.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <mach/mach.h>

#define true 1
#define false 0

kern_return_t bootstrap_register(mach_port_t, char *, mach_port_t);

typedef  void *posix_spawnattr_t;
typedef  void *posix_spawn_file_actions_t;
int posix_spawn(pid_t *, const char *,const posix_spawn_file_actions_t *,const posix_spawnattr_t *,char *const __argv[],char *const __envp[]);

//BEGIN libgeneral
#define cassure(a) do{ if ((a) == 0){err=__LINE__; goto error;} }while(0)
#define cretassure(cond, errstr ...) do{ if ((cond) == 0){err=__LINE__;error(errstr); goto error;} }while(0)
#define creterror(estr ...) do{error(estr);err=__LINE__; goto error; }while(0)
#define safeFree(ptr) ({if (ptr) {free(ptr); ptr=0;}})
#define safeFreeCustom(ptr,func) ({if (ptr) {func(ptr); ptr=0;}})
#define safeFreeConst(ptr) ({if(ptr){void *fbuf = (void*)ptr;ptr = 0; free(fbuf);}})
#define safeClose(fd) ({if (fd != -1) {close(fd); fd=-1;}})
//END libgeneral

#ifdef DEBUG
int debug(const char *str, ...){
    int ret = 0;
    char *buf = NULL;
    va_list ap;
    va_start(ap, str);
    vasprintf(&buf, str, ap);
    va_end(ap);
    ret = dprintf(1,"[DEBUG] %s\n",buf);
    safeFree(buf);
    return ret;
}
#else
#define debug(a...)
#endif

int info(const char *str, ...){
    int ret = 0;
    char *buf = NULL;
    va_list ap;
    va_start(ap, str);
    vasprintf(&buf, str, ap);
    va_end(ap);
    ret = dprintf(1,"%s\n",buf);
    safeFree(buf);
    return ret;
}

int warning(const char *str, ...){
    int ret = 0;
    char *buf = NULL;
    va_list ap;
    va_start(ap, str);
    vasprintf(&buf, str, ap);
    va_end(ap);
    ret = dprintf(1,"[WARNING] %s\n",buf);
    safeFree(buf);
    return ret;
}

int error(const char *str, ...){
    int ret = 0;
    char *buf = NULL;
    va_list ap;
    va_start(ap, str);
    vasprintf(&buf, str, ap);
    va_end(ap);
    ret = dprintf(1,"[ERROR] %s\n",buf);
    safeFree(buf);
    return ret;
}

int run(const char *cmd, char * const *args){
    int pid = 0;
    int retval = 0;
    char printbuf[0x1000] = {};
    for (char * const *a = args; *a; a++) {
        size_t csize = strlen(printbuf);
        if (csize >= sizeof(printbuf)) break;
        snprintf(printbuf+csize,sizeof(printbuf)-csize, "%s ",*a);
    }

    retval = posix_spawn(&pid, cmd, NULL, NULL, args, NULL);
    info("Execting: %s (posix_spawn returned: %d)",printbuf,retval);
    {
        int pidret = 0;
        info("waiting for '%s' to finish...",printbuf);
        retval = waitpid(pid, &pidret, 0);
        info("waitpid for '%s' returned: %d",printbuf,retval);
        return pidret;
    }
    return retval;
}

int loadDaemons(void){
  int err = 0;
  DIR *d = NULL;
  struct dirent *dir = NULL;
  cretassure(d = opendir("/Library/LaunchDaemons/"), "Failed to open dir with err=%d (%s)",errno,strerror(errno));

  while ((dir = readdir(d))) { //remove all subdirs and files
      if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) {
          continue;
      }
      char *pp = NULL;
      asprintf(&pp,"/Library/LaunchDaemons/%s",dir->d_name);

      {
        const char *args[] = {
          "/bin/launchctl",
          "load",
          pp,
          NULL
        };
        run(args[0],args);
      }
      safeFree(pp);
  }
  safeFreeCustom(d, closedir);
error:
  return err;
}

int main(int argc, char **argv){
  setvbuf(stdout, NULL, _IONBF, 0);
  info("Hello from jbloader!\n");
  // loadDaemons();
  {
    system("echo 'really jailbroken';ls /Library/LaunchDaemons | while read a; do launchctl load /Library/LaunchDaemons/$a; done; ls /etc/rc.d | while read a; do /etc/rc.d/$a; done;");
    //ssh workaround
    system("launchctl unload /Library/LaunchDaemons/com.openssh.sshd.plist;/usr/libexec/sshd-keygen-wrapper");
    system("(killall backboardd)&");
  }
  info("Bye from jbloader!\n");
  return 0;
}
