// (c)2024, Arthur van Hoff, Artfahrt Inc.
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include "webserver.h"
#include "sdcard.h"

static void handle_listdir(HTTP &http, const char *path, int depth = 0)
{
    File root = SD.open(path);
    if (!root){
        dprintf("SD: failed to open directory: '%s'", path);
        return;
    }
    if (!root.isDirectory()){
        dprintf("SD: not a directory: '%s'", path);
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

void SDCard::init() 
{
    WebServer::add("/list", [](HTTP &http) {
        http.header(200, "List Follows");
        http.body();
        http.printf("--- listing ---\n");
        handle_listdir(http, "/");
        http.close();
    });

    for (int i = 0 ; !SD.begin(21) ; i++){
        dprintf("SD: mount failed, attempt=%d/5", i);
        delay(i*200);
        if (i > 5) {
            return;
        }
    }
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE){
        dprintf("SD: no card found");
        return;
    }
    //uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    //dprintf("SD: mounted, %llu MB", cardSize);
    mounted = true;
}
