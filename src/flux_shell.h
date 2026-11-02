#pragma once
#include <Arduino.h>
#include <cstring>

// Fixed-size RAM storage. No shell execution, dynamic allocation or flash writes.
namespace FluxShell {
enum class Kind { Empty, Text, App };
struct Value { Kind kind=Kind::Empty; char text[160]={}; };
inline Value saved;
using HomeAction=void (*)(bool,const char*);
inline char* trim(char* s) {
  while(*s==' ' || *s=='\t') ++s;
  size_t n=strlen(s);
  while(n && (s[n-1]==' ' || s[n-1]=='\t')) s[--n]=0;
  return s;
}
inline void put(Value& value,Kind kind,const char* text) {
  value.kind=kind; snprintf(value.text,sizeof(value.text),"%s",text);
}
inline bool evaluate(const char* command,Value& out,bool home) {
  if(!strncmp(command,"get ",4)) {
    const char* name=command+4;
    if(strcmp(name,"launcher") && strcmp(name,"main")) {
      Serial.println("ERR unknown app; use get launcher or get main"); return false;
    }
    put(out,Kind::App,name);
  } else if(!strcmp(command,"status")) {
    out.kind=Kind::Text;
    snprintf(out.text,sizeof(out.text),"home=%s heap=%u psram=%u",home?"backup":"app",ESP.getFreeHeap(),ESP.getFreePsram());
  } else if(!strcmp(command,"apps")) put(out,Kind::Text,"launcher main");
  else { Serial.println("ERR unknown command; use flux | help"); return false; }
  return true;
}
inline void help() {
  Serial.println("flux | grep get launcher | grab launch");
  Serial.println("flux | grep get main | grab launch");
  Serial.println("grep VALUE stores text/numbers; grep get APP or grep status stores results.");
  Serial.println("grab prints stored value; grab launch launches a stored app.");
  Serial.println("get APP, apps, status, help do not change the stored value.");
  Serial.println("grep clear resets RAM storage. Values persist between lines until reboot.");
}
inline bool run(char* line,bool& home,HomeAction action) {
  char* first=trim(line);
  if(strncmp(first,"flux",4) || (first[4] && first[4]!=' ' && first[4]!='\t' && first[4]!='|')) return false;
  first=trim(first+4);
  if(!*first) { help(); return true; }
  if(*first!='|') { Serial.println("ERR expected flux | COMMAND"); return true; }
  char* stage=first+1;
  while(stage) {
    char* next=strchr(stage,'|');
    if(next) *next++=0;
    char* command=trim(stage);
    if(!*command) { Serial.println("ERR empty pipeline stage"); break; }
    if(!strcmp(command,"help")) help();
    else if(!strcmp(command,"grep clear")) { saved=Value{}; Serial.println("OK variable cleared"); }
    else if(!strncmp(command,"grep ",5)) {
      char* expression=trim(command+5);
      if(!*expression) { Serial.println("ERR grep needs a value"); break; }
      Value value;
      if(!strncmp(expression,"get ",4) || !strcmp(expression,"status") || !strcmp(expression,"apps")) {
        if(!evaluate(expression,value,home)) break;
      } else {
        if(strlen(expression)>=sizeof(value.text)) { Serial.println("ERR stored value too long (159 bytes maximum)"); break; }
        put(value,Kind::Text,expression);
      }
      saved=value; Serial.printf("stored=%s\n",saved.text);
    } else if(!strcmp(command,"grab")) {
      if(saved.kind==Kind::Empty) { Serial.println("ERR no stored value"); break; }
      Serial.println(saved.text);
    } else if(!strcmp(command,"grab launch")) {
      if(saved.kind!=Kind::App) { Serial.println("ERR select an app with grep get APP first"); break; }
      bool launcher=!strcmp(saved.text,"launcher");
      if(launcher && !StrynxConfig::backupHomeEnabled) { Serial.println("ERR backup home is disabled in app.h"); break; }
      action(launcher,"Launched by Flux serial command");
    } else {
      Value output;
      if(!evaluate(command,output,home)) break;
      Serial.println(output.text);
    }
    stage=next;
  }
  return true;
}
}
