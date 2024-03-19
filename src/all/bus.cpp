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
    client->reqlen = len;
    Wire.readBytes(client->req, client->reqlen);
    client->int_handler(*client);
    if (client->reqlen > 0) {
        client->interval = 0;
    }
}

static void handleRequestData() 
{
    if (client->reslen > 0) {
        int n = Wire.write(client->res, client->reslen);
        if (n != client->reslen) {
            dprintf("bus: hdlreqd FAILED 0x%02x len=%d, wrote %d", client->res[0], client->reslen, n);
        }
        client->reslen = 0;
    } else {
        dprintf("bus: error, missing request data");
    }
}

void BusSlave::init()
{
    client = this;
    Wire.begin(addr);
    Wire.setTimeOut(1000);
    Wire.setBufferSize(128);
    Wire.onReceive(handleReceiveCommand);
    Wire.onRequest(handleRequestData);
}

void BusSlave::idle(unsigned long now)
{
    if (cmd_handler != NULL && reqlen > 0) {
        cmd_handler(*this);
        if (reslen > 0) {
            dprintf("bus: error response after command not allowed");
            reslen = 0;
        }
        interval = 1000;
        reqlen = 0;
    }
}

//
// BusMaster
//

void BusMaster::init()
{
    Wire.begin();
    Wire.setTimeOut(1000);
    Wire.setBufferSize(128);
}

bool BusMaster::check(uint8_t addr)
{
    Wire.beginTransmission(addr);
    int error = Wire.endTransmission();
    return error == 0;
}

bool BusMaster::request(uint8_t addr, const unsigned char *req, int reqlen, unsigned char *res, int reslen)
{
    if (res != NULL && reslen > 0) {
        bzero(res, reslen);
    }
    if (reqlen < 1) {
        dprintf("bus: error, request too short, reqlen=%d", reqlen);
        return false;
    }
    static int last_error_addr = -1;
    Wire.beginTransmission(addr);
    Wire.write(req, reqlen);
    int error = Wire.endTransmission();
    if (error != 0) {
        if (last_error_addr != addr) {
            dprintf("bus: 0x%02x, error in endTransmission, %d", addr, error);
            last_error_addr = addr;
            return false;
        }
    } else {
        //dprintf("bus: 0x%02x, sent 0x%02x %d, reslen=%d", addr, req[0], reqlen, reslen);
    }
    if (res != NULL && reslen > 0) {
        //delay(5);
        int n = Wire.requestFrom(int(addr), reslen);
        if (n != reslen) {
            if (last_error_addr != addr) {
                dprintf("bus: 0x%02x, error, requested %d, got %d", addr, reslen, n);
                last_error_addr = addr;
                for (int i = 0 ; i < reslen ; i++) {
                    res[i] = 0;
                }
            }
            for (int i = 0 ; i < n ; i++) {
                Wire.read();
            }
            last_error_addr = addr;
            return false;
        }
        Wire.readBytes(res, reslen);
        Wire.flush();
    }
    return true;
}