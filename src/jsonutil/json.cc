#include "json.h"

#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

namespace jsonutil {
namespace {

/*=============================Parser Static functions=====================*/

bool IsSpace(const char* p) {
  return (*p == ' ' || *p == '\t' 
          || *p == '\r' || *p == '\n' || *p == '\0');
}

void SkipSpace(Slice& s) {
  while (IsSpace(s.Ptr()) && s.Len() > 0) {
    s.Move(1);
  }
}

bool CheckSingular(Slice& s) {
  SkipSpace(s);
  return s.Len() == 0; 
}

const char* IsInvalidNumber(Slice& s) {
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
  
  int state = 0;
  const char* ret = s.Ptr();
  const char* p;
  while (s.Len() > 0 && *s.Ptr() != ',' 
         && *s.Ptr() != ']' && !IsSpace(s.Ptr())) {
    CharType c = kINVALID;
    p = s.Ptr();
    if (*p == '0') {
      c = kZERO;
    } else if (*p >= '1' && *p <= '9') {
      c = kONE_TO_NINE;
    } else if (*p == '.') {
      c = kDOT;
    } else if (*p == 'e' || *p == 'E') {
      c = kEXPONENT;
    } else if (*p == '-') {
      c = kMINUS;
    } else if (*p == '+') {
      c = kPLUS;
    }
    state = state_table[state][c];
    if (state == -1) {
      return NULL;
    }
    s.Move(1);
  }
  if (state != 0 && state != 1) {
    return ret;
  }
  return NULL;
}

Status TranslateHex(Slice& s, uint16_t& buf) {
#pragma GCC diagnostic ignored "-Wconversion"
  if (s.Len() < 4) return kJSON_PARSE_STRING_UNICODE_INVALID_HEX;
  buf = 0;
  for (int i = 0; i < 4; ++i) {
    char chr = *s.Ptr();
    if (chr >= '0' && chr <= '9') {
      chr = (chr - '0');
    } else if (chr >= 'a' && chr <= 'f') {
      chr = (chr - 'a' + 10);
    } else if (chr >= 'A' && chr <= 'F') {
      chr = (chr - 'A' + 10);
    } else {
      return kJSON_PARSE_STRING_UNICODE_INVALID_HEX;
    }
    buf <<= 4;
    buf |= 0x0F & chr;
    s.Move(1);
  }  
#pragma GCC diagnostic error "-Wconversion"
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

Status TranslateUnicodeHex(Slice& s, uint32_t& buf, char& num) {
  if (s.Len() == 0) return kJSON_PARSE_STRING_ESCAPED_INVALID_CHAR;
  uint16_t high = 0, low = 0;
  Status ret = TranslateHex(s, high);
  if (ret != kJSON_OK) return ret;
  if (high >= 0xD800 && high <= 0xDBFF) {
    if (s.Len() < 6 || !(s.Ptr()[0] == '\\' && s.Ptr()[1] == 'u')) {
      return kJSON_PARSE_STRING_UNICODE_INVALID_SURROGATE;      
    }
    s.Move(2);
    ret =  TranslateHex(s, low);
    if (ret != kJSON_OK) return ret;
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

Status TranslateEscapedChar(Slice& s, uint32_t& buf, char& num) {
  if (s.Len() == 0) return kJSON_PARSE_STRING_ESCAPED_INVALID_CHAR;
  char chr = *s.Ptr();
  s.Move(1);
  num = 1;
  switch (chr) {
    case '\\':  buf = '\\'; break;
    case '\"':  buf = '\"'; break;
    case '/':   buf = '/';  break;
    case 'b':   buf = '\b'; break;
    case 'f':   buf = '\f'; break;
    case 'n':   buf = '\n'; break;
    case 'r':   buf = '\r'; break;
    case 't':   buf = '\t'; break;
    case 'u':   return TranslateUnicodeHex(s, buf, num);
    default:    return kJSON_PARSE_STRING_ESCAPED_INVALID_CHAR;
  }
  return kJSON_OK;
}

Status ParseStringInStack(Stack& stk, Slice& s, int& len) {
  s.Move(1); // +1 skip the leading mark '"' 
  int head = stk.Top();
  Status ret;
  uint32_t buf;
  char num = 0;
  while (true) {
    if (s.Len() == 0) return kJSON_PARSE_STRING_NO_END_MARK;
    buf = *s.Ptr();
    s.Move(1);
    switch (buf) {
      case '\"': len = stk.Top() - head;
                 return kJSON_OK;
      case '\\': buf = 0;
                 ret = TranslateEscapedChar(s, buf, num);
                 if (ret != kJSON_OK) return ret;
                 stk.PushUint32(buf, num);
                 break;
      default:   if ((buf & 0xFF) < 0x20) {
                   return kJSON_PARSE_STRING_INVALID_CHAR;
                 }
                 stk.PushUint32(buf, 1);
    }
  }
  return kJSON_OK; // never get here.
}

/* Note: Object member is sorted by key. */
Member* FindMemberByKey(Member* p, int size, const char* k, int klen) {
  int left = 0, right = size;
  while (right - left > 1) {
    int mid = left + (right - left) / 2;
    int len = p[mid].KLen();
    if (Compare(p[mid].Key(), len, k, len) <= 0) {
      left = mid;
    } else {
      right = mid;
    }
  }
  /* return value should be less-or-equal compared to the target. */
  return p + left;
}

/* Push Member  by key-order. */
/* @num is the number of member that was just pushed. */
Member* PushMemberInOrder(Member* dst, int num, const char* k, int len, const Value* v) {
  Member* pos = NULL;
  Member* cur = dst + num;
  if (num == 0) {
    pos = cur;
  } else {
    if (Compare((cur - 1)->Key(), (cur - 1)->KLen(), k, len) <= 0) {
      pos = cur;
    } else {
      Member* start = cur - num;
      pos = FindMemberByKey(start, num, k, len);
      if (Compare(pos->Key(), pos->KLen(), k, len) <= 0) {
        pos += 1;
      }
      memmove(pos + 1, pos, (cur - pos) * sizeof(Member));
    }
  }
  memset(pos, 0, sizeof(*pos));
  return pos;
}

Member* PushMemberInOrder(Member* dst, int num, const Member* cur) {
  return PushMemberInOrder(dst, num, cur->Key(), cur->KLen(), cur->Val());
}

bool CompareString(const Value* lhs, const Value* rhs) {
  assert(lhs && rhs && lhs->Type() == kJSON_STRING 
         && rhs->Type() == kJSON_STRING);
  const char* l = lhs->GetString();
  const char* r = rhs->GetString();
  int llen = lhs->GetStringLength(), rlen = rhs->GetStringLength();
  if (llen != rlen) return false;
  return Compare(l, llen, r, rlen) == 0;
}

bool CompareArray(const Value* lhs, const Value* rhs) {
  assert(lhs && rhs && lhs->Type() == kJSON_ARRAY 
         && rhs->Type() == kJSON_ARRAY);
  int num = lhs->GetArraySize();
  if (num != rhs->GetArraySize()) return false;
  if (num == 0) return true;
  const Value* lhs_p = lhs->GetArrayValue(0);
  const Value* rhs_p = rhs->GetArrayValue(0);
  for (int i = 0; i < num; ++i) {
    if (!Compare(lhs_p + i, rhs_p + i)) return false;
  }
  return true;
}

int CompareMember(const Member* lhs, const Member* rhs) {
  assert(lhs && rhs);
  return Compare(lhs->Key(), lhs->KLen(), rhs->Key(), rhs->KLen());
}

bool CompareObject(const Value* lhs, const Value* rhs) {
  assert(lhs && rhs && lhs->Type() == kJSON_OBJECT 
         && rhs->Type() == kJSON_OBJECT);
  int size = lhs->GetObjectSize();
  if (size != rhs->GetObjectSize()) return false;
  if (size == 0) return true;
  const Member* lp = lhs->GetObjectMember(0);
  const Member* rp = rhs->GetObjectMember(0);
  for (int i = 0; i < size; ++i) {
    if (CompareMember(lp + i, rp + i) != 0
        || !Compare(lp[i].Val(), rp[i].Val())) {
      return false;
    }
  }
  return true;
}

/* Note: Object member is sorted by key. */
static Member* FindObjectMemberByKey(Member* p, int size, const char* k, int klen) {
  int left = 0, right = size;
  while (right - left > 1) {
    int mid = left + (right - left) / 2;
    if (Compare(p[mid].Key(), p[mid].KLen(), k, klen) <= 0) {
      left = mid;
    } else {
      right = mid;
    }
  }
  /* return value should be less-or-equal compared to the target. */
  return p + left;
}

/*=============================Generator Static functions=====================*/

void LiteralToString(Stack& stk, const char* s, int len) {
  memcpy(stk.Push(len), s, len);
}

void NumberToString(Stack& stk, const Value* v) {
  char buf[32];
  int len = sprintf(buf, "%.17g", v->GetNumber());
  memcpy(stk.Push(len), buf, len);
}

uint32_t DecodeUTF8(const char* p, int* bytes) {
  uint32_t codepoint = 0;
  if ((*p & 0xF8) == 0xF0) {
    assert((p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80);
    assert((p[3] & 0xC0) == 0x80);
    *bytes = 4;
    codepoint =  p[3] & 0x3F;
    codepoint |= (p[2] & 0x3F) <<  6;
    codepoint |= (p[1] & 0x3F) << 12;
    codepoint |= (p[0] & 0x07) << 18;
  } else if ((*p & 0xF0) == 0xE0) {
    assert((p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80); 
    *bytes = 3;
    codepoint =  p[2] & 0x3F;
    codepoint |= (p[1] & 0x3F) <<  6;
    codepoint |= (p[0] & 0x0F) << 12;             
  } else if ((*p & 0xE0) == 0xC0) {
    assert((p[1] & 0xC0) == 0x80); 
    *bytes = 2;
    codepoint =  p[1] & 0x3F;
    codepoint |= (p[0] & 0x1F) <<  6;    
  }
  return codepoint;
}

void StringToString(Stack& stk, const Value* v) {
  stk.PushString("\"", 1);
  int len = v->GetStringLength();
  const char* p = v->GetString();
  stk.Prepare(len);
  for (int i = 0; i < len;) {
    int step = 1;
    switch (*p) {
      case '\\': stk.PushString("\\\\", 2); break;
      case '\"': stk.PushString("\\\"", 2); break;
      case '\b': stk.PushString("\\b", 2);  break;
      case '\f': stk.PushString("\\f", 2);  break;
      case '\n': stk.PushString("\\n", 2);  break;
      case '\r': stk.PushString("\\r", 2);  break;
      case '\t': stk.PushString("\\t", 2);  break;
      case '/':  stk.PushString("\\/", 2);  break;
      default:   if (*p < 0x20) {
                   stk.PushHex(*p);
                 } else if ((*p & 0x80) == 0x00) {
                   stk.PushString(p, 1);
                 } else {
                   int bytes = 0;
                   uint32_t codepoint = DecodeUTF8(p, &bytes);
                   step = bytes;
                   if (codepoint < 0x100000) {
                     stk.PushHex(codepoint & 0xFFFF);
                   } else {
                     // extract surrogate pair;
#pragma GCC diagnostic ignored "-Wconversion"
                     uint16_t high = static_cast<uint16_t>(((codepoint - 0x10000) >> 10));
                     uint16_t low =  static_cast<uint16_t>((codepoint - 0x10000 - (high << 10)) + 0xDC00);
                     high += static_cast<uint16_t>(0xD800);
#pragma GCC diagnostic error "-Wconversion"
                     stk.PushHex(high);
                     stk.PushHex(low);
                   }
                 }
    }
    p += step;
    i += step;
  }
  stk.PushString("\"", 1);
}

void ValueToString(Stack& stk, const Value* v);
void ArrayToString(Stack& stk, const Value* v) {
  stk.PushString("[", 1);  
  int size = v->GetArraySize();
  for (int i = 0; i < size; ++i) {
    ValueToString(stk, v->GetArrayValue(i));
    stk.PushString(",", 1);
  }
  if (size) stk.Pop(1); // pop the extra comma
  stk.PushString("]", 1);  
}

void ObjectToString(Stack& stk, const Value* v) {
  stk.PushString("{", 1);  
  int size = v->GetObjectSize();
  for (int i = 0; i < size; ++i) {
    const Member* m  = v->GetObjectMember(i);
    stk.PushString("\"", 1);
    stk.PushString(m->Key(), m->KLen());
    stk.PushString("\"", 1);
    stk.PushString(":", 1);
    ValueToString(stk, m->Val());
    stk.PushString(",", 1);
  }
  if (size) stk.Pop(1); // pop the extra comma
  stk.PushString("}", 1);  
}

void ValueToString(Stack& stk, const Value* v) {
  switch (v->Type()) {
    case kJSON_NULL:   LiteralToString(stk, "null", 4);  break;
    case kJSON_FALSE:  LiteralToString(stk, "false", 5); break;
    case kJSON_TRUE:   LiteralToString(stk, "true", 4);  break;
    case kJSON_NUMBER: NumberToString(stk, v);           break; 
    case kJSON_STRING: StringToString(stk, v);           break;
    case kJSON_ARRAY:  ArrayToString(stk, v);            break;
    case kJSON_OBJECT: ObjectToString(stk, v);           break;
  }
}

inline char* CopyBytes(const char* k, int len) {
  char* p = static_cast<char*>(malloc(len));
  if (p) memcpy(p, k, len);
  return p;
}

inline char* CopyWithNull(const char* k, int len) {
  char* p = static_cast<char*>(malloc(len + 1));
  if (p) {
    memcpy(p, k, len);
    p[len] = '\0';
  }
  return p;
}

inline void* MallocWithClear(int size) {
  void* p = malloc(size);
  if (p) memset(p, 0, size);
  return p;
}

} // static-function namespace

bool Compare(const Value* lhs, const Value* rhs) {
  assert(lhs && rhs);
  ValueType l = lhs->Type(), r = rhs->Type();
  if (l != r) return false;
  switch (l) {
    case kJSON_NULL:   // fall through
    case kJSON_FALSE:  // fall through
    case kJSON_TRUE:   return true;
    case kJSON_NUMBER: return (lhs->GetNumber() == rhs->GetNumber());
    case kJSON_STRING: return CompareString(lhs, rhs);
    case kJSON_ARRAY:  return CompareArray(lhs, rhs);
    case kJSON_OBJECT: return CompareObject(lhs, rhs);
    default:           return false; // won't be here.
  }
}

Status Value::ParseObject(Stack& stk, Slice& s) {
  type_ = kJSON_OBJECT;
  s.Move(1);
  const char* p = s.Ptr();
  SkipSpace(s);
  if (*p == '}') {
    val_.o.m = NULL;
    val_.o.size = 0;
    s.Move(1);
    return kJSON_OK;
  }

  Status ret;
  int len;
  int num = 0;
  while (true) {
    SkipSpace(s);

    /* parse the key part */
    p = s.Ptr();
    if (*p != '\"') return kJSON_PARSE_OBJECT_MISSING_KEY;
    len = 0;
    ret = ParseStringInStack(stk, s, len);
    if (ret != kJSON_OK) return ret;
    char* sp = CopyWithNull(stk.Pop(len), len);
    if (sp == NULL) return kJSON_OUT_OF_MEMORY;
    SkipSpace(s);
    if (*(s.Ptr()) != ':') {
      free(sp);
      return kJSON_PARSE_OBJECT_MISSING_COLON;
    }
    s.Move(1); // skip colon

    /* parse the value part */
    Value* val = reinterpret_cast<Value*>(MallocWithClear(sizeof(Value)));
    if (val == NULL) {
      free(sp);
      return kJSON_OUT_OF_MEMORY;
    }
    SkipSpace(s);
    if ((ret = val->ParseValue(stk, s)) != kJSON_OK) {
      free(sp);
      return ret;
    }

    Member* cur = reinterpret_cast<Member*>(stk.Push(sizeof(Member)));
    Member* pos = PushMemberInOrder(cur - num, num, sp, len, val);
    pos->Move(sp, len, val);
    val_.o.size = ++num;
    SkipSpace(s);
    if (*(s.Ptr()) == '}') {
#pragma GCC diagnostic ignored "-Wconversion"
      int bytes = num * sizeof(Member);
#pragma GCC diagnostic error "-Wconversion"
      char* dst = static_cast<char*>(MallocWithClear(bytes));
      if (dst == NULL) return kJSON_OUT_OF_MEMORY;
      memcpy(dst, stk.Pop(bytes), bytes);
      val_.o.m = reinterpret_cast<Member*>(dst);
      s.Move(1);
      break;
    } else if (*(s.Ptr()) == ',') {
      s.Move(1);
      SkipSpace(s);
      if(*(s.Ptr()) == '}') {
        return kJSON_PARSE_OBJECT_INVALID_EXTRA_COMMA;
      }
    } else {
      return kJSON_PARSE_OBJECT_MISSING_COMMA_OR_CURLY_BRACKET;
    }
  }
  return kJSON_OK;
}

Status Value::ParseValue(Stack& stk, Slice& s) {
  switch (*(s.Ptr())) {
    case 'n':  return ParseLiteral(s, 'n');
    case 'f':  return ParseLiteral(s, 'f');
    case 't':  return ParseLiteral(s, 't');
    case '[':  return ParseArray(stk, s);
    case '{':  return ParseObject(stk, s);
    case '\"': return ParseString(stk, s);
    case '\0': return kJSON_PARSE_EXPECT_VALUE;
    default:   return ParseNumber(s);
  }
}

Status Value::ParseLiteral(Slice& s, char c) {
  assert(c == 'n' || c == 'f' || c == 't');
  const char* p = s.Ptr();
  bool valide = false;
  ValueType type;

  if (c == 'n') {
    valide = s.Len() >= 4 && (p[0] == 'n' && p[1] == 'u' && p[2] == 'l' && p[3] == 'l');
    type = kJSON_NULL;
    if (valide) s.Move(4);
  } else if (c == 'f') {
    valide = s.Len() >= 5 && (p[0] == 'f' && p[1] == 'a' && p[2] == 'l' && p[3] == 's' && p[4] == 'e');
    type = kJSON_FALSE;
    if (valide) s.Move(5);
  } else {
    valide = s.Len() >= 4 && (p[0] == 't' && p[1] == 'r' && p[2] == 'u' && p[3] == 'e');
    type = kJSON_TRUE;
    if (valide) s.Move(4);
  }

  if (valide) {
    type_ = type;
    return kJSON_OK;
  }
  return kJSON_PARSE_INVALID_VALUE;
}

Status Value::ParseString(Stack& stk, Slice& s) {
  int len = 0;
  Status ret = ParseStringInStack(stk, s, len);
  if (ret == kJSON_OK) {
    SetString(stk.Pop(len), len);
  }
  return ret;
}

Status Value::ParseArray(Stack& stk, Slice& s) {
  type_ = kJSON_ARRAY;
  SkipSpace(s);
  s.Move(1);
  const char* p = s.Ptr();
  if (*p == ']') {
    val_.a.a = NULL;
    val_.a.size = 0;
    s.Move(1);
    return kJSON_OK;
  }
  int num = 0;
  while (true) {
    Value* val = reinterpret_cast<Value*>(MallocWithClear(sizeof(Value)));
    if (val == NULL) return kJSON_OUT_OF_MEMORY;
    Status ret = val->ParseValue(stk, s);
    if (ret != kJSON_OK) {
      val->Free();
      free(val);
      return ret;
    }
    memcpy(stk.Push(sizeof(*val)), val, sizeof(*val));
    free(val);
    val_.a.size = ++num;
    SkipSpace(s);
    p = s.Ptr();
    if (*p == ']') {
      int bytes = num * static_cast<int>(sizeof(*val));
      char* dst = static_cast<char*>(CopyBytes(stk.Pop(bytes), bytes));
      if (dst == NULL) return kJSON_OUT_OF_MEMORY;
      val_.a.a = reinterpret_cast<Value*>(dst);
      s.Move(1); // skip the ending bracket square
      break;
    } else if (*p == ',') {
      s.Move(1);
      SkipSpace(s);
      if (*(s.Ptr()) == ']') {
        return kJSON_PARSE_ARRAY_INVALID_EXTRA_COMMA;
      }
    } else {
      return kJSON_PARSE_ARRAY_MISSING_COMMA;
    }
  } 
  return kJSON_OK;
}

Status Value::ParseNumber(Slice& s) {
  const char* p = s.Ptr();
  if (!IsInvalidNumber(s)) {
    return kJSON_PARSE_INVALID_VALUE;
  }

  Status ret = kJSON_OK;
  int save_errno = errno;
  errno = 0;
  double num = strtod(p, NULL);
  if (errno) {
    // ERANGE
    if (num == 0) {
      ret = kJSON_PARSE_NUMBER_UNDERFLOW;
    } else {
      ret = kJSON_PARSE_NUMBER_OVERFLOW;
    }
  } else {
    type_ = kJSON_NUMBER;
    val_.num = num;
  }
  errno = save_errno;
  return ret;
}

Value::Value(const Value& rhs): val_({{NULL, 0}}), type_(kJSON_NULL) {
  *this = rhs;  
}

const Value& Value::operator=(const Value& src) {
  if (&src != this) {
    Free();
    type_ = src.Type();
    switch(src.Type()) {
      case kJSON_NULL:
      case kJSON_FALSE:
      case kJSON_TRUE:   break;
      case kJSON_NUMBER: SetNumber(src.GetNumber());
                         break;
      case kJSON_STRING: SetString(src.GetString(), src.GetStringLength());
                         break;
      case kJSON_ARRAY:  SetArray(&src);
                         break;
      case kJSON_OBJECT: SetObject(&src);
                         break;
      default:           assert(false);
    }
  }
  return *this;
}

Value::~Value() {
  Free();
}

void Value::Free() {
  if (type_ == kJSON_STRING) {
    if (val_.s.s) free(val_.s.s);
    val_.s.s = NULL;
    val_.s.len = 0;
  } else if (type_ == kJSON_ARRAY) {
    int mem_size = val_.a.size;
    Value* p = val_.a.a;
    for (int i = 0; i < mem_size; ++i) {
      p[i].Free();
    }
    if (p) free(p);
    val_.a.a = NULL;
    val_.a.size = 0;
  } else if (type_ == kJSON_OBJECT) {
    int mem_size = val_.o.size;
    Member* p = val_.o.m;
    for (int i = 0; i < mem_size; ++i) {
      p[i].Free();
    }
    if (p) free(p);
    val_.o.m = NULL;
    val_.o.size = 0;
  }
}

void Value::FreeOnError(Stack& stk) {
    int num = 0;
    if (type_ == kJSON_ARRAY) {
      Value* p = reinterpret_cast<Value*>(stk.Dump());
      num = val_.a.size;
      for (int i = 0; i < num; ++i) {
        (p + i)->Free();
      }
      val_.a.size = 0;
    } else if (type_ == kJSON_OBJECT) {
      Member* p = reinterpret_cast<Member*>(stk.Dump());
      num = val_.o.size;
      for (int i = 0; i < num; ++i) {
        (p + i)->Free();
      }
      val_.o.size = 0;
    }
    stk.Free();
}

Status Value::Parse(const char* text, int len) {
  assert(text != NULL);
  Stack stk;
  Slice s(text, len);
  SkipSpace(s);
  Status ret = ParseValue(stk, s);
  if (ret == kJSON_OK) {
    if (!CheckSingular(s)) {
      ret = kJSON_PARSE_ROOT_NOT_SINGULAR;
    }
  }
  if (ret != kJSON_OK) {
    FreeOnError(stk);
  } else {
    stk.Free();
  }
  return ret;
}

double Value::GetNumber() const {
  assert(type_ == kJSON_NUMBER);
  return val_.num;
}

void Value::SetNumber(double num) {
  Reset();
  type_ = kJSON_NUMBER;
  val_.num = num;
}

bool Value::GetBoolean() const {
  assert(type_ == kJSON_FALSE || type_ == kJSON_TRUE);
  return (type_ == kJSON_FALSE) ? false : true;
}

void Value::SetBoolean(bool b) {
  Reset();
  if (b) {
    type_ = kJSON_TRUE;
  } else {
    type_ = kJSON_FALSE;
  }
}

void Value::SetString(const char* s, int len) {
  Reset();
  type_ = kJSON_STRING;
  val_.s.len = len;
  char* p = val_.s.s = static_cast<char*>(CopyWithNull(s, len));
  assert(p != NULL);
  (void)p;
}

const char* Value::GetString() const {
  assert(type_ == kJSON_STRING);
  return val_.s.s;
}

char* Value::GetString() { 
  return const_cast<char*>(
    const_cast<const Value*>(this)->GetString()
  );
}

int Value::GetStringLength() const { 
  assert(type_ == kJSON_STRING);
  return val_.s.len;
}

int Value::GetArraySize() const {
  assert(type_ == kJSON_ARRAY);
  return val_.a.size;
}

const Value* Value::GetArrayValue(int index) const {
  assert(type_ == kJSON_ARRAY);
  assert(index >= 0 && index < val_.a.size);
  assert(val_.a.a);
  return (val_.a.a + index);
}

Value* Value::GetArrayValue(int index) {
  return const_cast<Value*>(
    const_cast<const Value*>(this)->GetArrayValue(index)
  );
}

void Value::SetArrayValue(int index, Value* value) {
  assert(type_ == kJSON_ARRAY && value);
  assert(index >= 0 && index < val_.a.size);
  Value* v = val_.a.a + index;
  *v = *value;
}

/*TODO: Omit the realloc when the array is empty. */
void Value::MergeArrayBuilder(Builder<Value>& b) {
  assert(type_ == kJSON_ARRAY);
  int num = 0;
  const Value* p = b.Dump(num);
  if (num == 0) return;
  val_.a.size += num;
  val_.a.a = reinterpret_cast<Value*>(realloc(val_.a.a, val_.a.size * sizeof(*p)));
  memcpy(val_.a.a + val_.a.size - num, p, num * sizeof(Value));
}

int Value::GetObjectSize() const {
  assert(type_ == kJSON_OBJECT);
  return val_.o.size;
}

const Member* Value::GetObjectMember(int index) const {
  assert(type_ == kJSON_OBJECT);
  assert(index >= 0 && index < val_.o.size);
  assert(val_.o.m);
  return (val_.o.m + index);
}

Member* Value::GetObjectMember(int index) { 
  return const_cast<Member*>(
    const_cast<const Value*>(this)->GetObjectMember(index)
  ); 
}

const Member* Value::GetMemberByKey(const char* k, int klen) const {
  int size = val_.o.size;
  Member* p = FindObjectMemberByKey(val_.o.m, size, k, klen);
  return Compare(k, klen, p->Key(), p->KLen()) == 0 ? p : NULL;
}

Member* Value::GetMemberByKey(const char* k, int klen) {
  return const_cast<Member*>(
    const_cast<const Value*>(this)->GetMemberByKey(k, klen)
  ); 
}

const Value* Value::GetValueByKey(const char* k, int klen) const {
  assert(type_ == kJSON_OBJECT);
  const Member* p = GetMemberByKey(k, klen);
  return p ? p->Val() : NULL;
}

Value* Value::GetValueByKey(const char* k, int klen) {
  return const_cast<Value*>(
    const_cast<const Value*>(this)->GetValueByKey(k, klen)
  ); 
}

void Value::MergeObjectBuilder(Builder<Member>& b) {
  assert(type_ == kJSON_OBJECT);
  int num = 0;
  const Member* p = b.Dump(num);
  if (num == 0) return;
  int ready = val_.o.size;
  val_.o.m = reinterpret_cast<Member*>(realloc(val_.o.m, (ready + num) * sizeof(*p)));
  for (int i = 0; i < num; ++i) {
    Member* pos = PushMemberInOrder(val_.o.m, ready, p + i);
    pos->Move((p + i)->Key(), (p + i)->KLen(), (p + i)->Val());
    ready++;
  }
  val_.o.size = ready; 
}

bool Value::SetObjectKeyValue(const char* k, int klen, Value* value) {
  assert(type_ == kJSON_OBJECT && k && value);
  Member* m = GetMemberByKey(k, klen);
  if (m) {
    m->Set(k, klen, value);  
    return true;
  }
  return false;
}

void Value::SetArray(const Value* src) {
  assert(src && type_ == kJSON_ARRAY && src->Type() == kJSON_ARRAY);
  if (src == this) return;
  Free();
  int size = src->GetArraySize();
#pragma GCC diagnostic ignored "-Wconversion"
  Value* a = reinterpret_cast<Value*>(MallocWithClear(size * sizeof(*src)));
#pragma GCC diagnostic error "-Wconversion"
  for (int i = 0; i < size; ++i) {
    *(a + i) = *src->GetArrayValue(i);
  }
  val_.a.a = a;
  val_.a.size = size;
}

void Value::SetObject(const Value* src) {
  assert(src && type_ == kJSON_OBJECT && src->Type() == kJSON_OBJECT);
  if (src == this) return;
  Free();
  int size = src->GetObjectSize();
#pragma GCC diagnostic ignored "-Wconversion"
  Member* m = reinterpret_cast<Member*>(MallocWithClear(size * sizeof(Member)));
#pragma GCC diagnostic error "-Wconversion"
  for (int i = 0; i < size; ++i) {
    const Member* p = src->GetObjectMember(i);
    (m + i)->Set(p->Key(), p->KLen(), p->Val());        
  }
  val_.o.m = m;
  val_.o.size = size;
}

void Value::Reset(ValueType t) {
  Free();
  type_ = t;
}

Member::Member(const Member& rhs) {
  *this = rhs;
}

const Member& Member::operator=(const Member& rhs) {
  Set(rhs.Key(), rhs.KLen(), rhs.Val());
  return *this;
}

Member::~Member() {
  Free();
}

void Member::Free() {
  if (k_) {
    free(k_);
    k_ = NULL;
    len_ = 0;
  }
  if (v_) {
    v_->Reset();
    free(v_);
    v_ = NULL;
  }
}

void Member::SetKey(const char* k, int len) {
  assert(k);
  if (k_ != k) {
    free(k_);
    k_ = CopyWithNull(k, len);
    len_ = len;
  }
}

void Member::SetValue(const Value* v) {
  assert(v);
  if (v_ != v) {
    if(v_) {
      v_->Reset(); 
    } else {
      v_ = reinterpret_cast<Value*>(MallocWithClear(sizeof(*v)));
    }
    *v_ = *v;
  }
}

void Member::Set(const char* k, int len, const Value* v) {
  SetKey(k, len);
  SetValue(v);
}

void Member::MoveKey(const char* k, int len) {
  assert(k);
  if (k_ != k) {
    free(k_);
    k_ = const_cast<char*>(k);
    len_ = len;
  }
}

void Member::MoveValue(const Value* v) {
  assert(v);
  if (v_ != v) {
    if(v_) v_->Reset(); 
    v_ = const_cast<Value*>(v);
  }
}

void Member::Move(const char* k, int len, const Value* v) {
  MoveKey(k, len);
  MoveValue(v);
}

std::string Value::ToString() const {
  Stack stk;
  ValueToString(stk, this);
  int len = stk.Top();
  return (std::string(stk.Pop(len), len)).append(1, '\0');
}

Value& operator<<(Value& v, double num) {
  v.SetNumber(num);
  return v;
}

Value& operator<<(Value& v, const std::string& s) {
  v.SetString(s.c_str(), static_cast<int>(s.size()));
  return v;
}

void operator>>(const Value& v, double& num) {
  assert(v.Type() == kJSON_NUMBER);
  num = v.GetNumber();
}

void operator>>(const Value& v, std::string& s) {
  assert(v.Type() == kJSON_STRING);
  s = std::string(v.GetString(), v.GetStringLength());
}

Builder<Value>& operator<<(Builder<Value>& b, const Value& data) {
  Value* p = b.Push();
  p->Reset();
  *p = data;
  return b;
}

Builder<Member>& operator<<(Builder<Member>& b, const Member& data) {
  Member* p = b.Push();
  p->Set(data.Key(), data.KLen(), data.Val());
  return b;
}

} // namespace jsonutil
