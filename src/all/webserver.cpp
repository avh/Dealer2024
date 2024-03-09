// (c)2024, Arthur van Hoff, Artfahrt Inc.
#include <SD.h>

#include "webserver.h"

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

//
// HTTP Server
//

static struct {
  const char *path;
  void (*handler)(HTTP &);
} handlers[MAX_HANDLERS];
static int nhandlers = 0;

void WebServer::init()
{
    networkIndex = 0;
#if USE_WIFI
    wifi_connect();
#endif
}

void WebServer::idle(unsigned long now) 
{
#if USE_WIFI
    int status = WiFi.status();
    if (connected && status == WL_CONNECTED) {
        // connected, check for traffic
        WiFiClient client = server.available();
        if (client) {
            handle(client);
            client.stop();
        }
        return;
    }
    if (connected) {
        // disconnected, report and reconnect
        connected = false;
        dprintf("wifi: %s disconnected", wifi_credentials[networkIndex]);
        WiFi.disconnect();
        server.end();
    }

    if (status == WL_CONNECTED) {
        // got a connection, report and return
        connected = true;
        wifi_report();
        server.begin();
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
#endif
}

void file_handler(HTTP &http)
{
  const char *path = http.path.c_str();
  if (!SD.exists(path)) {
    return;
  }
  File file = SD.open(path);
  if (!file) {
    return;
  }
  int content_length = file.size();

  const char *content_type = "application/octet-stream";
  if (http.path.endsWith(".html")) {
    content_type = "text/html";
  } else if (http.path.endsWith(".txt")) {
    content_type = "text/plain";
  } else if (http.path.endsWith(".png")) {
    content_type = "image/png";
  } else if (http.path.endsWith(".jpg")) {
    content_type = "image/jpeg";
  } else if (http.path.endsWith(".bmp")) {
    content_type = "image/bmp";
  }

  int buflen = 10*1024;
  auto buf = std::shared_ptr<unsigned char[]>(new unsigned char[buflen]);
  if (buf == NULL) {
    http.begin(404, "Out of Memory");
    return;
  }

  http.begin(200, "File Follows");
  http.printf("Content-Length: %d\n", content_length);
  http.printf("Content-Type: %s\n", content_type);
  http.end();

  dprintf("%s: sending %d bytes of %s", http.path.c_str(), content_length, content_type);

  for (; file.available() ;) {
    int n = file.readBytes((char *)buf.get(), buflen);
    if (n <= 0) {
      break;
    }
    http.write(buf.get(), n);
  }
  file.close();
}

void WebServer::handle(WiFiClient &client)
{
  String currentLine = "";
  String method = "";
  String path = "";
  while (client.connected()) {
    if (client.available()) {
      char c = client.read();
      //Serial.write(c);
      if (c == '\n') {
        if (currentLine.length() == 0) {
          WebServer::dispatch(client, method, path);
          return;
        }
        // header line
        if (method == "") {
          int i = currentLine.indexOf(' ');
          method = currentLine.substring(0, i);
          path = currentLine.substring(i+1, currentLine.indexOf(' ', i+1));
        }
        currentLine = "";
      } else if (c != '\r') {
        currentLine += c;
      }
    }
  }
}

void WebServer::dispatch(WiFiClient &client, String &method, String &path)
{
  HTTP http(client, method, path);

  for (int i = 0 ; i < nhandlers ; i++) {
    if (path.equals(handlers[i].path)) {
      handlers[i].handler(http);
    }
  }
  if (http.state == 0) {
    file_handler(http);
  }
  if (http.state == 0) {
    http.begin(404, "File Not Found");
  }
  if (http.state == 1) {
    http.end();
  }
}

void WebServer::add(const char *path, void (*handler)(HTTP &))
{
#if USE_WIFI
  if (nhandlers < MAX_HANDLERS) {
    handlers[nhandlers].path = path;
    handlers[nhandlers].handler = handler;
    nhandlers++;
  } else {
    dprintf("ERROR: too many HTTP handlers (max=%d)", MAX_HANDLERS);
  }
#endif
}
//
// HTTP
//


void HTTP::begin(int code, const char *msg)
{
  state = 1;
  printf("HTTP/1.1 %d %s\n", code, msg);
}

void HTTP::printf(const char *fmt, ...)
{
  char buf[128];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  client.print(buf);
}

void HTTP::end()
{
  if (state == 1) {
    client.println("Connection: close");
    client.println();
  }
  state = method == "HEAD" ? 4 : 2;
}

int HTTP::write(unsigned char *buf, int len)
{
  switch (state) {
    case 0:
      begin(200, "File Follows");
    case 1:
      end();
    case 2:
      state = 3;
    case 3:
      break;
    case 4:
      return len;
  }
  for (int off = 0 ; off < len ; ) {
    int n = client.write(buf + off, min(10*1024, len - off));
    if (n < 0) {
      state = 4;
      return off;
    }
    off += n;
  }
  return len;
}
