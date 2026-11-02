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
constexpr uint32_t bootSlideDurationMs = 650;
constexpr uint16_t background = 0xFFFF;
constexpr uint16_t accent = 0x219F; // Main blue theme, RGB565.
constexpr bool backupHomeEnabled = true;
constexpr uint32_t minimumFreeHeap = 48000;
constexpr uint32_t minimumFreePsram = 160000;
constexpr uint32_t menuAnimationMs = 260;
constexpr uint32_t appSlideDurationMs = 220; // Smooth bottom-up home/app transition.
constexpr uint16_t foreground = 0x0000;
constexpr const char* appName = "Flux app";
}

// Control center customization: labels, action IDs, and Strynx UI styles.
// Action IDs 0 and 1 are handled by the shell. Other IDs call your callback.
namespace StrynxMenu {
inline StrynxUI::Style panelStyle{0xFFFF,0xBDF7,0x0000,16,2};
inline StrynxUI::Style buttonStyle{StrynxConfig::accent,StrynxConfig::accent,0xFFFF,12,2};
struct Item { const char* label; int action; };
inline const Item items[]={{"Backup home",0},{"Open main app",1}};
inline void onAction(int action) { (void)action; /* Your custom action here. */ }
}

// APP AREA: replace these functions with your application.
// Coordinates passed to appTouch are relative to the content area.
// Draw only inside x,y,w,h so the shell's bars remain visible.
namespace StrynxApp {
// App authors: call fail("Explanation") then return from your callback.
// The shell closes the app and displays the error on backup home.
inline const char* errorMessage=nullptr;
inline void fail(const char* message) { errorMessage=message; }
inline StrynxUI::Keyboard keyboard;
inline char input[96]="";
inline size_t cursor=0;
inline bool cursorOn=true;
inline uint32_t cursorBlink=0;
inline int appWidth=0, appHeight=0;
inline bool fieldDown=false;
inline int taps=0;
inline const char* const matrixLabels[]={"One","Two","Three","Four","Five","Six"};
inline StrynxUI::ButtonMatrix matrix;
inline int matrixPress=-1;
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
inline void begin() {
  taps=0; matrix.labels=matrixLabels; matrix.count=6; matrix.columns=3;
}

inline void draw(int x,int y,int w,int h,bool full=true) {
  using namespace StrynxUI;
  appWidth=w; appHeight=h;
  keyboard.layout(w,h);
  theme.background=StrynxConfig::background;
  theme.foreground=StrynxConfig::foreground;
  StrynxUI::begin(x,y,w,h);
  if(!full) {
    if(damage.w<=0 || damage.h<=0) return;
    clip=damage;
  }
  if(!beginFrame()) { fail("App closed: render allocation failed"); return; }
  damage={0,0,0,0};
  clear();
  panel({24,16,w-48,64});
  // Keep the insertion point in view without changing the stored text.
  size_t first=0;
  char prefix[96];
  while(true) {
    memcpy(prefix,input+first,cursor-first); prefix[cursor-first]=0;
    if(textWidth(prefix)<=w-96 || first==cursor) break;
    ++first;
  }
  Rect fieldClip=clip;
  int left=max(clip.x,36), right=min(clip.x+clip.w,w-36);
  int top=max(clip.y,20), bottom=min(clip.y+clip.h,76);
  clip={left,top,max(0,right-left),max(0,bottom-top)};
  text(input[0]?input+first:(keyboard.visible?"":"Tap to type"),36,34,theme.foreground,theme.background);
  if(keyboard.visible && cursorOn) {
    int cx=36+textWidth(prefix);
    for(int yy=31;yy<61;++yy) span(cx,yy,2,theme.foreground);
  }
  clip=fieldClip;
  scroll.viewport={24,100,w-48,h-124};
  scroll.contentHeight=max(1500,h+400);
  scroll.background=theme.background;
  matrix.rect={24,334,w-120,126};
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
    if(i==0) {
      text("Button matrix",24,py+16,style.foreground,style.background);
      matrix.draw();
    } else {
      text(title,24,py+28,style.foreground,style.background);
      text("Drag up or down",24,py+80,style.foreground,style.background);
    }
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
    if(i==0) {
      text("Button matrix",24,py+16,style.foreground,style.background);
      matrix.draw();
    } else {
      text(title,24,py+28,style.foreground,style.background);
      text("Drag up or down",24,py+80,style.foreground,style.background);
    }
  }
    scroll.endContent();
  }
  scroll.drawBar();
  keyboard.draw();
  endFrame();
}
// Continuous touch samples support dragging; buttons activate on release.
inline bool pointer(int x,int y,bool down) {
  if(keyboard.progress>0 && (y>=keyboard.rect.y || keyboard.held)) {
    int key=keyboard.pointer(x,y,down);
    if(key>=0) {
      int row=key/16, col=key%16;
      size_t n=strlen(input);
      char c=0;
      if(row==1 && col==0) keyboard.shift=!keyboard.shift;
      else if(row==1 && col==10) {
        if(cursor) { memmove(input+cursor-1,input+cursor,n-cursor+1); --cursor; }
      }
      else if(row==2 && col==0) keyboard.numbers=!keyboard.numbers;
      else if(row==2 && col==8) keyboard.show(false);
      else if(row==2 && col==9) c='.';
      else if(row==3 && col==0) { if(cursor) --cursor; }
      else if(row==3 && col==1) { if(cursor<n) ++cursor; }
      else if(row==3 && col==2) c=' ';
      else {
        c=keyboard.rows[row].labels[col][0];
        if(!keyboard.shift && c>='A' && c<='Z') c+=('a'-'A');
      }
      if(c && n<sizeof(input)-1) {
        memmove(input+cursor+1,input+cursor,n-cursor+1);
        input[cursor++]=c;
      }
      keyboard.layout(appWidth,appHeight);
      cursorOn=true; cursorBlink=millis();
      invalidate(keyboard.rect);
      invalidate({24,16,appWidth-48,64});
      return true;
    }
    return false;
  }
  if(down && !scroll.dragging && y<90) { fieldDown=true; return false; }
  if(!down && fieldDown) {
    fieldDown=false;
    if(y<90) { keyboard.show(true); scroll.dragging=false; }
    return false;
  }
  bool wasDragging=scroll.dragging;
  if(down && !wasDragging)
    matrixPress=scroll.viewport.contains(x,y) ? matrix.hitTest(x-scroll.viewport.x,y-scroll.viewport.y+scroll.position) : -1;
  bool changed=scroll.touch(x,y,down);
  if(changed) invalidate(scroll.viewport);
  if(!down && wasDragging && !scroll.moved && scroll.viewport.contains(x,y)) {
    int localX=x-scroll.viewport.x;
    int localY=y-scroll.viewport.y+scroll.position;
    int hit=matrix.hitTest(localX,localY);
    if(hit==matrixPress && matrix.select(hit)) {
      cacheValid=false; changed=true;
      int top=max(scroll.viewport.y,scroll.viewport.y+matrix.rect.y-scroll.position);
      int bottom=min(scroll.viewport.y+scroll.viewport.h,scroll.viewport.y+matrix.rect.y+matrix.rect.h-scroll.position);
      invalidate({scroll.viewport.x+matrix.rect.x,top,matrix.rect.w,bottom-top});
    }
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
inline bool back() { if(keyboard.visible) { keyboard.show(false); return false; } matrix.selected=-1; matrixPress=-1; cacheValid=false; taps=0; scroll.position=0; return true; }
inline bool update() {
  bool caretChanged=false;
  if(keyboard.visible && millis()-cursorBlink>=500) {
    cursorBlink=millis(); cursorOn=!cursorOn; caretChanged=true;
    invalidate({24,16,appWidth-48,64});
  }
  bool animated=keyboard.tick();
  if(animated) {
    keyboard.layout(appWidth,appHeight);
    invalidate({0,appHeight-min(320,appHeight/2),appWidth,min(320,appHeight/2)});
  }
  if(!scroll.tick()) return animated || caretChanged;
  // Repaint the underlying content in this strip before blending the bar.
  invalidate({scroll.viewport.x+scroll.viewport.w-scroll.barWidth-3,
              scroll.viewport.y,scroll.barWidth+3,scroll.viewport.h});
  return true;
}
}
