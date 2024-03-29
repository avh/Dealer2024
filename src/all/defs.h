// (c)2024, Arthur van Hoff, Artfahrt Inc.
//
#pragma once

#define USE_SERIAL          1
#define SERIAL_SPEED        115200
#define USE_WIFI            1

//
// Dealer Pins
//
#define POWER_PIN   2
#define BUTTON_PIN  3
#define CARD_PIN    4
#define M1_PIN1     5
#define M1_PIN2     6
#define M2_PIN1     7
#define M2_PIN2     8
#define MR_PIN1     9
#define MR_PIN2     10
#define FN_PIN1     11
#define FN_PIN2     12
#define BUZZER_PIN  13

#define LED_WIFI      A7
#define LED_POWER     A6
#define LED_LOADED    A3
#define LED_ROTATING  A2
#define LED_EJECTING  A1

//
// Camera Pins
//
#define LIGHT_PIN     GPIO_NUM_44
#define LED_CAPTURE   D6

//
// Capture Commands
//

#define CAMERA_ADDR         0x1A
#define CMD_CAPTURE         0xFE
#define CMD_COLLATE         0xFD
#define CMD_IDENTIFY        0xFC
#define CMD_CLEAR           0xFB
#define CMD_STATUS          0xFA

#define CARD_NULL           255      // no card detected yet
#define CARD_EMPTY          254      // no card detected in hopper
#define CARD_FAIL           253      // card detection failed    
#define CARD_USED           252      // card has been used  

#define CARDSUIT(c,s)       ((c)*13 + (s))
#define CARD(cs)            ((cs) % 13) 
#define SUIT(cs)            ((cs) / 13)
