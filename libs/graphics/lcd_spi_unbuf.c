/*
 * This file is part of Espruino, a JavaScript interpreter for Microcontrollers
 *
 * Copyright (C) 2020 Gordon Williams <gw@pur3.co.uk>, atc1441, MaBecker, Jeffmer
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * ----------------------------------------------------------------------------
 * Graphics Backend for drawing to SPI displays in unbuffered mode
 * ----------------------------------------------------------------------------
 */

/*JSON{
  "type" : "class",
  "class" : "lcd_spi_unbuf"
}*/

#include "lcd_spilcd_info.h"
#include "lcd_spi_unbuf.h"
#include "jsutils.h"
#include "jsinteractive.h"
#include "jswrapper.h"
#include "jswrap_graphics.h"
#include "jshardware.h"

static Pin _pin_mosi;
static Pin _pin_clk;
static Pin _pin_cs;
static Pin _pin_dc;
static int _colstart;
static int _rowstart;
static int _lastx=-1;
static int _lasty=-1;
static uint16_t _chunk_buffer[LCD_SPI_UNBUF_LEN];
static int _chunk_index = 0;
IOEventFlags _device;

static void spi_cmd(const uint8_t cmd)
{
  jshPinSetValue(_pin_dc, 0);
  jshSPISend(_device, cmd);
  jshPinSetValue(_pin_dc, 1);
}

static inline void spi_data(const uint8_t *data, int len)
{
  jshSPISendMany(_device, data, NULL, len, NULL);
}

static void flush_chunk_buffer(){
  if(_chunk_index == 0) return;
  spi_data((uint8_t *)&_chunk_buffer, _chunk_index*2);
  _chunk_index = 0;
}

static inline bool willFlush(){
  return _chunk_index == LCD_SPI_UNBUF_LEN - 1;
}

static inline void _put_pixel( uint16_t c) {
  _chunk_buffer[_chunk_index++] = c;
  if (_chunk_index==LCD_SPI_UNBUF_LEN) flush_chunk_buffer();
}

void lcdSendInitCmd_SPILCD() {
  // Send initialization commands to ST7735
  const unsigned char *cmd = SPILCD_INIT_CODE;
  while(cmd[CMDINDEX_DATALEN]!=255) {
    jshPinSetValue(_pin_cs, 0);
    spi_cmd(cmd[CMDINDEX_CMD]);
    if (cmd[CMDINDEX_DATALEN]) spi_data(&cmd[3], cmd[CMDINDEX_DATALEN]);
    if (cmd[CMDINDEX_DELAY])
      jshDelayMicroseconds(1000*cmd[CMDINDEX_DELAY]);
    jshPinSetValue(_pin_cs, 1);
    cmd += 3 + cmd[CMDINDEX_DATALEN];
  }
}

 /// flush chunk buffer to screen
void lcd_flip() {
  if(_chunk_index == 0) return;
  jshPinSetValue(_pin_cs, 0);
  flush_chunk_buffer();
  jshPinSetValue(_pin_cs, 1);
}

void graphicsInternalFlip() {
  lcd_flip();
}

void jshLCD_SPI_UNBUFInitInfo(JshLCD_SPI_UNBUFInfo *inf) {
  inf->pinCS         = PIN_UNDEFINED;
  inf->pinDC         = PIN_UNDEFINED;
  inf->width         = 240;
  inf->height        = 320;
  inf->colstart        = 0;
  inf->rowstart        = 0;
}

bool jsspiPopulateOptionsInfo( JshLCD_SPI_UNBUFInfo *inf, JsVar *options){
  jshLCD_SPI_UNBUFInitInfo(inf);
  jsvConfigObject configs[] = {
    {"cs", JSV_PIN, &inf->pinCS},
    {"dc", JSV_PIN, &inf->pinDC},
    {"width", JSV_INTEGER , &inf->width},
    {"height", JSV_INTEGER , &inf->height},
    {"colstart", JSV_INTEGER , &inf->colstart},
    {"rowstart", JSV_INTEGER , &inf->rowstart},
  };

  return jsvReadConfigObject(options, configs, sizeof(configs) / sizeof(jsvConfigObject))
          && inf->pinDC != PIN_UNDEFINED;
}

/*JSON{
  "type" : "idle",
  "generate" : "jswrap_lcd_spi_unbuf_idle"
}*/
bool jswrap_lcd_spi_unbuf_idle() {
    lcd_flip();
    return false;
}

/*JSON{
  "type" : "staticmethod",
  "class" : "lcd_spi_unbuf",
  "name" : "connect",
  "generate" : "jswrap_lcd_spi_unbuf_connect",
  "params" : [
    ["device","JsVar","The used SPI device"],
    ["options","JsVar","An Object containing extra information"]
  ],
  "return" : ["JsVar","The new `Graphics` object"],
  "return_object" : "Graphics"
}*/
JsVar *jswrap_lcd_spi_unbuf_connect(JsVar *device, JsVar *options) {
  JsVar *parent = jspNewObject(0, "Graphics");
  if (!parent) {
    jsExceptionHere(JSET_ERROR,"creating new object Graphics");
    return NULL;
  }

  JshLCD_SPI_UNBUFInfo inf;

  if (!jsspiPopulateOptionsInfo(&inf, options)) {
    jsExceptionHere(JSET_ERROR,"pins not supplied correctly");
    jsvUnLock(parent);
    return NULL;
  }
  _pin_cs = inf.pinCS;
  _pin_dc = inf.pinDC;
  _colstart = inf.colstart;
  _rowstart = inf.rowstart;
  _device = jsiGetDeviceFromClass(device);

  if (!DEVICE_IS_SPI(_device)) {
    jsExceptionHere(JSET_ERROR,"Software SPI is not supported for now");
    jsvUnLock(parent);
    return NULL;
  }

  JsGraphics gfx;
  graphicsStructInit(&gfx,inf.width,inf.height,16);
  gfx.data.type = JSGRAPHICSTYPE_LCD_SPI_UNBUF;
  gfx.graphicsVar = parent;

  jshPinOutput(_pin_dc, 1);
  jshPinSetValue(_pin_dc, 1);

  jshPinOutput(_pin_cs, 1);
  jshPinSetValue(_pin_cs, 1);

  lcd_spi_unbuf_setCallbacks(&gfx);
  graphicsSetVarInitial(&gfx);

  // Create 'flip' fn
  JsVar *fn;
  fn = jsvNewNativeFunction((void (*)(void))lcd_flip, JSWAT_VOID);
  jsvObjectSetChildAndUnLock(parent,"flip",fn);

  return parent;
}

#ifdef LCD_SPI_DEVICE
void lcd_spi_unbuf_init(JsGraphics *gfx) {
  gfx->data.width = LCD_WIDTH;
  gfx->data.height = LCD_HEIGHT;
  gfx->data.bpp = LCD_BPP;

  _pin_mosi = LCD_SPI_MOSI;
  _pin_clk = LCD_SPI_SCK;
  _pin_cs = LCD_SPI_CS;
  _pin_dc = LCD_SPI_DC;
  _device = LCD_SPI_DEVICE;
  _colstart = 0;
  _rowstart = 0;

#ifdef LCD_BL
  jshPinOutput(LCD_BL, 1);
#endif
#ifdef LCD_EN
  jshPinOutput(LCD_EN, 1);
#endif
  jshPinOutput(_pin_dc, 1);
  jshPinOutput(_pin_cs, 1);

  JshSPIInfo inf;
  jshSPIInitInfo(&inf);
#ifndef LCD_SPI_BITRATE
#define LCD_SPI_BITRATE 8000000
#endif
  inf.baudRate = LCD_SPI_BITRATE;
  inf.pinMOSI = LCD_SPI_MOSI;
#ifdef LCD_SPI_MISO
  inf.pinMISO = LCD_SPI_MISO;
#endif
  inf.pinSCK = LCD_SPI_SCK;
  jshSPISetup(_device, &inf);
  lcdSendInitCmd_SPILCD();
}
#endif

void disp_spi_transfer_addrwin(int x1, int y1, int x2, int y2) {
  uint8_t wd[4];
  flush_chunk_buffer();
  x1 += _colstart;
  y1 += _rowstart;
  x2 += _colstart;
  y2 += _rowstart;
  spi_cmd(0x2A);
  wd[0] = (uint8_t)(x1>>8);
  wd[1] = (uint8_t)(x1 & 0xFF);
  wd[2] = (uint8_t)(x2>>8);
  wd[3] = (uint8_t)(x2 & 0xFF);
  spi_data((uint8_t *)&wd, 4);
  spi_cmd(0x2B);
  wd[0] = (uint8_t)(y1>>8);
  wd[1] = (uint8_t)(y1 & 0xFF);
  wd[2] = (uint8_t)(y2>>8);
  wd[3] = (uint8_t)(y2 & 0xFF);
  spi_data((uint8_t *)&wd, 4);
  spi_cmd(0x2C);
}

void lcd_spi_unbuf_setPixel(JsGraphics *gfx, int x, int y, unsigned int col) {
  uint16_t color = (uint16_t)((col>>8) | (col<<8)); // endianness
  if (x!=_lastx+1 || y!=_lasty) {
    jshPinSetValue(_pin_cs, 0);
    disp_spi_transfer_addrwin(x, y, gfx->data.width, y+1);
    jshPinSetValue(_pin_cs, 1); //will never flush after
    _put_pixel(color);
    _lastx = x;
    _lasty = y;
  } else {
    _lastx++;
    if (willFlush()){
      jshPinSetValue(_pin_cs, 0);
      _put_pixel(color);
      jshPinSetValue(_pin_cs, 1);
    } else {
      _put_pixel(color);
    }
  }
}

void lcd_spi_unbuf_fillRect(JsGraphics *gfx, int x1, int y1, int x2, int y2, unsigned int col) {
  int pixels = (1+x2-x1)*(1+y2-y1);
  uint16_t color = (uint16_t)((col>>8) | (col<<8)); // endianness
  jshPinSetValue(_pin_cs, 0);
  disp_spi_transfer_addrwin(x1, y1, x2, y2);
  for (int i=0; i<pixels; i++) _put_pixel(color);
  jshPinSetValue(_pin_cs, 1);
  _lastx=-1;
  _lasty=-1;
}

void lcd_spi_unbuf_scroll(JsGraphics *gfx, int xdir, int ydir, int x1, int y1, int x2, int y2) {
  // don't even try scrolling at the moment - we could maybe adjust display registers but then we have to adjust x/y of any subsequent writes
  NOT_USED(gfx);
  NOT_USED(xdir);
  NOT_USED(ydir);
  NOT_USED(x1);
  NOT_USED(y1);
  NOT_USED(x2);
  NOT_USED(y2);
}

void lcd_spi_unbuf_setCallbacks(JsGraphics *gfx) {
  gfx->setPixel = lcd_spi_unbuf_setPixel;
  gfx->fillRect = lcd_spi_unbuf_fillRect;
  gfx->scroll = lcd_spi_unbuf_scroll;
}
