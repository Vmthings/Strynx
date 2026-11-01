#pragma once
#include <M5Unified.h>
#include "strynx_ui.h"

// REQUIRED CONFIGURATION — change values, DO NOT DELETE these declarations.
// These are compile-time settings, not a security boundary.
namespace StrynxConfig {
constexpr bool bootAnimation = true;
constexpr bool bootScreen = true;
constexpr bool backButton = true;
constexpr bool topbar = true;
constexpr uint32_t bootDurationMs = 1800;
constexpr uint16_t background = 0xFFFF;
constexpr uint16_t accent = 0x07FF;
constexpr uint16_t foreground = 0x0000;
constexpr const char* appName = "Flux app";
}

// APP AREA: replace these functions with your application.
// Coordinates passed to appTouch are relative to the content area.
// Draw only inside x,y,w,h so the shell's bars remain visible.
namespace StrynxApp {
inline int taps=0;
inline M5Canvas contentCache(&M5.Display);
inline bool cacheValid=false;
inline int cacheWidth=0;
inline StrynxUI::Scroll scroll;
inline StrynxUI::Rect tapButton{24,150,260,64};
inline StrynxUI::Rect damage{0,0,0,0};
// Merge changes until the next frame, so touch and fade cannot lose updates.
inline void invalidate(StrynxUI::Rect r) {
  if(r.w<=0 || r.h<=0) return;
  if(damage.w<=0) { damage=r; return; }
  int right=max(damage.x+damage.w,r.x+r.w);
  int bottom=max(damage.y+damage.h,r.y+r.h);
  damage.x=min(damage.x,r.x); damage.y=min(damage.y,r.y);
  damage.w=right-damage.x; damage.h=bottom-damage.y;
}
inline void begin() { taps=0; }

inline void draw(int x,int y,int w,int h,bool full=true) {
  using namespace StrynxUI;
  theme.background=StrynxConfig::background;
  theme.foreground=StrynxConfig::foreground;
  StrynxUI::begin(x,y,w,h);
  if(!full) {
    if(damage.w<=0 || damage.h<=0) return;
    clip=damage;
  }
  if(!beginFrame()) return; // Keep damage pending if allocation fails.
  damage={0,0,0,0};
  clear();
  label("Strynx UI",28,24,2);
  scroll.viewport={24,100,w-48,h-124};
  scroll.contentHeight=max(1500,h+400);
  scroll.background=theme.background;
  // Rasterize static panels/text once, not on every drag sample.
  if(!cacheValid || cacheWidth!=w) {
    contentCache.setPsram(true);
    contentCache.setColorDepth(16);
    bool allocated=true;
    if(contentCache.width()!=scroll.viewport.w || contentCache.height()!=scroll.contentHeight)
      allocated=contentCache.createSprite(scroll.viewport.w,scroll.contentHeight)!=nullptr;
    if(allocated) {
      auto oldBounds=bounds; auto oldClip=clip; auto oldFrame=frameRect;
      int oldX=offsetX,oldY=offsetY;
      bounds={0,0,scroll.viewport.w,scroll.contentHeight}; clip=bounds;
      frameRect=bounds; offsetX=offsetY=0; target=&contentCache;
      clear();
  Style style; // White fill, grey outline, rounded corners by default.
  panel({0,0,w-72,250},style);
  text("Your app runs here",24,24,style.foreground,style.background);
  char count[40]; snprintf(count,sizeof(count),"Touches: %d",taps);
  text(count,24,76,style.foreground,style.background);
  button(tapButton,"Tap me");
  for(int i=0;i<5;++i) {
    int py=280+i*230;
    panel({0,py,w-72,200},style);
    char title[32]; snprintf(title,sizeof(title),"Panel %d",i+1);
    text(title,24,py+28,style.foreground,style.background);
    text("Drag up or down",24,py+80,style.foreground,style.background);
  }
      target=&frame; bounds=oldBounds; clip=oldClip; frameRect=oldFrame;
      offsetX=oldX; offsetY=oldY;
      cacheValid=true; cacheWidth=w;
    }
  }
  if(cacheValid) {
    // The damage-sized destination canvas clips the cached image automatically.
    frame.setClipRect(max(0,scroll.viewport.x-frameRect.x),max(0,scroll.viewport.y-frameRect.y),
                      min(frameRect.w,scroll.viewport.w),min(frameRect.h,scroll.viewport.h));
    contentCache.pushSprite(&frame,scroll.viewport.x-frameRect.x,
                           scroll.viewport.y-scroll.position-frameRect.y);
    frame.clearClipRect();
  } else {
    scroll.beginContent();
  Style style; // White fill, grey outline, rounded corners by default.
  panel({0,0,w-72,250},style);
  text("Your app runs here",24,24,style.foreground,style.background);
  char count[40]; snprintf(count,sizeof(count),"Touches: %d",taps);
  text(count,24,76,style.foreground,style.background);
  button(tapButton,"Tap me");
  for(int i=0;i<5;++i) {
    int py=280+i*230;
    panel({0,py,w-72,200},style);
    char title[32]; snprintf(title,sizeof(title),"Panel %d",i+1);
    text(title,24,py+28,style.foreground,style.background);
    text("Drag up or down",24,py+80,style.foreground,style.background);
  }
    scroll.endContent();
  }
  scroll.drawBar();
  endFrame();
}
// Continuous touch samples support dragging; buttons activate on release.
inline bool pointer(int x,int y,bool down) {
  bool wasDragging=scroll.dragging;
  bool changed=scroll.touch(x,y,down);
  if(changed) invalidate(scroll.viewport);
  if(!down && wasDragging && !scroll.moved && scroll.viewport.contains(x,y)) {
    int localX=x-scroll.viewport.x;
    int localY=y-scroll.viewport.y+scroll.position;
    if(StrynxUI::tapped(tapButton,localX,localY)) {
      ++taps; cacheValid=false; changed=true;
      // Only the counter line changes, not the button or surrounding panel.
      int top=max(scroll.viewport.y,scroll.viewport.y+76-scroll.position);
      int bottom=min(scroll.viewport.y+scroll.viewport.h,scroll.viewport.y+112-scroll.position);
      invalidate({scroll.viewport.x+24,top,scroll.viewport.w-48,bottom-top});
    }
  }
  return changed;
}
inline bool back() { cacheValid=false; taps=0; scroll.position=0; return true; }
inline bool update() {
  if(!scroll.tick()) return false;
  // Repaint the underlying content in this strip before blending the bar.
  invalidate({scroll.viewport.x+scroll.viewport.w-scroll.barWidth-3,
              scroll.viewport.y,scroll.barWidth+3,scroll.viewport.h});
  return true;
}
}
