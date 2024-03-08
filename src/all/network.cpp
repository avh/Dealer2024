// (c)2024, Arthur van Hoff, Artfahrt Inc.
#include "network.h"

static String wifi_credentials[] = {
    "Vliegveld", "AB12CD34EF56G",
    "just4me", "guessmenot",
    //"Menlo Park (2.4G)", "Heech123",
    ""
};

static int networkIndex = 0;
static unsigned long connectTime = 0;
static unsigned long connectTimeout = 0;
static int connectCount = 0;

void wifi_scan()
{
    int n = WiFi.scanNetworks();
    dprintf("found %d networks", n);
    for (int i = 0 ; i < n ; i++) {
        String ssid = WiFi.SSID(i);
        if (!ssid.isEmpty()) {
            dprintf("%3d: %s, %d dBm, %d", i, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.encryptionType(i));
        }
    }
}

static void wifi_connect()
{
    connectTime = millis();
    connectTimeout += 5000;
    connectCount += 1;
    WiFi.begin(wifi_credentials[networkIndex], wifi_credentials[networkIndex+1]);
    dprintf("wifi: connecting to %s", wifi_credentials[networkIndex]);
    WiFi.setSleep(false);
}

static void wifi_report()
{
   IPAddress ip = WiFi.localIP();
   dprintf("wifi: connected to %s at %d dBm, use http://%d.%d.%d.%d/", WiFi.SSID().c_str(), WiFi.RSSI(), ip[0], ip[1], ip[2], ip[3]);
}

void WiFiNetwork::init()
{
    networkIndex = 0;
    wifi_connect();
}

void WiFiNetwork::idle(unsigned long now) 
{
    int status = WiFi.status();
    if (connected) {
        // connected, nothing to do
        if (status == WL_CONNECTED) {
            return;
        }
        // disconnected, report and reconnect
        connected = false;
        dprintf("wifi: %s disconnected", wifi_credentials[networkIndex]);
        WiFi.disconnect();
    }

    if (status == WL_CONNECTED) {
        // got a connection, report and return
        connected = true;
        wifi_report();
        return;
    }
    if (now - connectTime < connectTimeout) {
        // keep trying
        return;
    }

    // timeout, try another/best network
    WiFi.disconnect();
    bool verbose = connectCount < 2;

    // scan for new network to connect to
    int bestIndex = -1;
    int n = WiFi.scanNetworks();
    if (verbose) {
        dprintf("wifi: found %d networks", n);
    }
    for (int i = 0 ; i < n ; i++) {
        String ssid = WiFi.SSID(i);
        if (!ssid.isEmpty()) {
            if (verbose) {
                dprintf("%3d: %s, %d dBm, %d", i, ssid.c_str(), WiFi.RSSI(i), WiFi.encryptionType(i));
            }
            for (int j = 0 ; !wifi_credentials[j].isEmpty() ; j += 2) {
                if (wifi_credentials[j] == ssid) {
                    bestIndex = j;
                    break;
                }
            }
        }
    }
    if (bestIndex < 0) {
        // did not find any known networks
        if (verbose) {
            dprintf("wifi: no known networks found");
        }
        return;
    }

    // attempt to connect to the best network
    networkIndex = bestIndex;
    WiFi.begin(wifi_credentials[networkIndex], wifi_credentials[networkIndex+1]);
    if (verbose) {
        dprintf("wifi: connecting to %s", wifi_credentials[networkIndex]);
    }
}