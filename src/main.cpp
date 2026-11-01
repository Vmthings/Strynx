#include <Arduino.h>
#include <M5Unified.h>
#include <cstring>
#include "app.h"
#include "custom_boot_screen.h"
#include "roboto24.h"

namespace {
bool booting = true;
uint32_t bootStart = 0, lastStatusCheck = 0;
int lastProgress = -1;
char previousStatus[48] = "";
int topHeight() { return StrynxConfig::topbar ? 56 : 0; }
int bottomHeight() { return StrynxConfig::backButton ? 64 : 0; }
int contentHeight() { return M5.Display.height()-topHeight()-bottomHeight(); }
int textWidth(const char* text) {
  int width=0;
  while(*text) { unsigned char c=*text++; if(c>=32 && c<=125) width+=strynxRoboto24Glyphs[c-32].advance; }
  return width;
}
// Render the real Roboto bitmap from the C font asset through LovyanGFX.
void roboto(const char* text,int x,int baseline) {
  auto& d=M5.Display;
  while(*text) {
    unsigned char c=*text++;
    if(c<32 || c>125) continue;
    const auto& g=strynxRoboto24Glyphs[c-32];
    for(int y=0;y<g.height;++y) for(int col=0;col<g.width;++col) {
      int bit=y*g.width+col;
      if(strynxRoboto24Bitmap[g.offset+bit/8] & (0x80>>(bit%8)))
        d.drawPixel(x+g.x+col,baseline+g.y+y,TFT_WHITE);
    }
    x+=g.advance;
  }
}
void status(bool force=false) {
  if(!StrynxConfig::topbar) return;
  m5::rtc_datetime_t dt{};
  char clock[8]="--:--", right[32], key[48];
  if(M5.Rtc.getDateTime(&dt) && dt.time.hours<24 && dt.time.minutes<60)
    snprintf(clock,sizeof(clock),"%02d:%02d",dt.time.hours,dt.time.minutes);
  int battery=M5.Power.getBatteryLevel();
  if(battery>=0 && battery<=100) snprintf(right,sizeof(right),"%d%%   %s",battery,clock);
  else snprintf(right,sizeof(right),"--%%   %s",clock);
  snprintf(key,sizeof(key),"%s %s",clock,right);
  if(!force && !strcmp(key,previousStatus)) return;
  strcpy(previousStatus,key);
  M5.Display.fillRect(0,0,M5.Display.width(),topHeight(),TFT_BLACK);
  roboto(clock,24,36);
  roboto(right,M5.Display.width()-24-textWidth(right),36);
}
void drawApp(bool full=true) { StrynxApp::draw(0,topHeight(),M5.Display.width(),contentHeight(),full); }
void render() {
  auto& d=M5.Display;
  d.fillScreen(StrynxConfig::background);
  drawApp(); status(true);
  if(StrynxConfig::backButton) {
    // Left-pointing (rotated) triangle with rounded corners, no label or outline.
    d.fillRect(0,d.height()-bottomHeight(),d.width(),bottomHeight(),TFT_BLACK);
    int x=40, y=d.height()-bottomHeight()/2;
    d.fillTriangle(x-12,y,x+9,y-12,x+9,y+12,TFT_WHITE);
    d.fillCircle(x-12,y,3,TFT_WHITE);
    d.fillCircle(x+9,y-12,3,TFT_WHITE);
    d.fillCircle(x+9,y+12,3,TFT_WHITE);
  }
}
void bootImage() {
  auto& d=M5.Display;
  d.fillScreen(StrynxConfig::background);
  int scale=min(d.width()/bootImageWidth,d.height()/bootImageHeight);
  scale=max(1,min(scale,12));
  int x=(d.width()-bootImageWidth*scale)/2, y=(d.height()-bootImageHeight*scale)/2;
  for(int row=0;row<bootImageHeight;++row) for(int col=0;col<bootImageWidth;++col)
    d.fillRect(x+col*scale,y+row*scale,scale,scale,bootImagePixels[row*bootImageWidth+col]);
}
}
void setup() {
  auto cfg=M5.config();
  // Retain official Tab5 power/display/touch startup and LovyanGFX engine.
  M5.begin(cfg);
  M5.Display.setRotation(0);
  if(M5.Display.width()>M5.Display.height()) M5.Display.setRotation(1);
  M5.Display.setBrightness(160);
  StrynxApp::begin();
  bootStart=millis();
  if(StrynxConfig::bootScreen) bootImage();
  else M5.Display.fillScreen(StrynxConfig::background);
  if(!StrynxConfig::bootScreen && !StrynxConfig::bootAnimation) { booting=false; render(); }
}
void loop() {
  M5.update();
  if(booting) {
    uint32_t elapsed=millis()-bootStart;
    if(elapsed>=StrynxConfig::bootDurationMs) { booting=false; render(); }
    else if(StrynxConfig::bootAnimation) {
      int progress=int(uint64_t(elapsed)*100/max(uint32_t(1),StrynxConfig::bootDurationMs));
      if(progress!=lastProgress) {
        lastProgress=progress;
        int w=M5.Display.width()-120;
        M5.Display.fillRect(60,M5.Display.height()-90,w,8,StrynxConfig::background);
        M5.Display.fillRect(60,M5.Display.height()-90,w*progress/100,8,StrynxConfig::accent);
      }
    }
    delay(10); return;
  }
  static bool appDirty=false;
  static uint32_t lastAppFrame=0;
  static bool appPointerDown=false;
  static int pointerX=0,pointerY=0;
  if(M5.Touch.getCount()) {
    auto t=M5.Touch.getDetail();
    if(t.wasPressed()) {
      if(StrynxConfig::backButton && t.y>=M5.Display.height()-bottomHeight()) {
        if(t.x>=0 && t.x<80 && StrynxApp::back()) drawApp();
      } else if(t.y>=topHeight() && t.y<M5.Display.height()-bottomHeight()) {
        appPointerDown=true;
      }
    }
    if(appPointerDown) {
      pointerX=t.x; pointerY=t.y-topHeight();
      if(StrynxApp::pointer(pointerX,pointerY,true)) appDirty=true;
    }
  } else if(appPointerDown) {
    appPointerDown=false;
    if(StrynxApp::pointer(pointerX,pointerY,false)) appDirty=true;
  }
  if(StrynxApp::update()) appDirty=true;
  // Coalesce touch and scrollbar fade changes into one frame (up to 60 Hz).
  if(appDirty && millis()-lastAppFrame>=16) {
    drawApp(false);
    appDirty=false;
    lastAppFrame=millis();
  }
  if(millis()-lastStatusCheck>=1000) { lastStatusCheck=millis(); status(); }
  delay(1);
}
