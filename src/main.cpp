#include <Arduino.h>
#include <M5Unified.h>
#include <cstring>
#include <cmath>
#include <Preferences.h>
#include <esp_system.h>
#include "app.h"
#include "flux_shell.h"
#include "custom_boot_screen.h"
#include "roboto24.h"

namespace {
Preferences recoveryPrefs;
bool safeHome=false;
char homeReason[96]="Backup home requested";
float menuProgress=0;
bool menuOpen=false, menuGesture=false, menuTap=false;
int gestureY=0, menuTouchX=0, menuTouchY=0;
uint32_t menuTick=0;
bool menuSnapshot=false;
char serialLine[512]={}; size_t serialUsed=0;
bool serialOverflow=false;
void setHome(bool enabled,const char* reason);
bool booting = true;
uint32_t bootStart = 0, lastStatusCheck = 0;
M5Canvas bootOS(&M5.Display);
lgfx::LGFXBase* shellOutput=&M5.Display;
bool slidePrepared=false;
uint32_t slideStart=0, lastSlideFrame=0;
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
void roboto(const char* text,int x,int baseline,uint16_t color=TFT_WHITE) {
  auto& d=*shellOutput;
  while(*text) {
    unsigned char c=*text++;
    if(c<32 || c>125) continue;
    const auto& g=strynxRoboto24Glyphs[c-32];
    for(int y=0;y<g.height;++y) for(int col=0;col<g.width;++col) {
      int bit=y*g.width+col;
      if(strynxRoboto24Bitmap[g.offset+bit/8] & (0x80>>(bit%8)))
        d.drawPixel(x+g.x+col,baseline+g.y+y,color);
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
  shellOutput->fillRect(0,0,M5.Display.width(),topHeight(),TFT_BLACK);
  roboto(clock,24,36);
  roboto(right,M5.Display.width()-24-textWidth(right),36);
}
StrynxUI::Rect launcherIcon() { return {28,topHeight()+100,96,96}; }
void drawApp(bool full=true) {
  if(!safeHome) { StrynxApp::draw(0,topHeight(),M5.Display.width(),contentHeight(),full); return; }
  auto& d=*shellOutput;
  d.fillRect(0,topHeight(),d.width(),contentHeight(),TFT_WHITE);
  d.setTextColor(TFT_BLACK,TFT_WHITE); d.setTextSize(2);
  // Recovery details remain available through serial home status.
  auto icon=launcherIcon();
  d.fillRect(icon.x,icon.y,icon.w,icon.h,0x8410); // Grey launcher icon.
  d.drawRect(icon.x+26,icon.y+26,44,44,TFT_WHITE);
  d.setTextColor(TFT_BLACK,TFT_WHITE);
  roboto("launcher",icon.x+(icon.w-textWidth("launcher"))/2,icon.y+icon.h+32,TFT_BLACK);
  d.setTextSize(1);
}
void navigation() {
  if(!StrynxConfig::backButton) return;
  auto& d=*shellOutput;
  int y=d.height()-bottomHeight()/2, x=40;
  d.fillRect(16,y-24,52,48,TFT_BLACK);
  float angle=-StrynxApp::keyboard.progress*1.5707963f;
  int vx[3]={-12,9,9}, vy[3]={0,-12,12}, px[3],py[3];
  for(int i=0;i<3;++i) {
    px[i]=x+lroundf(vx[i]*cosf(angle)-vy[i]*sinf(angle));
    py[i]=y+lroundf(vx[i]*sinf(angle)+vy[i]*cosf(angle));
  }
  d.fillTriangle(px[0],py[0],px[1],py[1],px[2],py[2],TFT_WHITE);
  for(int i=0;i<3;++i) d.fillCircle(px[i],py[i],3,TFT_WHITE);
}
void render() {
  auto& d=*shellOutput;
  d.fillScreen(StrynxConfig::background);
  drawApp(); status(true);
  if(StrynxConfig::backButton) {
    d.fillRect(0,d.height()-bottomHeight(),d.width(),bottomHeight(),TFT_BLACK);
    navigation();
  }
}
// Error API for app authors: call setHome(true, "Your error") from app code,
// or expose the wrapper below. Hard panics recover after reboot, not in place.
void setHome(bool enabled,const char* reason) {
  if(enabled && !StrynxConfig::backupHomeEnabled) return;
  const bool animate=!booting && safeHome!=enabled;
  // Freeze the previous app before changing its state or freeing its caches.
  menuOpen=false; menuProgress=0; menuSnapshot=false; bootOS.deleteSprite();
  bool haveTransition=false;
  if(animate) {
    bootOS.setPsram(true); bootOS.setColorDepth(16);
    if(bootOS.createSprite(M5.Display.width(),M5.Display.height())) {
      shellOutput=&bootOS; StrynxUI::output=&bootOS;
      render();
      shellOutput=&M5.Display; StrynxUI::output=&M5.Display;
      for(int y=0;y<bootOS.height();++y) for(int x=0;x<bootOS.width();++x) {
        uint16_t c=bootOS.readPixel(x,y);
        bootOS.drawPixel(x,y,StrynxUI::blend(0x8410,c,8));
      }
      bootOS.pushSprite(0,0);
      haveTransition=true;
    }
  }
  safeHome=enabled;
  snprintf(homeReason,sizeof(homeReason),"%s",reason);
  recoveryPrefs.putBool("forced",enabled);
  if(enabled) {
    StrynxApp::keyboard.show(false); StrynxApp::keyboard.progress=0;
    StrynxApp::contentCache.deleteSprite(); StrynxApp::cacheValid=false;
    StrynxUI::frame.deleteSprite();
  } else { StrynxApp::begin(); }
  Serial.printf("HOME %s: %s\n",enabled?"backup":"app",reason);
  if(haveTransition) {
    // Reuse the same buffer for the incoming screen. The grey previous
    // screen stays on the display; do not clear or repaint it each frame.
    shellOutput=&bootOS; StrynxUI::output=&bootOS;
    render();
    shellOutput=&M5.Display; StrynxUI::output=&M5.Display;
    const uint32_t start=millis();
    uint32_t nextFrame=start;
    for(;;) {
      uint32_t now=millis();
      if(int32_t(now-nextFrame)<0) { delay(1); continue; }
      float t=StrynxConfig::appSlideDurationMs
        ? min(1.0f,float(now-start)/StrynxConfig::appSlideDurationMs) : 1.0f;
      // Enter from below the bottom edge, easing gently into place.
      float eased=t*t*(3.0f-2.0f*t);
      bootOS.pushSprite(0,lroundf((1.0f-eased)*M5.Display.height()));
      if(t>=1.0f) break;
      nextFrame=millis()+16;
      delay(1);
    }
    bootOS.deleteSprite();
  } else if(!booting) render(); // Low-memory recovery must still open home.
}
int menuHeight() { return 64+int(sizeof(StrynxMenu::items)/sizeof(StrynxMenu::items[0]))*64; }
StrynxUI::Rect menuButton(int index) {
  return {28,topHeight()+64+index*64,M5.Display.width()-56,52};
}
void drawMenu() {
  float eased=menuProgress*menuProgress*(3.0f-2.0f*menuProgress);
  int h=menuHeight(), visible=lroundf(h*eased);
  bootOS.pushSprite(0,0); // Cached grey backdrop; the app is paused beneath it.
  if(!visible) return;
  using namespace StrynxUI;
  int y=topHeight()+visible-h;
  begin(12,y,M5.Display.width()-24,h);
  clip={0,max(0,topHeight()-y),bounds.w,visible};
  if(!beginFrame()) { menuOpen=false; return; }
  // Preserve the backdrop around the rounded panel's corners.
  bootOS.pushSprite(&frame,-bounds.x-frameRect.x,-bounds.y-frameRect.y);
  panel({0,0,bounds.w,h},StrynxMenu::panelStyle);
  text("Control center",20,20,StrynxMenu::panelStyle.foreground,StrynxMenu::panelStyle.background);
  for(size_t i=0;i<sizeof(StrynxMenu::items)/sizeof(StrynxMenu::items[0]);++i)
    button({16,64+int(i)*64,bounds.w-32,52},StrynxMenu::items[i].label,StrynxMenu::buttonStyle);
  endFrame();
}

bool shellTick() {
  while(Serial.available()) {
    char c=Serial.read();
    if(c=='\n' || c=='\r') {
      serialLine[serialUsed]=0;
      if(serialOverflow) Serial.println("ERR line too long (511 bytes maximum); discarded");
      else if(serialUsed) {
        char* command=FluxShell::trim(serialLine);
        if(FluxShell::run(command,safeHome,setHome)) {}
        else if(!strcmp(command,"home on") || !strcmp(command,"app stop")) setHome(true,"Forced by serial");
        else if(!strcmp(command,"home off") || !strcmp(command,"app start")) setHome(false,"Retry requested");
        else if(!strcmp(command,"home status")) Serial.printf("home=%s heap=%u psram=%u\n",safeHome?"backup":"app",ESP.getFreeHeap(),ESP.getFreePsram());
        else Serial.println("ERR unknown command; use flux | help");
      }
      serialUsed=0; serialOverflow=false;
    } else if(!serialOverflow) {
      if(c=='\b' || c==127) { if(serialUsed) --serialUsed; }
      else if(serialUsed<sizeof(serialLine)-1) serialLine[serialUsed++]=c;
      else serialOverflow=true;
    }
  }
  if(!safeHome && StrynxApp::errorMessage) {
    setHome(true,StrynxApp::errorMessage); StrynxApp::errorMessage=nullptr;
  }
  static uint32_t memoryAt=0;
  if(!safeHome && millis()-memoryAt>500) {
    memoryAt=millis();
    if(ESP.getFreeHeap()<StrynxConfig::minimumFreeHeap ||
       (ESP.getPsramSize() && ESP.getFreePsram()<StrynxConfig::minimumFreePsram))
      setHome(true,"App closed: low memory");
  }
  if(M5.Touch.getCount()) {
    auto t=M5.Touch.getDetail();
    if(t.wasPressed()) {
      menuGesture=t.y<topHeight(); gestureY=t.y;
      menuTap=menuProgress>0;
      if(safeHome && !menuTap && launcherIcon().contains(t.x,t.y)) setHome(false,"Launcher icon");
    }
    menuTouchX=t.x; menuTouchY=t.y;
    if(menuGesture && t.y-gestureY>24) menuOpen=true;
    if(menuGesture || menuTap) return true;
  } else {
    if(menuTap && menuProgress>=1) {
      bool inside=StrynxUI::Rect{12,topHeight(),M5.Display.width()-24,menuHeight()}.contains(menuTouchX,menuTouchY);
      for(size_t i=0;i<sizeof(StrynxMenu::items)/sizeof(StrynxMenu::items[0]);++i) {
        if(!menuButton(i).contains(menuTouchX,menuTouchY)) continue;
        int action=StrynxMenu::items[i].action;
        if(action==0) setHome(true,"Selected from menu");
        else if(action==1) setHome(false,"Selected from menu");
        else StrynxMenu::onAction(action);
        inside=false; break;
      }
      // Blank space inside the panel stays open; tapping the backdrop dismisses.
      if(!inside) menuOpen=false;
    }
    menuGesture=false; menuTap=false;
  }
  if(menuOpen || menuProgress>0) {
    if(!menuSnapshot) {
      bootOS.setPsram(true); bootOS.setColorDepth(16);
      if(!bootOS.createSprite(M5.Display.width(),M5.Display.height())) { menuOpen=false; menuProgress=0; return safeHome; }
      shellOutput=&bootOS; StrynxUI::output=&bootOS; render();
      shellOutput=&M5.Display; StrynxUI::output=&M5.Display; menuSnapshot=true; menuTick=millis();
      // Apply a grey veil once, not on every animation frame.
      for(int y=0;y<bootOS.height();++y) for(int x=0;x<bootOS.width();++x) {
        uint16_t c=bootOS.readPixel(x,y);
        bootOS.drawPixel(x,y,StrynxUI::blend(0x8410,c,8));
      }
    }
    uint32_t now=millis();
    if(now-menuTick>=16) {
      float step=float(now-menuTick)/StrynxConfig::menuAnimationMs; menuTick=now;
      float before=menuProgress;
      menuProgress=menuOpen?min(1.0f,menuProgress+step):max(0.0f,menuProgress-step);
      if(before!=menuProgress) drawMenu();
      if(menuProgress==0) { bootOS.deleteSprite(); menuSnapshot=false; render(); }
    }
    return true;
  }
  return safeHome;
}
void bootImage() {
  auto& d=*shellOutput;
  d.fillScreen(TFT_BLACK);
  d.pushImage((d.width()-CUSTOM_BOOT_SCREEN_WIDTH)/2,
              (d.height()-CUSTOM_BOOT_SCREEN_HEIGHT)/2,
              CUSTOM_BOOT_SCREEN_WIDTH,CUSTOM_BOOT_SCREEN_HEIGHT,CUSTOM_BOOT_SCREEN_DATA);

}
}
void setup() {
  auto cfg=M5.config();
  // Retain official Tab5 power/display/touch startup and LovyanGFX engine.
  M5.begin(cfg);
  M5.Display.setRotation(0);
  if(M5.Display.width()>M5.Display.height()) M5.Display.setRotation(1);
  M5.Display.setBrightness(160);
  Serial.begin(115200);
  recoveryPrefs.begin("strynx",false);
  esp_reset_reason_t reason=esp_reset_reason();
  safeHome=StrynxConfig::backupHomeEnabled && (recoveryPrefs.getBool("forced",false) ||
    reason==ESP_RST_PANIC || reason==ESP_RST_TASK_WDT || reason==ESP_RST_INT_WDT || reason==ESP_RST_WDT);
  if(safeHome) snprintf(homeReason,sizeof(homeReason),"Recovered after restart / forced home");
  StrynxUI::theme.accent=StrynxConfig::accent;
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
    if(elapsed>=StrynxConfig::bootDurationMs) {
      if(!StrynxConfig::bootAnimation) { booting=false; render(); }
      else {
        if(!slidePrepared) {
          bootOS.setPsram(true);
          bootOS.setColorDepth(16);
          if(!bootOS.createSprite(M5.Display.width(),M5.Display.height())) {
            booting=false; render(); return;
          }
          // Build the actual shell off-screen once, never flash it before the slide.
          shellOutput=&bootOS; StrynxUI::output=&bootOS;
          render();
          shellOutput=&M5.Display; StrynxUI::output=&M5.Display;
          slidePrepared=true; slideStart=millis();
        }
        uint32_t now=millis();
        if(now-lastSlideFrame>=16) {
          lastSlideFrame=now;
          float t=StrynxConfig::bootSlideDurationMs
            ? min(1.0f,float(now-slideStart)/StrynxConfig::bootSlideDurationMs) : 1.0f;
          float eased=t*t*(3.0f-2.0f*t);
          int y=lroundf((1.0f-eased)*M5.Display.height());
          // Each frame covers more of the static black/logo background.
          bootOS.pushSprite(0,y);
          if(t>=1.0f) { booting=false; bootOS.deleteSprite(); }
        }
      }
    }
    delay(1); return;
  }
  if(shellTick()) { delay(1); return; }
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
    static float lastNavigation=-1;
    if(lastNavigation!=StrynxApp::keyboard.progress) { navigation(); lastNavigation=StrynxApp::keyboard.progress; }
    appDirty=false;
    lastAppFrame=millis();
  }
  if(millis()-lastStatusCheck>=1000) { lastStatusCheck=millis(); status(); }
  delay(1);
}
