#ifndef JSONUTIL_H__
#define JSONUTIL_H__

#include <stdint.h>

namespace jsonutil {
typedef enum {
  kJSON_TYPE_NULL, 
  kJSON_TYPE_FALSE, 
  kJSON_TYPE_TRUE, 
  kJSON_TYPE_NUMBER, 
  kJSON_TYPE_STRING, 
  kJSON_TYPE_ARRAY, 
  kJSON_TYPE_OBJECT
} JsonType;

typedef struct JsonValue JsonValue;
typedef struct ObjectMember ObjectMember;
struct JsonValue {
  union {
    struct {
      ObjectMember* m;
      int size;
    } obj;
    struct {
      JsonValue* a;
      int size; // num of elements in the array
    } array;
    struct {
      char* s;
      int len;
    } str;
    double num;
  } data;
  JsonType type;
};

struct ObjectMember {
  char* k;
  int klen;
  JsonValue* v;
};

typedef struct {
  const char* json_text;
  char* stack;
  int top;
  int size;
} ArgContext;

typedef enum {
  kJSON_OK = 0,
  kJSON_PARSE_EXPECT_VALUE,
  kJSON_PARSE_INVALID_VALUE,
  kJSON_PARSE_ROOT_NOT_SINGULAR,
  kJSON_PARSE_NUMBER_OVERFLOW,
  kJSON_PARSE_NUMBER_UNDERFLOW,
  kJSON_PARSE_STRING_NO_END_MARK,
  kJSON_PARSE_STRING_ESCAPED_INVALID_CHAR,
  kJSON_PARSE_STRING_INVALID_CHAR,
  kJSON_PARSE_STRING_UNICODE_INVALID_HEX,
  kJSON_PARSE_STRING_UNICODE_INVALID_SURROGATE,
  kJSON_PARSE_STRING_UNICODE_INVALID_RANGE,
  kJSON_PARSE_ARRAY_INVALID_EXTRA_COMMA,
  kJSON_PARSE_ARRAY_MISSING_COMMA,
  kJSON_PARSE_OBJECT_MISSING_KEY,
  kJSON_PARSE_OBJECT_MISSING_COLON,
  kJSON_PARSE_OBJECT_INVALID_EXTRA_COMMA,
  kJSON_PARSE_OBJECT_MISSING_COMMA_OR_CURLY_BRACKET,
  kJSON_OUT_OF_MEMORY
} Status;

void FreeContext(ArgContext* arg_ptr);
char* ContextPop(ArgContext* arg_ptr, int size);
char* ContextPush(ArgContext* arg_ptr, int size);
void ContextPrepare(ArgContext* arg_ptr, int size);
void ContextPushUint32(ArgContext* arg_ptr, uint32_t* ptr, int size);
int CompareBytes(const char* l, int llen, const char* r, int rlen);

Status ParseJson(JsonValue* v, const char* json_text);
bool GetBoolean(const JsonValue* v);
void SetBoolean(JsonValue* v, bool b);

void SetNull(JsonValue* v);
void InitValue(JsonValue* v);
JsonType GetType(const JsonValue* v);

double GetNumber(const JsonValue* v);
void SetNumber(JsonValue* v, double num);

void SetString(JsonValue* v, const char* str, int len);
const char* GetString(const JsonValue* v);
int GetStringLength(const JsonValue* v);

int GetArraySize(const JsonValue* v);
JsonValue* GetArrayElement(const JsonValue* v, int index);
void SetArrayElement(JsonValue* v, int index, JsonValue* value);
void ContextPushValue(ArgContext* arg_ptr, JsonValue* v);
void PushArrayValue(ArgContext* arg_ptr, JsonValue* array);

int GetObjectSize(const JsonValue* o);
ObjectMember* GetObjectMember(const JsonValue* o, int index);
char* GetObjectMemberKey(const ObjectMember* o, int& size);
JsonValue* GetObjectMemberValue(const ObjectMember* o);
JsonValue* GetValueByKey(const JsonValue* o, const char* k, int klen);

/* Note: object member is sorted by key-order */
void ContextPushMember(ArgContext* arg_ptr, const char* k, int klen, JsonValue* v);
void PushObjectMember(ArgContext* arg_ptr, JsonValue* o);
bool SetObjectKeyValue(JsonValue* o, const char* k, int klen, JsonValue* value);
void SetObjectMember(ObjectMember* m, const char* k, int klen, const JsonValue* v);
bool JsonCompare(const JsonValue* lhs, const JsonValue* rhs);
char* JsonToString(const JsonValue* v, int* len);
void CopyJsonValue(JsonValue* dst, const JsonValue* src);
} // namespace jsonutil
#endif // JSONUTIL_H__

