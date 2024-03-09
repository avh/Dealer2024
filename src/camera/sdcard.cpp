// (c)2024, Arthur van Hoff, Artfahrt Inc.
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include "webserver.h"
#include "sdcard.h"

void SDCard::init() 
{
    if (!SD.begin(21)){
        dprintf("SD: mount failed");
        return;
    }
    uint8_t cardType = SD.cardType();

    if (cardType == CARD_NONE){
        dprintf("SD: no card found");
        return;
    }
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    //dprintf("SD: mounted, %llu MB", cardSize);

    WebServer::add("/list", [](HTTP &http) {
        http.begin(200, "List Follows");
        http.end();
        http.printf("--- listing ---\n");
        handle_listdir(http, "/");
    });
}

void SDCard::handle_listdir(HTTP &http, const char *path, int depth)
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
                http.printf("%s\n", file.name());
            }
        }
    }
    root.close();
}
