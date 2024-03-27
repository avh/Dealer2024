// (c)2024, Arthur van Hoff, Artfahrt Inc.

#include "deal.h"
#include "camera.h"
#include "webserver.h"

#if defined(CAMERA_MODEL_XIAO_ESP32S3)
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39

#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13
#endif

#define r565(v)     ((v) & 0x00F8)
#define g565(v)     ((((v) & 0x0007) << 5) | (((v) & 0xE000) >> 11))
#define b565(v)     (((v) & 0x1F00) >> 5)

//#define convert565(v) (r565(v) + b565(v) + (g565(v)<<1))
#define convert565(v) ((b565(v) + g565(v))>>1)

#define WINDOW_X                 50
#define WINDOW_Y                 85
#define WINDOW_WIDTH             120
#define WINDOW_HEIGHT            50
#define WIN_WIDTH                WINDOW_HEIGHT
#define WIN_HEIGHT               WINDOW_WIDTH

Image latest;
Image tmp;
Image card;
Image suit;
Image cardsuit;
Image overview;
Image cards;
Image suits;
extern WebServer www;
int light_delay = 200;

// unpack 565, convert from little endian to grayscale, rotate, scale down
static void unpack_565_rot_scale(unsigned short *src, int src_stride, int src_width, int src_height, Image &dst)
{
    dst.init(src_height/2, src_width/2);
    
    pixel *dstp = dst.data + dst.width * (dst.height - 1);
    for (int c = 0 ; c < dst.width ; c++, src += 2*src_stride, dstp += 1) {
        unsigned short *sp = src;
        pixel *dp = dstp;
        for (int r = 0 ; r < dst.height ; r++, sp += 2, dp -= dst.stride) {
            int v = convert565(sp[0]) + convert565(sp[1]) + convert565(sp[src_stride+0]) + convert565(sp[src_stride+1]);
            *dp = (v>>2) & 0xFF;
        }
    }
}
static void unpack_565_rot(unsigned short *src, int src_stride, int src_width, int src_height, Image &dst)
{
    dst.init(src_height, src_width);
    
    pixel *dstp = dst.data + dst.width * (dst.height - 1);
    for (int c = 0 ; c < dst.width ; c++, src += src_stride, dstp += 1) {
        unsigned short *sp = src;
        pixel *dp = dstp;
        for (int r = 0 ; r < dst.height ; r++, sp += 1, dp -= dst.stride) {
            *dp = convert565(*sp);
        }
    }
}


void Camera::init()
{
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    //config.xclk_freq_hz = 10000000;
    //config.frame_size = FRAMESIZE_VGA;
    config.frame_size = FRAMESIZE_240X240;
    //config.frame_size = FRAMESIZE_240X240;
    //config.pixel_format = PIXFORMAT_JPEG;
    config.pixel_format = PIXFORMAT_RGB565;
    //config.pixel_format = PIXFORMAT_RGB888;
    //config.pixel_format = PIXFORMAT_GRAYSCALE;
    //config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    config.grab_mode = CAMERA_GRAB_LATEST;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.jpeg_quality = 0;
    config.fb_count = 3;

    // camera init
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        dprintf("capture: camera init failed with error 0x%x", err);
        return;
    }

    sensor_t * s = esp_camera_sensor_get();
    s->set_brightness(s, 0); 
    s->set_saturation(s, 0);
    s->set_contrast(s, 0);
    s->set_agc_gain(s, 0);
    s->set_aec_value(s, 20);
    s->set_whitebal(s, 0);
    s->set_awb_gain(s, 0);

    s->set_exposure_ctrl(s, 0);
    s->set_aec2(s, 0);
    s->set_gain_ctrl(s, 0);
    s->set_bpc(s, 0);
    s->set_wpc(s, 0);
    s->set_raw_gma(s, 0);

    if (cards.load("/cards.jpg")) {
        if (cards.width != CARD_WIDTH * 13 || cards.height != CARD_HEIGHT) {
            dprintf("ERROR: cards image has wrong dimensions: %dx%d", cards.width, cards.height);
            cards.free();
        } else if (suits.load("/suits.jpg")) {
            if (suits.width != SUIT_WIDTH * 4 || suits.height != SUIT_HEIGHT) {
                dprintf("ERROR: suits image has wrong dimensions: %dx%d", suits.width, suits.height);
                cards.free();
                suits.free();
            }
        }
    }

    WebServer::add("/original.jpg", [](HTTP &http) {
        camera_fb_t *fb = cam.capture();

        for (int i = 0 ; i < 10 && fb != NULL ; i++) {
            esp_camera_fb_return(fb);
            fb = cam.capture();
        }
        if (fb == NULL) {
            http.header(404, "Capture Failed");
            http.close();
            return;
        }


        char buf[32];
        snprintf(buf, sizeof(buf), "Captured Frame %d", cam.frame_nr);
        http.header(200, buf);
        http.printf("Content-Type: image/jpeg\r\n");
        http.body();

        frame2jpg_cb(fb, 80, [](void *arg, size_t index, const void *data, size_t len) -> unsigned int {
            HTTP *http = (HTTP *)arg;
            return http->write((unsigned char *)data, len);
        }, &http); 

        esp_camera_fb_return(fb);
        http.close();
    });

    WebServer::add("/latest.jpg", [](HTTP &http) {
        cam.captureCard();
        latest.send(http);
    }); 
    WebServer::add("/cardsuit.jpg", [](HTTP &http) {
        cardsuit.send(http);
    }); 
    WebServer::add("/card.jpg", [](HTTP &http) {
        card.send(http);
    }); 
    WebServer::add("/cards.jpg", [](HTTP &http) {
        cards.send(http);
    }); 
    WebServer::add("/suit.jpg", [](HTTP &http) {
        suit.send(http);
    }); 
    WebServer::add("/suits.jpg", [](HTTP &http) {
        suits.send(http);
    }); 
    WebServer::add("/overview.jpg", [](HTTP &http) {
        overview.send(http);
    }); 
    www.add("/commit", [] (class HTTP& http) {
        if (cards.data != NULL) {
            cards.save("/cards.jpg");
        }
        if (suits.data != NULL) {
            suits.save("/suits.jpg");
        }
        http.header(200, "Committed");
        http.close();
    });

    WebServer::add("/controls", [](HTTP &http) {
        http.header(200, "Controls");
        http.body();
        sensor_t * s = esp_camera_sensor_get();
        for (auto i : http.param) {
            String key = i.first;
            String value = i.second;
            dprintf("got %s = %s", key.c_str(), value.c_str());
            if (key == "brightness") {
                s->set_brightness(s, atoi(value.c_str()));
                http.printf("set brightness to %d\n", atoi(value.c_str()));
            } else if (key == "contrast") {
                s->set_contrast(s, atoi(value.c_str()));
                http.printf("set contrast to %d\n", atoi(value.c_str()));
            } else if (key == "saturation") {
                s->set_saturation(s, atoi(value.c_str()));
                http.printf("set saturation to %d\n", atoi(value.c_str()));
            } else if (key == "light") {
                light.brightness = atoi(value.c_str());
                http.printf("set light to %d\n", atoi(value.c_str()));
            } else if (key == "agc") {
                s->set_agc_gain(s, atoi(value.c_str()));
                http.printf("set agc_gain to %d\n", atoi(value.c_str()));
            } else if (key == "gain_ctrl") {
                s->set_gain_ctrl(s, atoi(value.c_str()));
                http.printf("set gain_ctrl to %d\n", atoi(value.c_str()));
            } else if (key == "exposure_ctrl") {
                s->set_exposure_ctrl(s, atoi(value.c_str()));
                http.printf("set exposure_ctrl to %d\n", atoi(value.c_str()));
            } else if (key == "aec2") {
                s->set_aec2(s, atoi(value.c_str()));
                http.printf("set aec2 to %d\n", atoi(value.c_str()));
            } else if (key == "aec") {
                s->set_aec_value(s, atoi(value.c_str()));
                http.printf("set aec_value to %d\n", atoi(value.c_str()));
            } else if (key == "light_delay") {
                light_delay = atoi(value.c_str());
                http.printf("set light_delay to %d\n", atoi(value.c_str()));
            }
        } 
        http.close();
    });
}

void Camera::clearCard(bool learn)
{
    dprintf(learning ? "clearing cards for learning" : "clearing cards");
    last_card = CARD_NULL;
    prev_card = CARD_NULL;
    card_count = 0;
    this->learning = learn;

    if (learning) {
        cardsuit.init(SUITLEN * CARDSUIT_WIDTH, NSUITS * CARDSUIT_HEIGHT);
    }

    if (true) {
        overview.init((SUITLEN+1) * WIN_WIDTH, NSUITS * WIN_HEIGHT, 128);
    }
}

Image frms[2];

camera_fb_t *Camera::capture()
{
    // turn on the light
    light.on(100, 1000);

    // wait for the light to come on
    while (millis() < light.on_tm + light_delay) {
        delay(1);
    }

    for (int attempt = 0 ; ; attempt++) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb == NULL) {
            dprintf("camera: esp_camera_fb_get failed");
            return NULL;
        }
        unpack_565_rot((unsigned short *)fb->buf + WINDOW_X + WINDOW_Y * fb->width, fb->width, WINDOW_WIDTH, WINDOW_HEIGHT, frms[frame_nr%2]);
        if (frms[0].same(frms[1])) {
            dprintf("frame the same, retrying");
            continue;
        }
        
        frame_tm = millis();
        frame_nr += 1;
        return fb;
    }
}

bool Camera::captureCard()
{
    unsigned long tm = millis();
    for (int attempt = 0 ; ; attempt++) {
        //dprintf("capturing card, attempt=%d, learning=%d", attempt, learning);
        last_card = CARD_NULL;
        camera_fb_t *fb = cam.capture();
        if (fb == NULL) {
            return false;
        }
        // pick useful region
        int x = WINDOW_X;
        int y = WINDOW_Y;
        int w = WINDOW_WIDTH;
        int h = WINDOW_HEIGHT;
        unpack_565_rot((unsigned short *)fb->buf + x + y * fb->width, fb->width, w, h, latest);
        esp_camera_fb_return(fb);

        // identify card OR learn
        if (!learning) {
            // located card and suit
            latest.locate(tmp, card, suit);

            // try to predict card using ML
            last_card = predict(latest);
            if (last_card == CARD_NULL) {
                if (cards.data != NULL) {
                    int c = card.match(cards);
                    int r = suit.match(suits);
                    if (c >= 0 && r >= 0) {
                        last_card = c + r * 13;
                    } else {
                        last_card = CARD_FAIL;
                    }
                } else {
                    last_card = CARD_FAIL;
                }
            }
            if (last_card == prev_card && attempt == 0) {
                dprintf("capture: detected duplicate %s, trying again", full_name(last_card));
                continue;
            }
            if (last_card == CARD_MOTION) {
                if (attempt < 4) {
                    dprintf("capture: detected motion %s, trying again", full_name(last_card));
                    continue;
                }
                last_card = CARD_FAIL;
            }
       } else {
            // learn
            //dprintf("setting last_card to learn_card=%d", learn_card);
            last_card = card_count;
        }
        dprintf("capture: frame %d, %s card %d as %s in %dms", frame_nr, learning ? "learn" : "identify", card_count, full_name(last_card), millis() - tm);
        if (cardsuit.data != NULL) {
            int c = CARD(last_card);
            int r = SUIT(last_card);
            cardsuit.copy(c * CARDSUIT_WIDTH, r * CARDSUIT_HEIGHT, card);
            cardsuit.copy(c * CARDSUIT_WIDTH, r * CARDSUIT_HEIGHT + CARD_HEIGHT + 2, suit);
        }
        if (overview.data != NULL) {
            if (card_count == 52) {
                overview.copy(13 * latest.width, 3 * latest.height, latest);
            } else {
                int cs = card_count % DECKLEN;
                int c = CARD(cs);
                int r = SUIT(cs);
                overview.copy(c * latest.width, r * latest.height, latest);
            }
        }
        prev_card = last_card;
        card_count += 1;
        return true;
    }
}

void Camera::collate()
{
    dprintf("collate");
    if (cardsuit.data != NULL) {
        // cards
        cards.init(HANDSIZE * CARD_WIDTH, CARD_HEIGHT);
        for (int y = 0 ; y < cards.height; y++) {
            for (int x = 0 ; x < cards.width ; x++) {
                unsigned long sum = 0;
                for (int i = 0 ; i < NSUITS ; i++) {
                    sum += *cardsuit.addr(x, y + i * CARDSUIT_HEIGHT);
                }
                *cards.addr(x, y) = sum / NSUITS;
            }
        }
        dprintf("update cards");

        // suits
        suits.init(NSUITS * SUIT_WIDTH, SUIT_HEIGHT);
        for (int s = 0 ; s < 4 ; s++) {
            for (int y = 0 ; y < SUIT_HEIGHT; y++) {
                for (int x = 0 ; x < SUIT_WIDTH ; x++) {
                    unsigned long sum = 0;
                    for (int i = 0 ; i < SUITLEN ; i++) {
                        sum += *cardsuit.addr(x + i * CARDSUIT_WIDTH, y + s * CARDSUIT_HEIGHT + CARD_HEIGHT + 2);
                    }
                    *suits.addr(x + s * SUIT_WIDTH, y) = sum / SUITLEN;
                }
            }
        }
        dprintf("update suits");
    }
}