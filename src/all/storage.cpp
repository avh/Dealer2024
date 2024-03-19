// (c)2024, Arthur van Hoff, Artfahrt Inc.
#include <FS.h>
#include <LittleFS.h>
#define FORMAT_LITTLEFS_IF_FAILED true

#include "webserver.h"
#include "storage.h"

static void handle_listdir(HTTP &http, const char *path, int depth = 0)
{
    File root = LittleFS.open(path);
    if (!root){
        dprintf("storage: failed to open directory: '%s'", path);
        return;
    }
    if (!root.isDirectory()){
        dprintf("storage: not a directory: '%s'", path);
        return;
    }

    char buf[128];
    for (File file = root.openNextFile() ; file ; file = root.openNextFile()) {
        if (file.name()[0] != '.') {
            for (int i = 0 ; i < depth ; i++) {
                http.printf("  ");
            }
            if (file.isDirectory()) {
                http.printf("%s/\n", file.name());
                if (depth < 10) {
                    snprintf(buf, sizeof(buf), "%s%s/", path, file.name());
                    handle_listdir(http, buf, depth + 1);
                }
            } else {
                http.printf("%-20s %6d\n", file.name(), file.size());
            }
        }
    }
    root.close();
}

void Storage::init() 
{
    WebServer::add("/list", [](HTTP &http) {
        http.header(200, "List Follows");
        http.body();
        http.printf("--- listing ---\n");
        handle_listdir(http, "/");
        http.close();
    });

    if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)){
        dprintf("storage: LittleFS mount failed");
        return;
    }
    int percent = 100 * LittleFS.usedBytes() / LittleFS.totalBytes();
    dprintf("storage: LittleFS used=%luKB of %luKB (%d%%)", LittleFS.usedBytes()/1024, LittleFS.totalBytes()/1024, percent);    
    mounted = true;
}
