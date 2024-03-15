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

struct Handler {
  const char *path;
  void (*handler)(HTTP &);
};
static std::vector<Handler> handlers;

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
    static bool listed_handlers = false;
    if (!listed_handlers) {
      listed_handlers = true;
      dprintf("wifi: %d handlers", handlers.size());
      for (struct Handler h: handlers) {
        dprintf("wifi: handler %s", h.path);
      }
    }

    int status = WiFi.status();
    if (connected && status == WL_CONNECTED) {
        // connected, check for traffic
        WiFiClient client = server.available();
        if (client) {
          //dprintf("wifi: new client");
          active.push_back(std::unique_ptr<HTTP>(new HTTP(client)));
        }
        for (auto it = active.begin() ; it != active.end() ; ) {
          if ((*it)->idle(this, now) != 0) {
            it = active.erase(it);
            //dprintf("http: remove client");
          } else {
            it++;
          }
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

void WebServer::add(const char *path, void (*handler)(HTTP &))
{
#if USE_WIFI
  Handler h = { path, handler };
  handlers.push_back(h);
#endif
}

//
// HTTP
//

void WebServer::file_put_handler(HTTP &http)
{
  if (http.method != "PUT") {
    http.header(405, "Method Not Allowed");
    http.close();
    return;
  }
  unsigned long clen = http.hdrs.count("content-length") ? atoi(http.hdrs["content-length"].c_str()) : -1;
  if (clen < 0) {
    http.header(411, "Length Required");
    http.close();
    return;
  }
  const char *path = http.path.c_str();
  File file = SD.open(path, FILE_WRITE);
  if (!file) {
    http.header(404, "File Not Write");
    http.close();
    return;
  }

  int buflen = 10*1024;
  auto buf = std::shared_ptr<unsigned char[]>(new unsigned char[buflen]);
  if (buf == NULL) {
    http.header(404, "Out of Memory");
    file.close();
    return;
  }

  if (http.hdrs.count("expect") && http.hdrs["expect"] == "100-continue") {
    http.client.println("HTTP/1.1 100 Continue");
    http.client.println("");
    http.client.flush();
  }

  for (unsigned long bytes = 0 ; bytes < clen ; ){
    if (http.client.available()) {
      int n = http.client.readBytes(buf.get(), buflen);
      file.write(buf.get(), n);
      bytes += n;
    } else {
      delay(1);
    }
  }

  file.close();
  http.header(201, "File Created");
  http.printf("Content-Location: %s\n", path);
  http.body();
  http.close();
}

void WebServer::file_get_handler(HTTP &http)
{
  const char *path = http.path.c_str();
  File file = SD.open(path);
  if (!file) {
    http.header(404, "File Not Found");
    http.close();
    return;
  }
  if (!SD.exists(path)) {
    http.header(404, "File Not Found");
    http.close();
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
    http.header(404, "Out of Memory");
    file.close();
    return;
  }

  http.header(200, "File Follows");
  http.printf("Content-Length: %d\n", content_length);
  http.printf("Content-Type: %s\n", content_type);
  http.body();

  dprintf("%s: sending %d bytes of %s", http.path.c_str(), content_length, content_type);

  for (; file.available() ;) {
    int n = file.readBytes((char *)buf.get(), buflen);
    if (n <= 0) {
      break;
    }
    //dprintf("writing %d bytes", n);
    http.write(buf.get(), n);
  }
  file.close();
  http.close();
}

int HTTP::idle(WebServer *server, unsigned long now)
{
  if (!client.connected()) {
    dprintf("http: lost connection");
    return 1;
  }
  // read header
  while (state == HTTP_IDLE && client.available()) {
    char c = client.read();
    if (c == '\n') {
      //dprintf("GOT LINE '%s'", line.c_str());
      if (line.length() == 0) {
        state = HTTP_DISPATCH;
        break;
      }
      // header line
      if (method == "") {
        int i = line.indexOf(' ');
        method = line.substring(0, i);
        path = line.substring(i+1, line.indexOf(' ', i+1));
        int j = path.indexOf('?');
        if (j >= 0) {
          String query = path.substring(j+1);
          path = path.substring(0, j);
          while (query.length() > 0) {
            int amp = query.indexOf('&');
            String keyvalue = query.substring(0, amp);
            int eq = keyvalue.indexOf('=');
            String key = keyvalue.substring(0, eq > 0 ? eq : keyvalue.length());
            String val = keyvalue.substring(eq > 0 ? eq+1 : keyvalue.length());
            query = query.substring(amp > 0 ? amp+1 : query.length());
            param[key] = val;
            //dprintf("PARAM %s = %s", key.c_str(), val.c_str());
          }
        }
      } else {
        int c = line.indexOf(':');
        if (c >= 0) {
          String key = line.substring(0, c);
          String val = line.substring(c+1);
          key.toLowerCase();
          key.trim();
          val.trim();
          hdrs[key] = val;
        }
      }
      line = "";
    } else if (c != '\r') {
      line += c;
    }
  }
  if (state == HTTP_IDLE) {
    return 0;
  }
  // dispatch request
  if (state == HTTP_DISPATCH) {
    for (struct Handler h: handlers) {
      if (path.equals(h.path)) {
        handler = h.handler;
        state = HTTP_HEADER;
        break;
      }
    }
    if (state == HTTP_DISPATCH) {
      //dprintf("METHOD '%s' PATH '%s'", method.c_str(), path.c_str()); 
      handler = method == "PUT" ? WebServer::file_put_handler : WebServer::file_get_handler;
      state = HTTP_HEADER;
    }
  }
  // handle request, generate response
  if (state < HTTP_CLOSED) {
    handler(*this);
  }
  // check for slow response
  if (state < HTTP_CLOSED && http_tm + 2000 < now) {
    dprintf("http: response time out");
    close();
  }
  // check when done
  return state == HTTP_CLOSED;
}

void HTTP::header(int code, const char *msg)
{
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

void HTTP::body()
{
  client.println("Connection: close");
  client.println("");
  state = method == "HEAD" ? HTTP_CLOSED : HTTP_BODY;
}

int HTTP::write(unsigned char *buf, int len)
{
  switch (state) {
    case HTTP_HEADER:
      header(200, "File Follows");
      body();
      // fall through
    case HTTP_BODY:
      for (int off = 0 ; off < len ; ) {
        int n = client.write(buf + off, min(10*1024, len - off));
        if (n < 0) {
          dprintf("write failed, http closed");
          state = HTTP_CLOSED;
          return off;
        }
        off += n;
      }
      // fall through
    default:
      return len;
  }
}

void HTTP::close()
{
  if (state == HTTP_HEADER) {
    body();
  }
  state = HTTP_CLOSED;
  client.stop();
}
