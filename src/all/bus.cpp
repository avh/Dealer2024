// (c)2024, Arthur van Hoff, Artfahrt Inc.
//
#include "bus.h"
#include <Wire.h>

//
// Client
//
static BusSlave *client = NULL;

static void handleReceiveCommand(int len) 
{
    client->interval = 0;
    client->reqlen = len;
    Wire.readBytes(client->req, client->reqlen);
    dprintf("bus: recv cmd=0x%02x, len=%d", client->req[0], client->reqlen);
}

static void handleRequestData() 
{
    if (client->reslen > 0) {
        Wire.write(client->res, client->reslen);
        client->reslen = 0;
        client->interval = 1000;
    } else {
        client->result_requested = true;
        client->interval = 0;
    }
}

void BusSlave::init()
{
    client = this;
    Wire.begin(addr);
    Wire.setBufferSize(32);
    Wire.onReceive(handleReceiveCommand);
    Wire.onRequest(handleRequestData);
}

void BusSlave::idle(unsigned long now)
{
    if (reqlen > 0) {
        dprintf("bus: got req=0x%02x, len=%d", req[0], reqlen);
        handler(*this);
        reqlen = 0;
    }
    if (result_requested) {
        Wire.write(res, reslen);
        reslen = 0;
        result_requested = false;
        interval = 1000;
    }
}

//
// BusMaster
//

void BusMaster::init()
{
    Wire.begin();
    Wire.setTimeOut(100);
}

bool BusMaster::check(uint8_t addr)
{
    Wire.beginTransmission(addr);
    int error = Wire.endTransmission();
    return error == 0;
}

bool BusMaster::request(uint8_t addr, const unsigned char *req, int reqlen, unsigned char *res, int reslen)
{
    static int last_error_addr = -1;
    Wire.beginTransmission(addr);
    Wire.write(req, reqlen);
    int error = Wire.endTransmission();
    if (error != 0) {
        if (last_error_addr != addr) {
            dprintf("Wire: 0x%02x, error=%d", addr, error);
            last_error_addr = addr;
            return false;
        }
    }
    if (res != NULL && reslen > 0) {
        int n = Wire.requestFrom(int(addr), reslen);
        if (n != reslen) {
            if (last_error_addr != addr) {
                dprintf("bus: 0x%02x, requested %d, got %d", addr, reslen, n);
                last_error_addr = addr;
            }
            for (int i = 0 ; i < n ; i++) {
                Wire.read();
            }
            last_error_addr = addr;
            return false;
        }
        while (!Wire.available()) {
            delayMicroseconds(1);
        }
        for (int i = 0 ; i < reslen ; i++) {
            res[i] = Wire.read();
        }
    }
    return true;
}