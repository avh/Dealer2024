// (c)2024, Arthur van Hoff, Artfahrt Inc.
//
#include "bus.h"
#include <Wire.h>

//
// Client
//
static BusSlave *client = NULL;

bool hasPower() 
{
    return digitalRead(POWER_PIN) == HIGH;
}

static void handleReceiveCommand(int len) 
{
    BusSlave::Buffer cmd(len);
    Wire.readBytes(cmd.data(), len);
    client->int_handler(*client, cmd, client->response);
    if (client->response.size() == 0) {    
        client->pending.push_back(cmd);
        client->interval = 0;
    }
}

static void handleRequestData() 
{
    if (client->response.size() > 0) {
        int n = Wire.write(client->response.data(), client->response.size());
        if (n != client->response.size()) {
            dprintf("bus: hdlreqd FAILED 0x%02x len=%d, wrote %d", client->response[0], client->response.size(), n);
        }
        client->response.resize(0);
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
    if (pending.size() > 0) {
        noInterrupts();
        Buffer cmd = pending[0];
        pending.erase(pending.begin());
        interval = pending.size() == 0 ? 1000 : 0;
        interrupts();
        cmd_handler(*this, cmd);
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
        if (last_error_addr != addr && hasPower()) {
            dprintf("bus: 0x%02x, error in endTransmission, %d", addr, error);
        }
        last_error_addr = addr;
        return false;
    } else {
        //dprintf("bus: 0x%02x, sent 0x%02x %d, reslen=%d", addr, req[0], reqlen, reslen);
    }
    if (res != NULL && reslen > 0) {
        //delay(5);
        int n = Wire.requestFrom(int(addr), reslen);
        if (n != reslen) {
            if (last_error_addr != addr && hasPower()) {
                dprintf("bus: 0x%02x, error, requested %d, got %d", addr, reslen, n);
            }
            last_error_addr = addr;
            for (int i = 0 ; i < reslen ; i++) {
                res[i] = 0;
            }
            for (int i = 0 ; i < n ; i++) {
                Wire.read();
            }
            return false;
        }
        //dprintf("reading %d bytes", reslen);
        Wire.readBytes(res, reslen);
        Wire.flush();
    }
    last_error_addr = -1;
    return true;
}