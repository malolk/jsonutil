#include "json_util.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#ifndef JSON_PARSER_ARG_CONTEXT_STACK_INIT_SIZE
  #define JSON_PARSER_ARG_CONTEXT_STACK_INIT_SIZE 256
#endif

namespace jsonutil {
static bool IsSpace(const char* ptr) {
  return (*ptr == ' ' || *ptr == '\t' || *ptr == '\r' || *ptr == '\n');
}

static void SkipSpace(ArgContext* arg_ptr) {
  const char* ptr = arg_ptr->json_text;
  while (IsSpace(ptr)) {
    ++ptr;
  }
  arg_ptr->json_text = ptr;
}

static bool CheckSingular(ArgContext* arg_ptr) {
  SkipSpace(arg_ptr);
  return *(arg_ptr->json_text) == '\0'; 
}

static Status ParseLiteral(ArgContext* arg_ptr, JsonValue* v, char c) {
  assert(c == 'n' || c == 'f' || c == 't');
  const char* ptr = arg_ptr->json_text;
  bool valide = false;
  JsonType type;

  if (c == 'n') {
    valide = ptr[0] == 'n' && ptr[1] == 'u' && ptr[2] == 'l' && ptr[3] == 'l';
    type = kJSON_TYPE_NULL;
    arg_ptr->json_text += 4;
  } else if (c == 'f') {
    valide = ptr[0] == 'f' && ptr[1] == 'a' && ptr[2] == 'l' && ptr[3] == 's' && ptr[4] == 'e';
    type = kJSON_TYPE_FALSE;
    arg_ptr->json_text += 5;
  } else {
    valide = ptr[0] == 't' && ptr[1] == 'r' && ptr[2] == 'u' && ptr[3] == 'e';
    type = kJSON_TYPE_TRUE;
    arg_ptr->json_text += 4;
  }

  if (valide) {
    v->type = type;
    return kJSON_OK;
  }
  return kJSON_PARSE_INVALID_VALUE;
}

static const char* IsInvalidNumber(ArgContext* arg_ptr) {
  typedef enum {
    kINVALID = 0,
    kMINUS,
    kZERO, 
    kONE_TO_NINE,
    kDOT,
    kEXPONENT,
    kPLUS
  } CharType;
  
  // construct the finite state machine according to the "Figure 4 - numbers" in 
  // http://www.ecma-international.org/publications/files/ECMA-ST/ECMA-404.pdf
  const char state_table[][7] = {
    // 'invalid'  '-'    '0'   '1-9'    '.'  'e or E'    '+'
           -1,     1,     2,     3,     -1,     -1,      -1, // initiate state 0
           -1,    -1,     2,     3,     -1,     -1,      -1, // state 1
           -1,    -1,    -1,    -1,      5,      7,      -1, // state 2
           -1,    -1,     4,     4,      5,      7,      -1, // state 3
           -1,    -1,     4,     4,      5,      7,      -1, // state 4
           -1,    -1,     6,     6,     -1,      7,      -1, // state 5
           -1,    -1,     6,     6,     -1,      7,      -1, // state 6
           -1,     8,     9,     9,     -1,     -1,       8, // state 7
           -1,    -1,     9,     9,     -1,     -1,      -1, // state 8
           -1,    -1,     9,     9,     -1,     -1,      -1  // state 9
  };
  
  const char* ptr = arg_ptr->json_text;
  int state = 0;
  while (*ptr != '\0' && *ptr != ',' && *ptr != ']' && !IsSpace(ptr)) {
    CharType c = kINVALID;
    if (*ptr == '0') {
      c = kZERO;
    } else if (*ptr >= '1' && *ptr <= '9') {
      c = kONE_TO_NINE;
    } else if (*ptr == '.') {
      c = kDOT;
    } else if (*ptr == 'e' || *ptr == 'E') {
      c = kEXPONENT;
    } else if (*ptr == '-') {
      c = kMINUS;
    } else if (*ptr == '+') {
      c = kPLUS;
    }
    state = state_table[state][c];
    if (state == -1) {
      return NULL;
    }
    ++ptr;
  }
  const char* ret = arg_ptr->json_text;
  if (state != 0 && state != 1) {
    arg_ptr->json_text = ptr;
    return ret;
  }
  return NULL;
}

static Status ParseNumber(ArgContext* arg_ptr, JsonValue* v) {
  const char* ptr = arg_ptr->json_text;
  if (!IsInvalidNumber(arg_ptr)) {
    return kJSON_PARSE_INVALID_VALUE;
  }

  Status ret = kJSON_OK;
  v->type = kJSON_TYPE_NUMBER;
  int save_errno = errno;
  errno = 0;
  double num = strtod(ptr, NULL);
  if (errno) {
    // ERANGE
    if (num == 0) {
      ret = kJSON_PARSE_NUMBER_UNDERFLOW;
    } else {
      ret = kJSON_PARSE_NUMBER_OVERFLOW;
    }
  } else {
    (v->data).num = num;
  }
  errno = save_errno;
  return ret;
}


char* ContextPop(ArgContext* arg_ptr, int size) {
  assert(arg_ptr->top >= size && size >= 0);
  arg_ptr->top -= size;
  return (arg_ptr->stack + arg_ptr->top);
}

char* ContextPush(ArgContext* arg_ptr, int size) {
  assert(size >= 0);
  int need = arg_ptr->top + size;
  if (need > arg_ptr->size) {
    if (arg_ptr->stack == NULL) {
      arg_ptr->size = JSON_PARSER_ARG_CONTEXT_STACK_INIT_SIZE;
    }
    while (need > arg_ptr->size) {
      arg_ptr->size += arg_ptr->size >> 1;
    }
    arg_ptr->stack = static_cast<char*>(realloc(arg_ptr->stack, arg_ptr->size));
  }
  char* ret = arg_ptr->stack + arg_ptr->top;
  arg_ptr->top += size;
  return ret;
}

void ContextPrepare(ArgContext* arg_ptr, int size) {
  if (size < arg_ptr->size - arg_ptr->top) return;
  ContextPush(arg_ptr, size);
  ContextPop(arg_ptr, size);
}

void ContextPushUint32(ArgContext* arg_ptr, uint32_t* ptr, int size) {
  assert(ptr != NULL && size >= 0 && size <= 4);
  char* dst = ContextPush(arg_ptr, size);
  while (--size >= 0) {
    *dst++ = ((*ptr) >> (8 * size)) & 0xFF;  // big endian
  }
}

static Status TranslateHex(const char** p, uint16_t& buf) {
  buf = 0;
  for (int i = 0; i < 4; ++i) {
    char chr = **p;
    if (chr >= '0' && chr <= '9') {
      chr = chr - '0';
    } else if (chr >= 'a' && chr <= 'f') {
      chr = chr - 'a' + 10;
    } else if (chr >= 'A' && chr <= 'F') {
      chr = chr - 'A' + 10;
    } else {
      return kJSON_PARSE_STRING_UNICODE_INVALID_HEX;
    }
    buf <<= 4;
    buf |= 0x0F & chr;
    (*p)++;
  }  
  return kJSON_OK;
}

Status EncodeByUTF8(uint32_t& buf, char& num) {
  uint32_t ret = 0;
  if (buf <= 0x7F) {
    num = 1;
    ret = buf;
  } else if (buf <= 0x7FF) {
    num = 2;
    ret = (0xC0 | ((buf >> 6) & 0x1F)) << 8;
    ret |= 0x80 | ( buf       & 0x3F);
  } else if (buf <= 0xFFFF) {
    num = 3;
    ret =  (0xE0 | ((buf >> 12) & 0x0F)) << 16;
    ret |= (0x80 | ((buf >> 6 ) & 0x3F)) <<  8;
    ret |= (0x80 | ( buf        & 0x3F));
  } else if (buf <= 0x10FFFF) {
    num = 4;
    ret =  (0xF0 | ((buf >> 18) & 0x07)) << 24;
    ret |= (0x80 | ((buf >> 12) & 0x3F)) << 16;
    ret |= (0x80 | ((buf >>  6) & 0x3F)) <<  8;
    ret |= (0x80 | ( buf        & 0x3F));
  } else {
    return kJSON_PARSE_STRING_UNICODE_INVALID_RANGE;
  }
  buf = ret;
  return kJSON_OK;
}

static Status TranslateUnicodeHex(const char** p, uint32_t& buf, char& num) {
  uint16_t high = 0, low = 0;
  Status s = TranslateHex(p, high);
  if (s != kJSON_OK) return s;
  if (high >= 0xD800 && high <= 0xDBFF) {
    if (!((*p)[0] == '\\' && (*p)[1] == 'u')) {
      return kJSON_PARSE_STRING_UNICODE_INVALID_SURROGATE;      
    }
    *p += 2;
    s =  TranslateHex(p, low);
    if (s != kJSON_OK) return s;
    if (low < 0xDC00 || low > 0xDFFF) {
      return kJSON_PARSE_STRING_UNICODE_INVALID_SURROGATE;
    }
    buf = 0x10000 + (high - 0xD800) * 0x400 + (low - 0xDC00);
  } else if (high >= 0xDC00 && high <= 0xDFFF) {
    return kJSON_PARSE_STRING_UNICODE_INVALID_SURROGATE; 
  } else {
    buf = high;
  }
  return EncodeByUTF8(buf, num);
}

static Status TranslateEscapedChar(const char** p, uint32_t& buf, char& num) {
  char chr = **p;
  (*p)++;
  num = 1;
  Status s;
  switch (chr) {
    case '\\':  buf = '\\'; break;
    case '\"':  buf = '\"'; break;
    case '/':   buf = '/';  break;
    case 'b':   buf = '\b'; break;
    case 'f':   buf = '\f'; break;
    case 'n':   buf = '\n'; break;
    case 'r':   buf = '\r'; break;
    case 't':   buf = '\t'; break;
    case 'u':   return TranslateUnicodeHex(p, buf, num);
    default:    return kJSON_PARSE_STRING_ESCAPED_INVALID_CHAR;
  }
  return kJSON_OK;
}

static Status ParseStringInStack(ArgContext* arg_ptr, int& len) {
  const char* ptr = arg_ptr->json_text + 1; // +1 skip the leading mark '"'
  int head = arg_ptr->top;
  Status s;
  uint32_t buf;
  char num = 0;
  while (true) {
    buf = *ptr++;
    switch (buf) {
      case '\"': arg_ptr->json_text = ptr;
                 len = arg_ptr->top - head;
                 return kJSON_OK;
      case '\0': return kJSON_PARSE_STRING_NO_END_MARK;
      case '\\': buf = 0;
                 s = TranslateEscapedChar(&ptr, buf, num);
                 if (s != kJSON_OK) return s;
                 ContextPushUint32(arg_ptr, &buf, num);
                 break;
      default:   if ((buf & 0xFF) < 0x20) return kJSON_PARSE_STRING_INVALID_CHAR;
                 ContextPushUint32(arg_ptr, &buf, 1);
    }
  }
  return kJSON_OK; // never get here.
}

static Status ParseString(ArgContext* arg_ptr, JsonValue* v) {
  int len = 0;
  Status s = ParseStringInStack(arg_ptr, len);
  if (s == kJSON_OK) {
    SetString(v, ContextPop(arg_ptr, len), len);
  }
  return s;
}

static Status ParseValue(ArgContext* arg_ptr, JsonValue* v);
static Status ParseArray(ArgContext* arg_ptr, JsonValue* v) {
  v->type = kJSON_TYPE_ARRAY;
  SkipSpace(arg_ptr);
  const char* ptr = (arg_ptr->json_text += 1);
  if (*ptr == ']') {
    (*v).data.array.a = NULL;
    (*v).data.array.size = 0;
    arg_ptr->json_text++;
    return kJSON_OK;
  }
  int num = 0;
  while (true) {
    JsonValue val;
    InitValue(&val);
    Status s = ParseValue(arg_ptr, &val);
    if (s != kJSON_OK) return s;
    memcpy(ContextPush(arg_ptr, sizeof(val)), &val, sizeof(val));
    (*v).data.array.size = ++num;
    SkipSpace(arg_ptr);
    ptr = arg_ptr->json_text;
    if (*ptr == ']') {
      int bytes = num * sizeof(val);
      char* dst = static_cast<char*>(malloc(bytes));
      if (dst == NULL) {
        return kJSON_OUT_OF_MEMORY;
      }
      memcpy(dst, ContextPop(arg_ptr, bytes), bytes);
      (*v).data.array.a = reinterpret_cast<JsonValue*>(dst);
      arg_ptr->json_text++; // skip the ending bracket square
      break;
    } else if (*ptr == ',') {
      arg_ptr->json_text++;
      SkipSpace(arg_ptr);
      if (*arg_ptr->json_text == ']') {
        return kJSON_PARSE_ARRAY_INVALID_EXTRA_COMMA;
      }
    } else {
      return kJSON_PARSE_ARRAY_MISSING_COMMA;
    }
  }
  
  return kJSON_OK;
}

static void FreeObjectMember(ObjectMember* p) {
  if (p->k && p->klen > 0) {
    free(p->k);
    p->k = NULL;
    p->klen = 0;
  }
  if (p->v) {
    SetNull(p->v);
    free(p->v);
    p->v = NULL;
  }
}

/* Note: Object member is sorted by key. */
static ObjectMember* FindObjectMemberByKey(ObjectMember* p, int size, const char* k, int klen) {
  int left = 0, right = size;
  while (right - left > 1) {
    int mid = left + (right - left) / 2;
    int len = p[mid].klen;
    if (CompareBytes(p[mid].k, len, k, klen) <= 0) {
      left = mid;
    } else {
      right = mid;
    }
  }
  /* return value should be less-or-equal compared to the target. */
  return p + left;
}

static void PushObjectMemberOntoStack(ArgContext* arg_ptr, const char* k, 
                                      int klen, JsonValue* v, int num) {
  /* push ObjectMember onto stack by key-order. */
  ObjectMember* pos = NULL;
  ObjectMember* cur = reinterpret_cast<ObjectMember*>(ContextPush(arg_ptr, sizeof(ObjectMember)));
  memset(cur, 0, sizeof(*cur));
  if (num == 0) { 
    pos = cur;
  } else {
    if (CompareBytes((cur - 1)->k, (cur - 1)->klen, k, klen) <= 0) {
      pos = cur;
    } else {
      ObjectMember* start = cur - num;
      pos = FindObjectMemberByKey(start, num, k, klen);
      if (CompareBytes(pos->k, pos->klen, k, klen) <= 0) {
        pos += 1;
      }
      memmove(pos + 1, pos, (cur - pos) * sizeof(ObjectMember));
    }
  }
  memset(pos, 0, sizeof(*pos));
  SetObjectMember(pos, k, klen, v);
}

static Status ParseObject(ArgContext* arg_ptr, JsonValue* v) {
  v->type = kJSON_TYPE_OBJECT;
  const char* ptr = (arg_ptr->json_text += 1);
  SkipSpace(arg_ptr);
  if (*ptr == '}') {
    (*v).data.obj.m = NULL;
    (*v).data.obj.size = 0;
    arg_ptr->json_text++;
    return kJSON_OK;
  }

  Status s;
  int len;
  int num = 0;
  while (true) {
    SkipSpace(arg_ptr);

    /* parse the key part */
    ptr = arg_ptr->json_text;
    if (*ptr != '\"') return kJSON_PARSE_OBJECT_MISSING_KEY;
    len = 0;
    s = ParseStringInStack(arg_ptr, len);
    if (s != kJSON_OK) return s;
    char* kptr = static_cast<char*>(malloc(len));
    if (kptr  == NULL) return kJSON_OUT_OF_MEMORY;
    memcpy(kptr, ContextPop(arg_ptr, len), len);
    
    SkipSpace(arg_ptr);
    if (*arg_ptr->json_text != ':') {
      free(kptr);
      return kJSON_PARSE_OBJECT_MISSING_COLON;
    }
    arg_ptr->json_text++; // skip colon

    /* parse the value part */
    JsonValue val;
    InitValue(&val);
    SkipSpace(arg_ptr);
    if ((s = ParseValue(arg_ptr, &val)) != kJSON_OK) {
      free(kptr);
      return s;
    }

    PushObjectMemberOntoStack(arg_ptr, kptr, len, &val, num);
    free(kptr);
    SetNull(&val);
    (*v).data.obj.size = ++num;
    SkipSpace(arg_ptr);
    if (*arg_ptr->json_text == '}') {
      int bytes = num * sizeof(ObjectMember);
      char* dst = static_cast<char*>(malloc(bytes));
      if (dst == NULL) return kJSON_OUT_OF_MEMORY;
      memcpy(dst, ContextPop(arg_ptr, bytes), bytes);
      (*v).data.obj.m = reinterpret_cast<ObjectMember*>(dst);
      arg_ptr->json_text++;
      break;
    } else if (*arg_ptr->json_text == ',') {
      arg_ptr->json_text++;
      SkipSpace(arg_ptr);
      if(*arg_ptr->json_text == '}') {
        return kJSON_PARSE_OBJECT_INVALID_EXTRA_COMMA;
      }
    } else {
      return kJSON_PARSE_OBJECT_MISSING_COMMA_OR_CURLY_BRACKET;
    }
  }
  return kJSON_OK;
}

static Status ParseValue(ArgContext* arg_ptr, JsonValue* v) {
  Status ret;
  switch (*arg_ptr->json_text) {
    case 'n':  ret = ParseLiteral(arg_ptr, v, 'n'); break;
    case 'f':  ret = ParseLiteral(arg_ptr, v, 'f'); break;
    case 't':  ret = ParseLiteral(arg_ptr, v, 't'); break;
    case '[':  ret = ParseArray(arg_ptr, v);        break;
    case '{':  ret = ParseObject(arg_ptr, v);       break;
    case '\"': ret = ParseString(arg_ptr, v);       break;
    case '\0': ret = kJSON_PARSE_EXPECT_VALUE;      break;
    default:   ret = ParseNumber(arg_ptr, v);       break;
  }
  return ret;
}

static void FreeValue(JsonValue* v) {
  if (v->type == kJSON_TYPE_STRING && (v->data).str.s) {
    free((v->data).str.s);
    (v->data).str.s = NULL;
    (v->data).str.len = 0;
  } else if (v->type == kJSON_TYPE_ARRAY && (v->data).array.a) {
    int mem_size = (v->data).array.size;
    assert(mem_size > 0);
    JsonValue* p = (v->data).array.a;
    for (int i = 0; i < mem_size; ++i) {
      FreeValue(&p[i]);
    }
    free(p);
    (v->data).array.a = NULL;
    (v->data).array.size = 0;
  } else if (v->type == kJSON_TYPE_OBJECT && (v->data).obj.m) {
    int mem_size = (v->data).obj.size;
    assert(mem_size > 0);
    ObjectMember* p = (v->data).obj.m;
    for (int i = 0; i < mem_size; ++i) {
      FreeObjectMember(&p[i]);
    }
    free(p);
    (v->data).obj.m = NULL;
    (v->data).obj.size = 0;
  }
}

void FreeContext(ArgContext* arg_ptr) {
  if (arg_ptr->stack) {
    arg_ptr->top = arg_ptr->size = 0;
    free(arg_ptr->stack);
    arg_ptr->stack = NULL;
  }
}

static void FreeContextOnError(ArgContext* arg_ptr, JsonValue* v) {
  if (arg_ptr->stack) {
    int num = 0;
    if (v->type == kJSON_TYPE_ARRAY) {
      JsonValue* ptr = reinterpret_cast<JsonValue*>(arg_ptr->stack);
      num = (*v).data.array.size;
      for (int i = 0; i < num; ++i) {
        FreeValue(ptr + i);
      }
    } else if (v->type == kJSON_TYPE_OBJECT) {
      ObjectMember* ptr = reinterpret_cast<ObjectMember*>(arg_ptr->stack);
      num = (*v).data.obj.size;
      for (int i = 0; i < num; ++i) {
        FreeObjectMember(ptr + i);
      }
    }
    FreeContext(arg_ptr);
  }
}

Status ParseJson(JsonValue* v, const char* json_text) {
  assert(v != NULL && json_text != NULL);
  ArgContext arg;
  arg.json_text = json_text;
  arg.stack = NULL;
  arg.top = arg.size = 0;
  SkipSpace(&arg);
  InitValue(v);
  Status ret = ParseValue(&arg, v);
  if (ret == kJSON_OK) {
    if (!CheckSingular(&arg)) {
      ret = kJSON_PARSE_ROOT_NOT_SINGULAR;
    }
  }
  if (ret != kJSON_OK) {
    FreeContextOnError(&arg, v);
  } else {
    FreeContext(&arg);
  }
  return ret;
}

double GetNumber(const JsonValue* v) {
  assert(v != NULL && v->type == kJSON_TYPE_NUMBER);
  return (v->data).num;
}

void SetNumber(JsonValue* v, double num) {
  assert(v != NULL);
  FreeValue(v);    
  v->type = kJSON_TYPE_NUMBER;
  (v->data).num = num;
}

bool GetBoolean(const JsonValue* v) {
  assert(v != NULL && (v->type == kJSON_TYPE_FALSE || v->type == kJSON_TYPE_TRUE));
  return (v->type == kJSON_TYPE_FALSE) ? false : true;
}

void SetBoolean(JsonValue* v, bool b) {
  assert(v != NULL);
  FreeValue(v);    
  if (b) {
    v->type = kJSON_TYPE_TRUE;
  } else {
    v->type = kJSON_TYPE_FALSE;
  }
}

void SetNull(JsonValue* v) {
  assert(v != NULL);
  FreeValue(v);
  InitValue(v);
}

void InitValue(JsonValue* v) {
  memset(static_cast<void*>(v), 0, sizeof(*v));
  v->type = kJSON_TYPE_NULL;
}

JsonType GetType(const JsonValue* v) {
  assert(v != NULL);
  return v->type;
}

void SetString(JsonValue* v, const char* str, int len) {
  assert(v != NULL && (str != NULL && len > 0 || str == NULL && len == 0));
  FreeValue(v);
  v->type = kJSON_TYPE_STRING;
  (v->data).str.len = len;
  char* ptr = (v->data).str.s = static_cast<char*>(malloc(len + 1));
  assert(ptr != NULL);
  memcpy(ptr, str, len);
  ptr[len] = '\0';
}

const char* GetString(const JsonValue* v) {
  assert(v != NULL && v->type == kJSON_TYPE_STRING);
  return (v->data).str.s;
}

int GetStringLength(const JsonValue* v) {
  assert(v != NULL && v->type == kJSON_TYPE_STRING);
  assert((v->data).str.len >= 0);
  return (v->data).str.len; 
}

int CompareBytes(const char* lhs, int llen, const char* rhs, int rlen) {
  int min_len = llen < rlen ? llen : rlen;
  for (int i = 0; i < min_len; ++i) {
    if (lhs[i] != rhs[i]) {
      return (lhs[i] < rhs[i]) ? -1 : 1;
    }
  }
  if (llen == rlen) {
    return 0;
  } else {
    return llen < rlen ? -1 : 1;
  }
}

static bool JsonCompareString(const JsonValue* lhs, const JsonValue* rhs) {
  assert(lhs && rhs && lhs->type == kJSON_TYPE_STRING 
         && rhs->type == kJSON_TYPE_STRING);
  int len = (*lhs).data.str.len;
  if (len != (*rhs).data.str.len) return false;
  return CompareBytes((*lhs).data.str.s, len, (*rhs).data.str.s, len) == 0;
}

static bool JsonCompareArray(const JsonValue* lhs, const JsonValue* rhs) {
  assert(lhs && rhs && lhs->type == kJSON_TYPE_ARRAY 
         && rhs->type == kJSON_TYPE_ARRAY);
  int num = (*lhs).data.array.size;
  if (num != (*rhs).data.array.size) return false;
  if (num == 0) return true;
  const JsonValue* lhs_p = (*lhs).data.array.a;
  const JsonValue* rhs_p = (*rhs).data.array.a;
  for (int i = 0; i < num; ++i) {
    if (!JsonCompare(lhs_p + i, rhs_p + i)) return false;
  }
  return true;
}

static bool JsonCompareObject(const JsonValue* lhs, const JsonValue* rhs) {
  assert(lhs && rhs && lhs->type == kJSON_TYPE_OBJECT 
         && rhs->type == kJSON_TYPE_OBJECT);
  int size = (*lhs).data.obj.size;
  if (size != (*rhs).data.obj.size) return false;
  if (size == 0) return true;
  const ObjectMember* lp = (*lhs).data.obj.m;
  const ObjectMember* rp = (*rhs).data.obj.m;
  for (int i = 0; i < size; ++i) {
    if (lp[i].klen != rp[i].klen 
        || CompareBytes(lp[i].k, lp[i].klen, rp[i].k, rp[i].klen) != 0
        || !JsonCompare(lp[i].v, rp[i].v)) {
      return false;
    }
  }
  return true;
}

bool JsonCompare(const JsonValue* lhs, const JsonValue* rhs) {
  assert(lhs && rhs);
  JsonType l = lhs->type, r = rhs->type;
  if (lhs->type != rhs->type) return false;
  switch (lhs->type) {
    case kJSON_TYPE_NULL:   // fall through
    case kJSON_TYPE_FALSE:  // fall through
    case kJSON_TYPE_TRUE:   return (lhs->type == rhs->type);
    case kJSON_TYPE_NUMBER: return ((*lhs).data.num == (*rhs).data.num);
    case kJSON_TYPE_STRING: return JsonCompareString(lhs, rhs);
    case kJSON_TYPE_ARRAY:  return JsonCompareArray(lhs, rhs);
    case kJSON_TYPE_OBJECT: return JsonCompareObject(lhs, rhs);
    default:                return false; // won't be here.
  }
}

int GetArraySize(const JsonValue* v) {
  assert(v && v->type == kJSON_TYPE_ARRAY);
  return (*v).data.array.size;
}

JsonValue* GetArrayElement(const JsonValue* v, int index) {
  assert(v && v->type == kJSON_TYPE_ARRAY);
  assert(index >= 0 && index < (*v).data.array.size);
  assert((*v).data.array.a);
  return ((*v).data.array.a + index);
}

void SetArrayElement(JsonValue* arr, int index, JsonValue* value) {
  assert(arr && arr->type == kJSON_TYPE_ARRAY && value);
  int size = (*arr).data.array.size;
  assert(index >= 0 && index < size);
  JsonValue* v = (*arr).data.array.a + index;
  CopyJsonValue(v, value);
}

/* Note: @parameter arg_ptr is exclusive to the json value. */
void ContextPushValue(ArgContext* arg_ptr, JsonValue* value) {
  assert(arg_ptr && value);
  assert(arg_ptr->top % sizeof(*value) == 0);
  JsonValue* dst = reinterpret_cast<JsonValue*>(ContextPush(arg_ptr, sizeof(*value)));
  CopyJsonValue(dst, value);
}

/* Note: @parameter arg_ptr is exclusive to the json value. */
void PushArrayValue(ArgContext* arg_ptr, JsonValue* arr) {
  assert(arr && arr->type == kJSON_TYPE_ARRAY && arg_ptr);
  assert(arg_ptr->top % sizeof(*arr) == 0);
  int size = (*arr).data.array.size;
  int num = arg_ptr->top / sizeof(*arr);
  if (size == 0) {
    arg_ptr->stack = static_cast<char*>(realloc(arg_ptr->stack, arg_ptr->top));
    (*arr).data.array.a = reinterpret_cast<JsonValue*>(arg_ptr->stack);
    (*arr).data.array.size = num;
    arg_ptr->stack = NULL;
    arg_ptr->top = arg_ptr->size = 0;
  } else {
    size += num;
    JsonValue* a = (*arr).data.array.a;
    a = reinterpret_cast<JsonValue*>(realloc(a, size * sizeof(*arr)));
    memcpy(a + size - num, arg_ptr->stack, arg_ptr->top);
    (*arr).data.array.a = a;
    (*arr).data.array.size = size;
  }
}

int GetObjectSize(const JsonValue* v) {
  assert(v && v->type == kJSON_TYPE_OBJECT);
  return (*v).data.obj.size;
}

ObjectMember* GetObjectMember(const JsonValue* v, int index) {
  assert(v && v->type == kJSON_TYPE_OBJECT);
  assert(index >= 0 && index < (*v).data.obj.size);
  assert((*v).data.obj.m);
  return ((*v).data.obj.m + index);
}

char* GetObjectMemberKey(const ObjectMember* o, int& size) {
  assert(o != NULL);
  size = o->klen;
  return o->k;
}

JsonValue* GetObjectMemberValue(const ObjectMember* o) {
  assert(o != NULL);
  return o->v;
}

static ObjectMember* GetMemberByKey(const JsonValue* o, const char* k, int klen) {
  assert(o != NULL);
  int size = (*o).data.obj.size;
  ObjectMember* p = FindObjectMemberByKey((*o).data.obj.m, size, k, klen);
  return CompareBytes(k, klen, p->k, p->klen) == 0 ? p : NULL;
}

JsonValue* GetValueByKey(const JsonValue* o, const char* k, int klen) {
  assert(o != NULL && o->type == kJSON_TYPE_OBJECT);
  ObjectMember* p = GetMemberByKey(o, k, klen);
  return p ? p->v : NULL;
}

/* Note: @parameter arg_ptr is exclusive to the object. */
void ContextPushMember(ArgContext* arg_ptr, const char* k, 
                      int klen, JsonValue* v) {
  assert(arg_ptr && k && v);
  assert(arg_ptr->top % sizeof(ObjectMember) == 0);
  int num = arg_ptr->top / sizeof(ObjectMember);
  PushObjectMemberOntoStack(arg_ptr, k, klen, v, num); 
}

static void MergeMemberList(ObjectMember* dst, 
                            const ObjectMember* lhs, int llen,
                            const ObjectMember* rhs, int rlen) {
  int i = 0, l = 0, r = 0;
  for (; l < llen && r < rlen; ++i) {
    const ObjectMember* lp = lhs + l;
    const ObjectMember* rp = rhs + r;
    if (CompareBytes(lp->k, lp->klen, rp->k, rp->klen) <= 0) {
      memcpy(dst + i, lhs + l++, sizeof(*dst));
    } else {
      memcpy(dst + i, rhs + r++, sizeof(*dst));
    }
  }
  while (l < llen) memcpy(dst + i++, lhs + l++, sizeof(*dst));
  while (r < rlen) memcpy(dst + i++, rhs + r++, sizeof(*dst));
}

void PushObjectMember(ArgContext* arg_ptr, JsonValue* o) {
  assert(arg_ptr && o && o->type == kJSON_TYPE_OBJECT);
  assert(arg_ptr->top % sizeof(ObjectMember) == 0);
  int num = arg_ptr->top / sizeof(ObjectMember);
  if (num == 0) return;
  int size = (*o).data.obj.size;
  if (size == 0) {
    (*o).data.obj.m = reinterpret_cast<ObjectMember*>(realloc(arg_ptr->stack, arg_ptr->top));
    (*o).data.obj.size = num;
    arg_ptr->stack = NULL;
    arg_ptr->top = arg_ptr->size = 0;
  } else {
    ObjectMember* lhs = (*o).data.obj.m;
    int bytes = num * sizeof(*lhs);
    ObjectMember* rhs = reinterpret_cast<ObjectMember*>(ContextPop(arg_ptr, bytes));
    ObjectMember* dst = reinterpret_cast<ObjectMember*>(malloc((size + num) * sizeof(*lhs)));
    MergeMemberList(dst, lhs, size, rhs, num);
    free((*o).data.obj.m);
    (*o).data.obj.m = dst;
    (*o).data.obj.size = size + num;
  }
}

bool SetObjectKeyValue(JsonValue* o, const char* k, int klen, JsonValue* value) {
  assert(o && o->type == kJSON_TYPE_OBJECT && k && value);
  ObjectMember* m = GetMemberByKey(o, k, klen);
  if (m) {
    if (m->v != value) {
      SetNull(m->v);
      CopyJsonValue(m->v, value);
    }
    return true;
  }
  return false;
}

static void SetArray(JsonValue* dst, const JsonValue* src) {
  assert(dst && src && dst->type == kJSON_TYPE_ARRAY && src->type == kJSON_TYPE_ARRAY);
  FreeValue(dst);
  int size = (*dst).data.array.size = (*src).data.array.size;
  JsonValue* a = reinterpret_cast<JsonValue*>(malloc(size * sizeof(*dst)));
  memset(a, 0, size * sizeof(*dst));
  for (int i = 0; i < size; ++i) {
    CopyJsonValue(a + i, GetArrayElement(src, i));
  }
  (*dst).data.array.a = a;
}

void SetObjectMember(ObjectMember* m, const char* k, int klen, const JsonValue* v) {
  assert(m && k && v);
  if (m->k != k) {
    free(m->k);
    m->k = static_cast<char*>(malloc(klen));
    memcpy(m->k, k, klen);
    m->klen = klen;
  }
  if (m->v != v) {
    if(m->v) {
      SetNull(m->v); 
    } else {
      m->v = reinterpret_cast<JsonValue*>(malloc(sizeof(*v)));
      memset(m->v, 0, sizeof(JsonValue));
    }
    CopyJsonValue(m->v, v);
  }
}

static void SetObject(JsonValue* dst, const JsonValue* src) {
  assert(dst && src && dst->type == kJSON_TYPE_OBJECT && src->type == kJSON_TYPE_OBJECT);
  FreeValue(dst);
  int size = (*dst).data.obj.size = (*src).data.obj.size;
  ObjectMember* m = reinterpret_cast<ObjectMember*>(malloc(size * sizeof(ObjectMember)));
  memset(m, 0, size * sizeof(ObjectMember));
  for (int i = 0; i < size; ++i) {
    ObjectMember* p = GetObjectMember(src, i);
    SetObjectMember(m + i, p->k, p->klen, p->v);        
  }
  (*dst).data.obj.m = m;
}

void CopyJsonValue(JsonValue* dst, const JsonValue* src) {
  assert(dst && src);
  SetNull(dst);
  dst->type = src->type;
  switch(src->type) {
    case kJSON_TYPE_NULL:
    case kJSON_TYPE_FALSE:
    case kJSON_TYPE_TRUE:   break;
    case kJSON_TYPE_NUMBER: SetNumber(dst, (*src).data.num);
                            break;
    case kJSON_TYPE_STRING: SetString(dst, GetString(src), GetStringLength(src));
                            break;
    case kJSON_TYPE_ARRAY:  SetArray(dst, src);
                            break;
    case kJSON_TYPE_OBJECT: SetObject(dst, src);
                            break;
    default:                assert(false);
  }
}

} // namespace jsonutil
