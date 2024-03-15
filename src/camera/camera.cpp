// (c)2024, Arthur van Hoff, Artfahrt Inc.

#include "camera.h"
#include "image.h"
#include "webserver.h"

#define CAMERA_MODEL_XIAO_ESP32S3

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

#define convert565(v) (r565(v) + b565(v) + (g565(v)<<1))

Image latest;
Image tmp;
Image card;
Image suit;

float *vignet = NULL;
float vignet_f = 0;
float vignet_s = 0.95;
float vignet_a = 20.0;
int vignet_b = 128;

// unpack 565, convert from little endian to grayscale, rotate, scale down
static void unpack_565(unsigned short *src, int src_stride, int src_width, int src_height, Image &dst)
{
    dst.init(src_height/2, src_width/2);
    
    // calculate vignet
    if (vignet == NULL && vignet_f > 0) {
        vignet = new float[dst.width * dst.height];
        for (int r = 0 ; r < dst.height ; r++) {
            float dr = r - dst.height/2;
            for (int c = 0 ; c < dst.width ; c++) {
                float dc = c - dst.width/2;
                float d = sqrt(dr*dr + dc*dc);
                float v = vignet_s * vignet_f/sqrt(d*d + vignet_f*vignet_f);
                vignet[r * dst.width + c] = 1-v;
                if (r == 60) {
                    dprintf("vignet %3d,%3d = %.4f", r, c, 1-v);
                }
            }
        }
    }

    pixel *dstp = dst.data + dst.width * (dst.height - 1);
    for (int c = 0 ; c < dst.width ; c++, src += 2*src_stride, dstp += 1) {
        unsigned short *sp = src;
        pixel *dp = dstp;
        for (int r = 0 ; r < dst.height ; r++, sp += 2, dp -= dst.stride) {
            int v = convert565(sp[0]) + convert565(sp[1]) + convert565(sp[src_stride+0]) + convert565(sp[src_stride+1]);
            if (vignet != NULL) {
                v = ((v-(128<<4)) * vignet[r * dst.width + c]) * vignet_a + (vignet_b<<4);
                v = v < 0 ? 0 : v > 255<<4 ? 255<<4 : v;
            }
            *dp = (v>>4) & 0xFF;
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
    config.frame_size = FRAMESIZE_VGA;
    //config.frame_size = FRAMESIZE_240X240;
    //config.pixel_format = PIXFORMAT_JPEG;
    config.pixel_format = PIXFORMAT_RGB565;
    //config.pixel_format = PIXFORMAT_GRAYSCALE;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.jpeg_quality = 12;
    config.fb_count = 1;

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
        
    // drop down frame size for higher initial frame rate
    if (config.pixel_format == PIXFORMAT_JPEG){
        s->set_framesize(s, FRAMESIZE_QVGA);
    }
    next_capture_tm = millis();

    WebServer::add("/original.jpg", [](HTTP &http) {
        camera_fb_t *fb = cam.capture();
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

    WebServer::add("/capture.jpg", [](HTTP &http) {
        if (!cam.captureCard()) {
            http.header(404, "Capture Failed");
            http.close();
            return;
        }

        unsigned long tm = millis();
        latest.save("/latest.jpg");
        card.save("/card.jpg");
        suit.save("/suit.jpg");
        dprintf("saved in %lu ms", millis() - tm);
        
        //latest.debug("LATEST IMAGE");
        http.path = "/latest.jpg";
        WebServer::file_get_handler(http);
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
                http.printf("set aec_value to %d\n", atof(value.c_str()));
            } else if (key == "vignet_f") {
                vignet_f = atof(value.c_str());
                http.printf("set vignet_f to %f\n", atof(value.c_str()));
            } else if (key == "vignet_s") {
                vignet_s = atof(value.c_str());
                http.printf("set vignet_s to %f\n", atof(value.c_str()));
            } else if (key == "vignet_a") {
                vignet_a = atof(value.c_str());
                http.printf("set vignet_a to %f\n", atof(value.c_str()));
            } else if (key == "vignet_b") {
                vignet_b = atoi(value.c_str());
                http.printf("set vignet_b to %d\n", atoi(value.c_str()));
            }
            free(vignet);
            vignet = NULL;
        } 
        http.close();
    });
}

void Camera::idle(unsigned long now)
{
    // REMIND: later
}

camera_fb_t *Camera::capture()
{
    light.on(100, 500);

    // wait for the light to come on
    while (millis() < light.on_tm + 100) {
        delay(1);
    }
    // wait for the next frame to be ready
    while (millis() < next_capture_tm) {
        delay(1);
    }
    // capture frame
    camera_fb_t *fb = esp_camera_fb_get();
    // work around bug in esp_camera_fb_get
    if (fb != NULL) {
        esp_camera_fb_return(fb);
        fb = esp_camera_fb_get();
    }
    if (fb == NULL) {
        dprintf("camera: esp_camera_fb_get failed");
        return NULL;
    }

    next_capture_tm = millis() + 34;
    frame_nr += 1;
    return fb;
}

bool Camera::captureCard(int learn_card)
{
    dprintf("capturing card");
    last_card = CARD_NULL;
    camera_fb_t *fb = cam.capture();
    if (fb == NULL) {
        return false;
    }

    // pick useful region
    int x = 140;
    int y = 135;
    int w = 350;
    int h = 150;
    unpack_565((unsigned short *)fb->buf + x + y * fb->width, fb->width, w, h, latest);
    esp_camera_fb_return(fb);

    // located card and suit
    latest.locate(tmp, card, suit);

    // REMIND: identify card OR learn
    if (learn_card == CARD_NULL) { 
        // REMIND: identify
        dprintf("setting last_card to CARD_FAIL");
        last_card = CARD_FAIL;
    } else {
        // REMIND: learn
        dprintf("setting last_card to learn_card=%d", learn_card);
        last_card = learn_card;
    }
    return true;
}

void Camera::clearCard()
{
    dprintf("clearing last_card");
    last_card = CARD_NULL;
}

void Camera::commit()
{
    // REMIND: later
}