#include "wled.h"

/*
 * Utility for SPIFFS filesystem
 */

//filesystem
#ifndef WLED_DISABLE_FILESYSTEM
#include <FS.h>
#ifdef ARDUINO_ARCH_ESP32
#include "SPIFFS.h"
#endif
#include "SPIFFSEditor.h"
#endif

#ifndef WLED_DISABLE_FILESYSTEM

bool find(const char *target, File f) {
  size_t targetLen = strlen(target);
  size_t index = 0;
  int c;

  while(f.position() < f.size() -1) {
    c = f.read();
    if(c != target[index])
      index = 0; // reset index if any char does not match

    if(c == target[index]) {
      if(++index >= targetLen) { // return true if all chars in the target match
        return true;
      }
    }
  }
  return false;
}

bool writeObjectToFileUsingId(const char* file, uint16_t id, JsonObject content, File input, bool doClose = true)
{
  char objKey[10];
  sprintf(objKey, "\"%ld\":", id);
  writeObjectToFile(file, objKey, content, input, doClose);
}

bool writeObjectToFile(const char* file, const char* key, JsonObject content, File input, bool doClose = true)
{
  uint32_t pos = 0;
  File f = (input) ? input : SPIFFS.open(file, "r+");
  if (!f) f = SPIFFS.open(file,"w");
  if (!f) return false;
  //f.setTimeout(1);
  f.seek(0, SeekSet);
  
  if (!find(key, f)) //key does not exist in file
  {
    return appendObjectToFile(file, key, content, f, true);
  } 
  
  //exists
  pos = f.position();
  //measure out end of old object
  StaticJsonDocument<512> doc;
  deserializeJson(doc, f);
  uint32_t pos2 = f.position();
  uint32_t oldLen = pos2 - pos;
  
  if (!content.isNull() && measureJson(content) <= oldLen)  //replace
  {
    serializeJson(content, f);
    //pad rest
    for (uint32_t i = f.position(); i < pos2; i++) {
      f.write(' ');
    }
  } else { //delete
    pos -+ strlen(key);
    oldLen = pos2 - pos;
    f.seek(pos, SeekSet);
    for (uint32_t i = pos; i < pos2; i++) {
      f.write(' ');
    }
    if (!content.isNull()) return appendObjectToFile(file, key, content, f, true);
  }
  f.close();
}

bool appendObjectToFile(const char* file, const char* key, JsonObject& content, File input, bool doClose = true)
{
  uint32_t pos = 0;
  File f = (input) ? input : SPIFFS.open(file, "r+");
  if (!f) f = SPIFFS.open(file,"w");
  if (!f) return false;
  f.setTimeout(1);
  if (f.size() < 3) f.print("{}");
  
  //if there is enough empty space in file, insert there instead of appending
  uint32_t contentLen = measureJson(content);
  uint32_t spaces = 0, spaceI = 0, spacesMax = 0, spaceMaxI = 0;
  f.seek(1, SeekSet);
  for (uint32_t i = 1; i < f.size(); i++)
  {
    if (f.read() == ' ') {
      if (!spaces) spaceI = i; spaces++;
    } else {
      if (spaces > spacesMax) { spacesMax = spaces; spaceMaxI = spaceI;}
      spaces = 0;
      if (spacesMax >= contentLen) {
        f.seek(spaceMaxI, SeekSet);
        serializeJson(content, f);
        return true;
      }
    }
  }
  
  //check if last character in file is '}' (typical)
  uint32_t lastByte = f.size() -1;
  f.seek(1, SeekEnd);
  if (f.read() == '}') pos = lastByte;
  
  if (pos == 0) //not found
  {
    while (find("}",f)) //find last closing bracket in JSON if not last char
    {
      pos = f.position();
    }
  }
  
  if (pos)
  {
    f.seek(pos -1, SeekSet);
    f.write(',');
  } else { //file content is not valid JSON object
    f.seek(0, SeekSet);
    f.write('{'); //start JSON
  }

  f.print("\"");
  f.print(key);
  f.print("\":");
  //Append object
  serializeJson(content, f);
  
  f.write('}');
  if (doClose) f.close();
}

bool readObjectFromFileUsingId(const char* file, uint16_t id, JsonDocument* dest)
{
  char objKey[10];
  sprintf(objKey, "\"%ld\":", id);
  readObjectFromFile(file, objKey, dest);
}

bool readObjectFromFile(const char* file, const char* key, JsonDocument* dest)
{
  //if (id == playlistId) return true;
  //playlist is already loaded, but we can't be sure that file hasn't changed since loading
  
  File f = SPIFFS.open(file, "r");
  if (!f) return false;
  f.setTimeout(0);
  Serial.println(key);
  if (f.find(key)) //key does not exist in file
  {
    f.close();
    return false;
  }

  deserializeJson(*dest, f);

  f.close();
  return true;
}
#endif

#if !defined WLED_DISABLE_FILESYSTEM && defined WLED_ENABLE_FS_SERVING
//Un-comment any file types you need
String getContentType(AsyncWebServerRequest* request, String filename){
  if(request->hasArg("download")) return "application/octet-stream";
  else if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
//  else if(filename.endsWith(".css")) return "text/css";
//  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".json")) return "application/json";
  else if(filename.endsWith(".png")) return "image/png";
//  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
//  else if(filename.endsWith(".xml")) return "text/xml";
//  else if(filename.endsWith(".pdf")) return "application/x-pdf";
//  else if(filename.endsWith(".zip")) return "application/x-zip";
//  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(AsyncWebServerRequest* request, String path){
  DEBUG_PRINTLN("FileRead: " + path);
  if(path.endsWith("/")) path += "index.htm";
  String contentType = getContentType(request, path);
  /*String pathWithGz = path + ".gz";
  if(SPIFFS.exists(pathWithGz)){
    request->send(SPIFFS, pathWithGz, contentType);
    return true;
  }*/
  if(SPIFFS.exists(path)) {
    request->send(SPIFFS, path, contentType);
    return true;
  }
  return false;
}

#else
bool handleFileRead(AsyncWebServerRequest*, String path){return false;}
#endif
