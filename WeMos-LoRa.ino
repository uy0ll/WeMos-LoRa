/*******************************************************************************
 * Jan 2016 Adapted for use with ESP8266 mcu by Maarten Westenberg
 * Copyright (c) 2015 Thomas Telkamp, Matthijs Kooijman and Maarten Westenberg
 *
 * Permission is hereby granted, free of charge, to anyone
 * obtaining a copy of this document and accompanying files,
 * to do whatever they want with them without any restriction,
 * including, but not limited to, copying, modification and redistribution.
 * NO WARRANTY OF ANY KIND IS PROVIDED.
 *
 * This example sends a valid LoRaWAN packet with payload "Hello ESP world!", that
 * will be processed by The Things Network server.
 *
 * Note: LoRaWAN per sub-band duty-cycle limitation is enforced (1% in g1, 
*  0.1% in g2). 
 *
 * Change DEVADDR to a unique address! 
 * See http://thethingsnetwork.org/wiki/AddressSpace
 *
 * Do not forget to define the radio type correctly in config.h, default is:
 *   #define CFG_sx1272_radio 1
 * for SX1272 and RFM92, but change to:
 *   #define CFG_sx1276_radio 1
 * for SX1276 and RFM95.
 *
 * History: 
 * Jan 2016, Modified by Maarten to run on ESP8266. Running on Wemos D1-mini
 *******************************************************************************/
 
// Use ESP declarations. This sketch does not use WiFi stack of ESP
//  #include <ESP8266WiFi.h>
//  #include <WiFiManager.h> 
// #include <ESP.h>
#include <base64.h>

// All specific changes needed for ESP8266 need be made in hal.cpp if possible
// Include ESP environment definitions in lmic.h (lmic/limic.h) if needed
#include <esp-lmic.h>
#include <hal/hal.h>
#include <SPI.h>

// LoRaWAN Application identifier (AppEUI)
// Not used in this example
static const u1_t APPEUI[8]  = { 0x70, 0xB3, 0xD5, 0x7E, 0xF0, 0x00, 0x6B, 0x21 };

// LoRaWAN DevEUI, unique device ID (LSBF)
// Not used in this example
static const u1_t DEVEUI[8]  = { 0x00, 0xB9, 0xD9, 0xF7, 0xAB, 0xE5, 0x0F, 0x51 };

// LoRaWAN NwkSKey, network session key 
// Use this key for The Things Network
static const u1_t DEVKEY[16] = { 0x0C, 0xEB, 0x9B, 0xE7, 0xC6, 0xC0, 0xCF, 0x6E, 0x71, 0xF4, 0x1F, 0xC6, 0x37, 0xAE, 0xF1, 0x8A };

// LoRaWAN AppSKey, application session key
// Use this key to get your data decrypted by The Things Network
static const u1_t ARTKEY[16] = { 0xDA, 0x88, 0x14, 0x79, 0xC7, 0xB9, 0x04, 0xD5, 0xC3, 0x5C, 0x93, 0xBA, 0x2C, 0xE3, 0x11, 0xF1 };

// LoRaWAN end-device address (DevAddr)
// See http://thethingsnetwork.org/wiki/AddressSpace
static const u4_t DEVADDR = 0x26011AC8 ; // <-- Change this address for every node! ESP8266 node 0x01

//////////////////////////////////////////////////
// APPLICATION CALLBACKS
//////////////////////////////////////////////////

// provide application router ID (8 bytes, LSBF)
void os_getArtEui (u1_t* buf) {
    memcpy(buf, APPEUI, 8);
}

// provide device ID (8 bytes, LSBF)
void os_getDevEui (u1_t* buf) {
    memcpy(buf, DEVEUI, 8);
}

// provide device key (16 bytes)
void os_getDevKey (u1_t* buf) {
    memcpy(buf, DEVKEY, 16);
}

uint8_t mydata[] = "Hello UrbanGrid world!";
static osjob_t sendjob;

// Pin mapping
// XXX We have to see whether all these pins are really used
// if not, we can use them for real sensor work.
lmic_pinmap pins = {
  .nss = 16,      // Make D0/GPIO16, is nSS on ESP8266
  .rxtx = LMIC_UNUSED_PIN,      // D4/GPIO2. For placeholder only,
            // Do not connected on RFM92/RFM95
  .rst = LMIC_UNUSED_PIN,       // Make float, not  Needed on RFM92/RFM95? (probably not)
  .dio = {15, LMIC_UNUSED_PIN, LMIC_UNUSED_PIN},   // Specify pin numbers for DIO0, 1, 2
            // D1/GPIO5,D2/GPIO4,D3/GPIO3 are usable pins on ESP8266
            // NOTE: D3 not really usable when UART not connected
            // As it is used during bootup and will probably not boot.
            // Leave D3 Pin unconnected for sensor to work
};

void onEvent (ev_t ev) {
    //debug_event(ev);

    switch(ev) {
      // scheduled data sent (optionally data received)
      // note: this includes the receive window!
      case EV_TXCOMPLETE:
          // use this event to keep track of actual transmissions
          Serial.print("Event EV_TXCOMPLETE, time: ");
          Serial.println(millis() / 1000);
          if(LMIC.dataLen) { // data received in rx slot after tx
              //debug_buf(LMIC.frame+LMIC.dataBeg, LMIC.dataLen);
              Serial.println("Data Received!");
          }
          break;
       default:
          break;
    }
}

void do_send(osjob_t* j){
      Serial.print("Time: ");
      Serial.println(millis() / 1000);
      // Show TX channel (channel numbers are local to LMIC)
      Serial.print("Send, txCnhl: ");
      Serial.println(LMIC.txChnl);
      Serial.print("Opmode check: ");
      // Check if there is not a current TX/RX job running
    if (LMIC.opmode & (1 << 7)) {
      Serial.println("OP_TXRXPEND, not sending");
    } else {
      Serial.println("ok");
      // Prepare upstream data transmission at the next possible time.
      LMIC_setTxData2(1, mydata, sizeof(mydata)-1, 0);
    }
    // Schedule a timed job to run at the given timestamp (absolute system time)
    os_setTimedCallback(j, os_getTime()+sec2osticks(20), do_send);
         
}

// Remove the Serial messages once the unit is running reliable
// 
void setup() {
  Serial.begin(115200);
  Serial.println("Starting");
  // LMIC init
  os_init();
  Serial.println("os_init() finished");
  
  // Reset the MAC state. Session and pending data transfers will be discarded.
  LMIC_reset();
  Serial.println("LMIC_reet() finished");
  
  // Set static session parameters. Instead of dynamically establishing a session 
  // by joining the network, precomputed session parameters are be provided.
  LMIC_setSession (0x1, DEVADDR, (uint8_t*)DEVKEY, (uint8_t*)ARTKEY);
  Serial.println("LMIC_setSession() finished");
  
  // Disable data rate adaptation
  LMIC_setAdrMode(0);
  Serial.println("LMICsetAddrMode() finished");
  
  // Disable link check validation
  LMIC_setLinkCheckMode(0);
  // Disable beacon tracking
  LMIC_disableTracking ();
  // Stop listening for downstream data (periodical reception)
  LMIC_stopPingable();
  // Set data rate and transmit power (note: txpow seems to be ignored by the library)
  LMIC_setDrTxpow(DR_SF12,10);
  //
// LMIC_setupChannel(0, 868100000, DR_RANGE_MAP(DR_SF12, DR_SF7),BAND_AUX );   
LMIC_setupChannel(0, 868100000, DR_RANGE_MAP(DR_SF7, DR_SF7),BAND_DECI );     // BAND_DECI - duty cycle 10% 27dBm
for(int i=1;i<=8;i++) {
  LMIC_disableChannel(i);
}

/*
 LMIC_setupChannel(1, 868300000, DR_RANGE_MAP(DR_SF12, DR_SF7B), BAND_CENTI); // g-band
 LMIC_setupChannel(2, 868500000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI); // g-band
 LMIC_setupChannel(3, 867100000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI); // g-band
 LMIC_setupChannel(4, 867300000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI); // g-band
 LMIC_setupChannel(5, 867500000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI); // g-band
 LMIC_setupChannel(6, 867700000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI); // g-band
 LMIC_setupChannel(7, 867900000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI); // g-band
 LMIC_setupChannel(8, 868800000, DR_RANGE_MAP(DR_FSK, DR_FSK), BAND_MILLI); // g2-band
 */
  //Serial.flush();
  Serial.println("Init done");
}

// Same loop as used in original sketch. Modify for ESP8266 sensor use.
//
void loop() {

do_send(&sendjob);
delay(10);

while(1) {
  os_runloop_once();
  delay(1000);
  }
}


