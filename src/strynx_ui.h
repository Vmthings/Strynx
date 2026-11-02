//v2
//added keyboard
//added button matrix
//and idk what else
#pragma once
#include <M5Unified.h>
#include "montserrat24.h"

// Lightweight immediate-mode UI on the existing LovyanGFX display.
// Coordinates are local to the app; begin() sets its bounds. Redraw on changes only.
namespace StrynxUI {
struct Rect {
  int x,y,w,h;
  bool contains(int px,int py) const { return px>=x && py>=y && px<x+w && py<y+h; }
};
struct Theme {
  uint16_t background=0x0843, surface=0x18E5, foreground=0xFFFF, accent=0x07FF;
};
inline Theme theme;
inline Rect bounds{0,0,0,0};
inline Rect clip{0,0,0,0};
inline int offsetX=0, offsetY=0;
// Compose only the damaged rectangle in PSRAM. Never expose its clear pass.
inline lgfx::LovyanGFX* output=&M5.Display;
inline M5Canvas frame(&M5.Display);
inline M5Canvas* target=&frame;
inline Rect frameRect{0,0,0,0};
inline bool beginFrame() {
  int left=max(0,clip.x), top=max(0,clip.y);
  int right=min(bounds.w,clip.x+clip.w), bottom=min(bounds.h,clip.y+clip.h);
  if(right<=left || bottom<=top) return false;
  frameRect={left,top,right-left,bottom-top};
  target=&frame;
  frame.setPsram(true);
  frame.setColorDepth(16);
  if(frame.width()!=frameRect.w || frame.height()!=frameRect.h) {
    if(!frame.createSprite(frameRect.w,frameRect.h)) return false;
  }
  return true;
}
inline void endFrame() {
  frame.pushSprite(output,bounds.x+frameRect.x,bounds.y+frameRect.y);
}

inline void begin(int x,int y,int w,int h) { bounds={x,y,w,h}; clip={0,0,w,h}; offsetX=offsetY=0; }
inline void pixel(int x,int y,uint16_t color) {
  x+=offsetX; y+=offsetY;
  if(clip.contains(x,y) && x>=0 && y>=0 && x<bounds.w && y<bounds.h) target->drawPixel(x-frameRect.x,y-frameRect.y,color);
}
// Clip before sending a single accelerated span to LovyanGFX.
inline void span(int x,int y,int width,uint16_t color) {
  x+=offsetX; y+=offsetY;
  if(y<max(0,clip.y) || y>=min(bounds.h,clip.y+clip.h)) return;
  int left=max(x,max(0,clip.x));
  int right=min(x+width,min(bounds.w,clip.x+clip.w));
  if(right>left) target->drawFastHLine(left-frameRect.x,y-frameRect.y,right-left,color);
}
inline uint16_t blend(uint16_t fg,uint16_t bg,int a) {
  return ((((fg>>11)*a+(bg>>11)*(15-a))/15)<<11)
    | (((((fg>>5)&63)*a+((bg>>5)&63)*(15-a))/15)<<5)
    | (((fg&31)*a+(bg&31)*(15-a))/15);
}
inline int textWidth(const char* text,int scale=1) {
  int advance=0;
  while(*text) { unsigned char c=*text++; if(c<32 || c>126) c='?'; advance+=strynxMontserrat24Glyphs[c-32].advance; }
  return ((advance+8)/16)*scale;
}
// Top-left text positioning. 24px Montserrat, or integer enlargement for titles.
// Pass the actual background color for smooth anti-aliased edges.
inline void text(const char* value,int x,int y,uint16_t fg,uint16_t bg,int scale=1) {
  if(scale<1) scale=1;
  int advance=0, origin=x;
  while(*value) {
    unsigned char c=*value++; if(c<32 || c>126) c='?';
    const auto& g=strynxMontserrat24Glyphs[c-32];
    x=origin+((advance+8)/16)*scale;
    int glyphX=x+g.x*scale+offsetX;
    int glyphY=y+(22-g.height-g.y)*scale+offsetY;
    if(glyphX+g.width*scale<=clip.x || glyphX>=clip.x+clip.w ||
       glyphY+g.height*scale<=clip.y || glyphY>=clip.y+clip.h) {
      advance+=g.advance; continue;
    }
    for(int row=0;row<g.height;++row) for(int col=0;col<g.width;++col) {
      int n=row*g.width+col;
      int a=(strynxMontserrat24Bitmap[g.offset+n/2]>>(n%2?0:4))&15;
      if(!a) continue;
      auto color=blend(fg,bg,a);
      for(int sy=0;sy<scale;++sy) for(int sx=0;sx<scale;++sx)
        pixel(x+(g.x+col)*scale+sx,y+(22-g.height-g.y+row)*scale+sy,color);
    }
    advance+=g.advance;
  }
}
inline void label(const char* value,int x,int y,int scale=1) { text(value,x,y,theme.foreground,theme.background,scale); }
// Respect the current damage rectangle, just like every other primitive.
inline void clear() {
  int left=max(0,clip.x), top=max(0,clip.y);
  int right=min(bounds.w,clip.x+clip.w), bottom=min(bounds.h,clip.y+clip.h);
  if(right>left && bottom>top)
    target->fillRect(left-frameRect.x,top-frameRect.y,right-left,bottom-top,theme.background);
}
inline void card(Rect r,uint16_t color) {
  // Clip locally without changing the shell's graphics state.
  int radius=12;
  for(int y=0;y<r.h;++y) for(int x=0;x<r.w;++x) {
    int dx=x<radius?radius-x:(x>=r.w-radius?x-(r.w-radius-1):0);
    int dy=y<radius?radius-y:(y>=r.h-radius?y-(r.h-radius-1):0);
    if(dx*dx+dy*dy<=radius*radius) pixel(r.x+x,r.y+y,color);
  }
}
// Colors are RGB565. Customize each element independently.
struct Style {
  uint16_t background=0xFFFF, outline=0xBDF7, foreground=0x0000;
  int radius=16, borderWidth=2;
};
inline bool roundedContains(int x,int y,int w,int h,int radius) {
  if(x<0 || y<0 || x>=w || y>=h) return false;
  int r=max(0,min(radius,min(w,h)/2));
  int dx=x<r?r-x:(x>=w-r?x-(w-r-1):0);
  int dy=y<r?r-y:(y>=h-r?y-(h-r-1):0);
  return dx*dx+dy*dy<=r*r;
}
inline void panel(Rect r,const Style& style=Style{}) {
  if(r.w<=0 || r.h<=0) return;
  int b=max(0,min(style.borderWidth,min(r.w,r.h)/2));
  int first=max(0,max(0,clip.y)-offsetY-r.y);
  int last=min(r.h,min(bounds.h,clip.y+clip.h)-offsetY-r.y);
  if(r.x+offsetX>=min(bounds.w,clip.x+clip.w) || r.x+r.w+offsetX<=max(0,clip.x)) return;
  // Only the corner edges need testing; solid interiors are hardware spans.
  for(int y=first;y<last;++y) {
    int edge=0;
    while(edge<r.w/2 && !roundedContains(edge,y,r.w,r.h,style.radius)) ++edge;
    int inner=edge;
    bool hasInner=b==0;
    if(b>0 && y>=b && y<r.h-b && r.w>2*b) {
      inner=b;
      while(inner<r.w/2 && !roundedContains(inner-b,y-b,r.w-2*b,r.h-2*b,max(0,style.radius-b))) ++inner;
      hasInner=inner<r.w/2;
    }
    if(!hasInner) span(r.x+edge,r.y+y,r.w-2*edge,style.outline);
    else {
      span(r.x+edge,r.y+y,inner-edge,style.outline);
      span(r.x+inner,r.y+y,r.w-2*inner,style.background);
      span(r.x+r.w-inner,r.y+y,inner-edge,style.outline);
    }
  }
}
inline void button(Rect r,const char* title,const Style& style=Style{}) {
  panel(r,style);
  text(title,r.x+(r.w-textWidth(title))/2,r.y+(r.h-27)/2,style.foreground,style.background);
}
inline bool tapped(Rect r,int x,int y) { return r.contains(x,y); }

// Reusable grid; labels remain owned by the app. Touch coordinates are local
// to the same content space as rect. select() returns true on selection change.
struct ButtonMatrix {
  Rect rect{0,0,300,140};
  const char* const* labels=nullptr;
  int count=0, columns=3, gap=10, selected=-1;
  const int* columnWeights=nullptr; // Optional relative widths, one per column.
  Style style{};
  Style selectedStyle{0xDEFB,0x8410,0x0000,16,2};
  int rows() const { return columns>0 ? (max(0,count)+columns-1)/columns : 0; }
  Rect cell(int index) const {
    int n=rows();
    if(index<0 || index>=count || !n) return {0,0,0,0};
    int spacing=max(0,gap);
    int width=rect.w-(columns-1)*spacing;
    int height=rect.h-(n-1)*spacing;
    if(width<columns || height<n) return {0,0,0,0};
    int col=index%columns, row=index/columns;
    int total=columns, before=col, weight=1;
    if(columnWeights) {
      total=0; before=0;
      for(int c=0;c<columns;++c) {
        int v=max(1,columnWeights[c]);
        total+=v; if(c<col) before+=v;
      }
      weight=max(1,columnWeights[col]);
    }
    int left=before*width/total+col*spacing;
    int top=row*height/n+row*spacing;
    return {rect.x+left,rect.y+top,(before+weight)*width/total-before*width/total,
            (row+1)*height/n-row*height/n};
  }
  int hitTest(int x,int y) const {
    if(!rect.contains(x,y)) return -1;
    for(int i=0;i<count;++i) {
      Rect r=cell(i);
      const Style& st=i==selected?selectedStyle:style;
      if(r.contains(x,y) && roundedContains(x-r.x,y-r.y,r.w,r.h,st.radius)) return i;
    }
    return -1;
  }
  bool select(int index) {
    if(index<0 || index>=count || index==selected) return false;
    selected=index; return true;
  }
  void draw() const {
    if(!labels) return;
    for(int i=0;i<count;++i) {
      Rect r=cell(i);
      if(r.w>0 && r.h>0) button(r,labels[i]?labels[i]:"",i==selected?selectedStyle:style);
    }
  }
};

// QWERTY keyboard composed exclusively of ButtonMatrix rows.
struct Keyboard {
  ButtonMatrix rows[4];
  const char* row0[10]={"Q","W","E","R","T","Y","U","I","O","P"};
  const char* row1[11]={"Shift","A","S","D","F","G","H","J","K","L","Del"};
  const char* row2[10]={"123","Z","X","C","V","B","N","M","Enter","."};
  const char* row3[3]={"<",">","Space"};
  const char* number0[10]={"1","2","3","4","5","6","7","8","9","0"};
  const char* number1[11]={"Shift","!","@","#","$","%","^","&","*","(","Del"};
  const char* number2[10]={"ABC",")","-","_","=","+","/","?","Enter","."};
  const int thirdWeights[10]={1,1,1,1,1,1,1,1,2,1};
  const int bottomWeights[3]={1,1,8};
  bool numbers=false;
  bool visible=false, held=false, moved=false, shift=false;
  float progress=0, start=0;
  uint32_t started=0;
  int pressed=-1, startX=0,startY=0;
  Rect rect{0,0,0,0};
  void show(bool value) {
    if(visible==value) return;
    visible=value; start=progress; started=millis(); held=false;
  }
  bool tick() {
    float t=min(1.0f,float(millis()-started)/240.0f);
    float eased=t*t*(3-2*t);
    float next=start+((visible?1.0f:0.0f)-start)*eased;
    if(next==progress) return false;
    progress=next; return true;
  }
  void layout(int w,int h) {
    int height=min(320,h/2);
    rect={0,h-int(height*progress),w,height};
    const char* const* labels[]={numbers?number0:row0,numbers?number1:row1,numbers?number2:row2,row3};
    int counts[]={10,11,10,3};
    for(int i=0;i<4;++i) {
      int inset=12;
      rows[i].labels=labels[i]; rows[i].count=counts[i]; rows[i].columns=counts[i];
      rows[i].columnWeights=i==2?thirdWeights:(i==3?bottomWeights:nullptr);
      rows[i].gap=6;
      rows[i].rect={inset,rect.y+10+i*(height-20)/4,w-2*inset,(height-20)/4-6};
      rows[i].style.radius=10;
    }
  }
  int hit(int x,int y) const {
    for(int i=0;i<4;++i) { int k=rows[i].hitTest(x,y); if(k>=0) return i*16+k; }
    return -1;
  }
  // Returns a key on release; dragging never types.
  int pointer(int x,int y,bool down) {
    if(down) {
      if(!held) { held=true; moved=false; startX=x; startY=y; pressed=hit(x,y); }
      if(abs(x-startX)>12 || abs(y-startY)>12) moved=true;
      return -1;
    }
    int key=held && !moved && hit(x,y)==pressed?pressed:-1;
    held=false; pressed=-1; return key;
  }
  void draw() const {
    if(progress<=0) return;
    Style background; background.background=0xEF7D; background.radius=0; background.borderWidth=0;
    panel(rect,background);
    for(int i=0;i<4;++i) rows[i].draw();
  }
};

// Vertical touch scrolling. Draw content between beginContent/endContent.
// Feed every touch sample (including release) into touch(), then tick() each loop.
struct Scroll {
  Rect viewport{0,0,0,0};
  int contentHeight=0, position=0;
  uint16_t barColor=0x8410, background=0x0843;
  int barWidth=6;
  uint32_t idleMs=2000, fadeMs=350;
  bool dragging=false, moved=false, visible=false;
  int lastY=0, startY=0, opacity=0;
  uint32_t lastMotion=0;
  Rect savedClip{};
  int savedX=0,savedY=0;
  int maximum() const { return max(0,contentHeight-viewport.h); }
  bool touch(int x,int y,bool down) {
    if(!down) { dragging=false; return false; }
    if(!dragging) {
      if(!viewport.contains(x,y)) return false;
      dragging=true; moved=false; startY=lastY=y; return false;
    }
    int delta=lastY-y; lastY=y;
    if(abs(y-startY)>8) moved=true;
    int next=max(0,min(maximum(),position+delta));
    if(next==position) return false;
    position=next; visible=true; opacity=15; lastMotion=millis(); return true;
  }
  bool tick() {
    int next=opacity;
    if(visible) {
      uint32_t age=millis()-lastMotion;
      next=age<=idleMs?15:(age-idleMs>=fadeMs?0:15-int((age-idleMs)*15/max(uint32_t(1),fadeMs)));
      if(!next) visible=false;
    }
    if(next==opacity) return false;
    opacity=next; return true;
  }
  void beginContent() {
    position=max(0,min(maximum(),position));
    savedClip=clip; savedX=offsetX; savedY=offsetY;
    int left=max(clip.x,offsetX+viewport.x), top=max(clip.y,offsetY+viewport.y);
    int right=min(clip.x+clip.w,offsetX+viewport.x+viewport.w);
    int bottom=min(clip.y+clip.h,offsetY+viewport.y+viewport.h);
    clip={left,top,max(0,right-left),max(0,bottom-top)};
    offsetX+=viewport.x; offsetY+=viewport.y-position;
  }
  void endContent() {
    clip=savedClip; offsetX=savedX; offsetY=savedY;
  }
  // Draw after the content. Set background to the color beneath the bar.
  void drawBar() const {
    if(!opacity || maximum()==0) return;
    int height=max(24,viewport.h*viewport.h/max(1,contentHeight));
    height=min(height,viewport.h);
    int y=viewport.y+(viewport.h-height)*position/maximum();
    Style style; style.background=blend(barColor,background,opacity);
    style.outline=style.background; style.borderWidth=0; style.radius=barWidth/2;
    panel({viewport.x+viewport.w-barWidth-3,y,barWidth,height},style);
  }
};
}
