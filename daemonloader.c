#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <dlfcn.h>
#include <sys/stat.h>

#include <CoreFoundation/CoreFoundation.h>

#define cassure(a) do{ if ((a) == 0){err=__LINE__; goto error;} }while(0)
#define cretassure(cond, errstr ...) do{ if ((cond) == 0){err=__LINE__;printf(errstr); goto error;} }while(0)
#define creterror(errstr ...) do{printf(errstr);err=__LINE__; goto error; }while(0)
#define safeFree(ptr) ({if (ptr) {free(ptr); ptr=NULL;}})
#define safeFreeCustom(ptr,func) ({if (ptr) {func(ptr); ptr=NULL;}})
#define safeFreeCustomType(ptr,func,type) ({if (ptr) {func(ptr); ptr=(type)0LLU;}})
#define safeFreeConst(ptr) ({if(ptr){void *fbuf = (void*)ptr;ptr = NULL; free(fbuf);}})
#define safeClose(fd) ({if (fd != -1) {close(fd); fd=-1;}})
#define safeCloseCustom(fd,func) ({if (fd != -1) {func(fd); fd=-1;}})

typedef void* xpc_object_t;
typedef void* xpc_type_t;
typedef void* launch_data_t;
typedef bool (^xpc_dictionary_applier_t)(const char *key, xpc_object_t value);
typedef bool (^xpc_array_applier_t)(size_t index, xpc_object_t value);

xpc_object_t xpc_dictionary_create(const char * const *keys, const xpc_object_t *values, size_t count);
void xpc_dictionary_set_uint64(xpc_object_t dictionary, const char *key, uint64_t value);
void xpc_dictionary_set_string(xpc_object_t dictionary, const char *key, const char *value);
int64_t xpc_dictionary_get_int64(xpc_object_t dictionary, const char *key);
xpc_object_t xpc_dictionary_get_value(xpc_object_t dictionary, const char *key);
bool xpc_dictionary_get_bool(xpc_object_t dictionary, const char *key);
void xpc_dictionary_set_fd(xpc_object_t dictionary, const char *key, int value);
void xpc_dictionary_set_bool(xpc_object_t dictionary, const char *key, bool value);
const char *xpc_dictionary_get_string(xpc_object_t dictionary, const char *key);
void xpc_dictionary_set_value(xpc_object_t dictionary, const char *key, xpc_object_t value);
xpc_type_t xpc_get_type(xpc_object_t object);
bool xpc_dictionary_apply(xpc_object_t xdict, xpc_dictionary_applier_t applier);
int64_t xpc_int64_get_value(xpc_object_t xint);
char *xpc_copy_description(xpc_object_t object);
void xpc_dictionary_set_int64(xpc_object_t dictionary, const char *key, int64_t value);
const char *xpc_string_get_string_ptr(xpc_object_t xstring);
xpc_object_t xpc_array_create(const xpc_object_t *objects, size_t count);
xpc_object_t xpc_string_create(const char *string);
size_t xpc_dictionary_get_count(xpc_object_t dictionary);
void xpc_array_append_value(xpc_object_t xarray, xpc_object_t value);
xpc_object_t xpc_bool_create(bool value);
xpc_object_t xpc_double_create(double value);
xpc_object_t xpc_int64_create(int64_t value);
xpc_object_t xpc_data_create(const void *bytes, size_t length);
void xpc_release(xpc_object_t object);
bool xpc_bool_get_value(xpc_object_t xbool);
size_t xpc_data_get_length(xpc_object_t xdata);
bool xpc_array_apply(xpc_object_t xarray, xpc_array_applier_t applier);
#define xpc_array_append_string(arr,s) do {xpc_object_t o = xpc_string_create(s); xpc_array_append_value(arr, o); xpc_release(o);}while(0)

#define XPC_ARRAY_APPEND ((size_t)(-1))
#define XPC_ERROR_CONNECTION_INVALID XPC_GLOBAL_OBJECT(_xpc_error_connection_invalid)
#define XPC_ERROR_TERMINATION_IMMINENT XPC_GLOBAL_OBJECT(_xpc_error_termination_imminent)
#define XPC_TYPE_ARRAY (&_xpc_type_array)
#define XPC_TYPE_BOOL (&_xpc_type_bool)
#define XPC_TYPE_DICTIONARY (&_xpc_type_dictionary)
#define XPC_TYPE_ERROR (&_xpc_type_error)
#define XPC_TYPE_STRING (&_xpc_type_string)

extern const struct _xpc_dictionary_s _xpc_error_connection_invalid;
extern const struct _xpc_dictionary_s _xpc_error_termination_imminent;
extern const struct _xpc_type_s _xpc_type_array;
extern const struct _xpc_type_s _xpc_type_bool;
extern const struct _xpc_type_s _xpc_type_dictionary;
extern const struct _xpc_type_s _xpc_type_error;
extern const struct _xpc_type_s _xpc_type_string;

#define DYLD_INTERPOSE(_replacment,_replacee) \
__attribute__((used)) static struct{ const void* replacment; const void* replacee; } _interpose_##_replacee \
__attribute__ ((section ("__DATA,__interpose"))) = { (const void*)(unsigned long)&_replacment, (const void*)(unsigned long)&_replacee };

static xpc_object_t cf_to_xpc(CFTypeRef cfObj) {
    if (!cfObj) return NULL;
    CFTypeID typeID = CFGetTypeID(cfObj);

    if (typeID == CFStringGetTypeID()) {
        CFStringRef cfStr = (CFStringRef)cfObj;
        CFIndex len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfStr), kCFStringEncodingUTF8) + 1;
        char *buf = malloc(len);
        if (!buf) return NULL;
        
        xpc_object_t xpcStr = NULL;
        if (CFStringGetCString(cfStr, buf, len, kCFStringEncodingUTF8)) {
            xpcStr = xpc_string_create(buf);
        }
        free(buf);
        return xpcStr;
    }
    else if (typeID == CFBooleanGetTypeID()) {
        return xpc_bool_create(CFBooleanGetValue((CFBooleanRef)cfObj));
    }
    else if (typeID == CFNumberGetTypeID()) {
        CFNumberRef cfNum = (CFNumberRef)cfObj;
        if (CFNumberIsFloatType(cfNum)) {
            double val = 0;
            CFNumberGetValue(cfNum, kCFNumberDoubleType, &val);
            return xpc_double_create(val);
        } else {
            int64_t val = 0;
            CFNumberGetValue(cfNum, kCFNumberSInt64Type, &val);
            return xpc_int64_create(val);
        }
    }
    else if (typeID == CFDataGetTypeID()) {
        CFDataRef cfData = (CFDataRef)cfObj;
        return xpc_data_create(CFDataGetBytePtr(cfData), CFDataGetLength(cfData));
    }
    else if (typeID == CFArrayGetTypeID()) {
        CFArrayRef cfArr = (CFArrayRef)cfObj;
        xpc_object_t xpcArr = xpc_array_create(NULL, 0);
        CFIndex count = CFArrayGetCount(cfArr);
        
        for (CFIndex i = 0; i < count; i++) {
            xpc_object_t item = cf_to_xpc(CFArrayGetValueAtIndex(cfArr, i));
            if (item) {
                xpc_array_append_value(xpcArr, item);
                xpc_release(item);
            }
        }
        return xpcArr;
    }
    else if (typeID == CFDictionaryGetTypeID()) {
        CFDictionaryRef cfDict = (CFDictionaryRef)cfObj;
        xpc_object_t xpcDict = xpc_dictionary_create(NULL, NULL, 0);
        CFIndex count = CFDictionaryGetCount(cfDict);
        
        if (count > 0) {
            const void **keys = malloc(sizeof(void *) * count);
            const void **values = malloc(sizeof(void *) * count);
            if (keys && values) {
                CFDictionaryGetKeysAndValues(cfDict, keys, values);
                for (CFIndex i = 0; i < count; i++) {
                    if (CFGetTypeID(keys[i]) == CFStringGetTypeID()) {
                        CFStringRef cfKey = (CFStringRef)keys[i];
                        CFIndex kLen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfKey), kCFStringEncodingUTF8) + 1;
                        char *kBuf = malloc(kLen);
                        if (kBuf) {
                            if (CFStringGetCString(cfKey, kBuf, kLen, kCFStringEncodingUTF8)) {
                                xpc_object_t xpcVal = cf_to_xpc(values[i]);
                                if (xpcVal) {
                                    xpc_dictionary_set_value(xpcDict, kBuf, xpcVal);
                                    xpc_release(xpcVal);
                                }
                            }
                            free(kBuf);
                        }
                    }
                }
            }
            free(keys);
            free(values);
        }
        return xpcDict;
    }

    return NULL;
}

xpc_object_t my_xpc_dictionary_get_value(xpc_object_t dict, const char *key) {
    xpc_object_t retval = xpc_dictionary_get_value(dict, key);
    if (strcmp(key, "LaunchDaemons") == 0) {
        {
            int err = 0;
            DIR *dir = NULL;
            struct dirent *ent = NULL;
            dir = opendir("/Library/LaunchDaemons/");
            if (dir){
                while ((ent = readdir(dir)) != NULL) {
                    size_t nameLen = strlen(ent->d_name);
                    if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) continue;
                    if (nameLen < 6 || strcmp(".plist", ent->d_name + nameLen - 6) != 0) continue;

                    char fullPath[PATH_MAX];
                    snprintf(fullPath, sizeof(fullPath), "/Library/LaunchDaemons/%s", ent->d_name);

                    FILE *fp = fopen(fullPath, "rb");
                    if (!fp) continue;

                    fseek(fp, 0, SEEK_END);
                    long fsize = ftell(fp);
                    fseek(fp, 0, SEEK_SET);

                    if (fsize > 0) {
                        uint8_t *buf = malloc(fsize);
                        if (buf && fread(buf, 1, fsize, fp) == (size_t)fsize) {
                            CFDataRef data = CFDataCreateWithBytesNoCopy(
                                kCFAllocatorDefault, buf, fsize, kCFAllocatorNull
                            );
                            if (data) {
                                CFPropertyListRef plist = CFPropertyListCreateWithData(
                                    kCFAllocatorDefault, data, kCFPropertyListImmutable, NULL, NULL
                                );
                                if (plist) {
                                    if (CFGetTypeID(plist) == CFDictionaryGetTypeID()) {
                                        xpc_object_t submitJob = cf_to_xpc(plist);
                                        if (submitJob) {
                                            xpc_dictionary_set_value(retval, fullPath, submitJob);
                                            xpc_release(submitJob);
                                        }
                                    }
                                    CFRelease(plist);
                                }
                                CFRelease(data);
                            }
                        }
                        free(buf);
                    }
                    fclose(fp);
                }
            }
            safeFreeCustom(dir, closedir);
        }
        {
            //MobileSubstrate workaround!
            struct stat st = {};
            if (!stat("/Library/Frameworks/CydiaSubstrate.framework/Libraries/SubstrateLauncher.dylib", &st)){
                dlopen("/Library/Frameworks/CydiaSubstrate.framework/Libraries/SubstrateLauncher.dylib", RTLD_NOW);
            }
        }
        // Block OTA updates
        xpc_dictionary_set_value(retval, "/System/Library/LaunchDaemons/com.apple.mobile.softwareupdated.plist", NULL);
    } else if (strcmp(key, "SystemXPCService") == 0 || strcmp(key, "Daemon") == 0) {
        /*
            Allow our daemons to execute (jetsam policy)
        */
        xpc_object_t def = xpc_dictionary_get_value(retval, "Default");
        if (def){
            xpc_object_t global = xpc_dictionary_get_value(def, "Global");
            if (global) xpc_dictionary_set_bool(global, "ExecuteAllowed", true);
        }
    } else if (strcmp(key, "Paths") == 0) {
        /*
            Add loading LaunchDaemons from /Library/LaunchDaemons
        */
        static bool sDidRun = false;
        if (!sDidRun){
            sDidRun = true;
            xpc_array_append_string(retval, "/Library/LaunchDaemons");
        }
    }

    return retval;
}
DYLD_INTERPOSE(my_xpc_dictionary_get_value, xpc_dictionary_get_value);

__attribute__((constructor))
static void customConstructor(int argc, const char **argv) {
    if (getpid() == 1) {
        int fd_console = open("/dev/console", O_RDWR, 0);
        dprintf(fd_console,"================ Hello from dylib ================ \n");
        dprintf(fd_console,"========= Goodbye from dylib constructor ========= \n");
        close(fd_console);
    }
}
