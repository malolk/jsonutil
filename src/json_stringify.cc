#include "json_util.h"

#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#ifndef JSON_STRINGIFY_INIT_BUF_SIZE
  #define JSON_STRINGIFY_INIT_BUF_SIZE 256
#endif

namespace jsonutil {

static void LiteralToString(ArgContext* arg_ptr, const char* s, int len) {
  memcpy(ContextPush(arg_ptr, len), s, len);
}

static void NumberToString(ArgContext* arg_ptr, const JsonValue* v) {
  char buf[32];
  int len = sprintf(buf, "%.17g", (*v).data.num);
  memcpy(ContextPush(arg_ptr, len), buf, len);
}

static uint32_t DecodeUTF8(const char* p, int* bytes) {
  uint32_t codepoint = 0;
  if (*p & 0xF8 == 0xF0) {
    assert(p[1] & 0xC0 == 0x80 && p[2] & 0xC0 == 0x80);
    assert(p[3] & 0xC0 == 0x80);
    *bytes = 4;
    codepoint =  p[3] & 0x3F;
    codepoint |= (p[2] & 0x3F) <<  6;
    codepoint |= (p[1] & 0x3F) << 12;
    codepoint |= (p[0] & 0x07) << 18;
  } else if (*p & 0xF0 == 0xE0) {
    assert(p[1] & 0xC0 == 0x80 && p[2] & 0xC0 == 0x80); 
    *bytes = 3;
    codepoint =  p[2] & 0x3F;
    codepoint |= (p[1] & 0x3F) <<  6;
    codepoint |= (p[0] & 0x0F) << 12;             
  } else if (*p & 0xE0 == 0xC0) {
    assert(p[1] & 0xC0 == 0x80); 
    *bytes = 2;
    codepoint =  p[1] & 0x3F;
    codepoint |= (p[0] & 0x1F) <<  6;    
  }
  return codepoint;
}

static void ContextPushHex(ArgContext* arg_ptr, uint16_t hex) {
  char buf[] = "\\u0000";
  sprintf(buf, "\\u%04X", hex);
  memcpy(ContextPush(arg_ptr, 6), buf, 6);
}

#define ContextPushString(arg_ptr, s, len) \
  memcpy(ContextPush(arg_ptr, len), s, len)

static void StringToString(ArgContext* arg_ptr, const JsonValue* v) {
  ContextPushString(arg_ptr, "\"", 1);
  int len = (*v).data.str.len;
  char* p = (*v).data.str.s;
  ContextPrepare(arg_ptr, len);
  for (int i = 0; i < len;) {
    int step = 1;
    switch (*p) {
      case '\\': ContextPushString(arg_ptr, "\\\\", 2); break;
      case '\"': ContextPushString(arg_ptr, "\\\"", 2); break;
      case '\b': ContextPushString(arg_ptr, "\\b", 2);  break;
      case '\f': ContextPushString(arg_ptr, "\\f", 2);  break;
      case '\n': ContextPushString(arg_ptr, "\\n", 2);  break;
      case '\r': ContextPushString(arg_ptr, "\\r", 2);  break;
      case '\t': ContextPushString(arg_ptr, "\\t", 2);  break;
      case '/':  ContextPushString(arg_ptr, "\\/", 2);  break;
      default:   if (*p < 0x20) {
                   ContextPushHex(arg_ptr, *p);
                 } else if ((*p & 0x80) == 0x00) {
                   ContextPushString(arg_ptr, p, 1);
                 } else {
                   int bytes = 0;
                   uint32_t codepoint = DecodeUTF8(p, &bytes);
                   step = bytes;
                   if (codepoint < 0x100000) {
                     ContextPushHex(arg_ptr, codepoint & 0xFFFF);
                   } else {
                     // extract surrogate pair;
                     uint16_t high = ((codepoint - 0x10000) >> 10);
                     uint16_t low =  (codepoint - 0x10000 - (high << 10)) + 0xDC00;
                     high += 0xD800;
                     ContextPushHex(arg_ptr, high);
                     ContextPushHex(arg_ptr, low);
                   }
                 }
    }
    p += step;
    i += step;
  }
  ContextPushString(arg_ptr, "\"", 1);
}

static void ValueToString(const JsonValue* v, ArgContext* arg_ptr);
static void ArrayToString(ArgContext* arg_ptr, const JsonValue* v) {
  ContextPushString(arg_ptr, "[", 1);  
  int size = (*v).data.array.size;
  const JsonValue* p = (*v).data.array.a;
  for (int i = 0; i < size; ++i) {
    ValueToString(p + i, arg_ptr);
    ContextPushString(arg_ptr, ",", 1);
  }
  if (size) {
    ContextPop(arg_ptr, 1); // pop the extra comma
  }
  ContextPushString(arg_ptr, "]", 1);  
}

static void ObjectToString(ArgContext* arg_ptr, const JsonValue* v) {
  ContextPushString(arg_ptr, "{", 1);  
  int size = (*v).data.obj.size;
  const ObjectMember* p = (*v).data.obj.m;
  for (int i = 0; i < size; ++i) {
    ContextPushString(arg_ptr, "\"", 1);
    ContextPushString(arg_ptr, (p + i)->k, (p + i)->klen);
    ContextPushString(arg_ptr, "\"", 1);
    ContextPushString(arg_ptr, ":", 1);
    ValueToString((p + i)->v, arg_ptr);
    ContextPushString(arg_ptr, ",", 1);
  }
  if (size) {
    ContextPop(arg_ptr, 1); // pop the extra comma
  }
  ContextPushString(arg_ptr, "}", 1);  
}

static void ValueToString(const JsonValue* v, ArgContext* arg_ptr) {
  switch (v->type) {
    case kJSON_TYPE_NULL:   LiteralToString(arg_ptr, "null", 4);  break;
    case kJSON_TYPE_FALSE:  LiteralToString(arg_ptr, "false", 5); break;
    case kJSON_TYPE_TRUE:   LiteralToString(arg_ptr, "true", 4);  break;
    case kJSON_TYPE_NUMBER: NumberToString(arg_ptr, v);           break; 
    case kJSON_TYPE_STRING: StringToString(arg_ptr, v);           break;
    case kJSON_TYPE_ARRAY:  ArrayToString(arg_ptr, v);            break;
    case kJSON_TYPE_OBJECT: ObjectToString(arg_ptr, v);           break;
  }
}

char* JsonToString(const JsonValue* v, int* len) {
  assert(v != NULL);
  ArgContext arg;
  arg.stack = reinterpret_cast<char*>(malloc(JSON_STRINGIFY_INIT_BUF_SIZE));
  if (arg.stack == NULL) {
    fprintf(stderr, "%s\n", "JsonToString error: out of memory!");
    return NULL;
  }
  arg.size = JSON_STRINGIFY_INIT_BUF_SIZE;
  arg.top = 0;
  ValueToString(v, &arg);
  int length = arg.top;
  void* ret = malloc(length + 1);
  if (ret == NULL) {
    fprintf(stderr, "%s\n", "JsonToString error: out of memory!");
    return NULL;
  }
  memcpy(ret, ContextPop(&arg, length), length);
  *(static_cast<char*>(ret) + length) = '\0'; // add null-terminator
  if (len) *len = length;
  FreeContext(&arg);
  return static_cast<char*>(ret);
}
} // namespace jsonutil
