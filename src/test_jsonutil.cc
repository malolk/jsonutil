#include "json_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_RESET   "\x1b[0m"

using namespace jsonutil;

#define TEST_EQUAL_CHECK(answer, result, fmt, func_name, line, check) \
  do { \
    if (!check) { \
      fprintf(stderr, "%20s:%-10d Answer: %-20" fmt " Result: %-20" \
              fmt ANSI_COLOR_RED " %10s" ANSI_COLOR_RESET "\n", \
              func_name, line, answer, result, "[failed]"); \
    } else {\
      fprintf(stderr, "%20s:%-10d Answer: %-20" fmt " Result: %-20" \
              fmt ANSI_COLOR_GREEN " %10s" ANSI_COLOR_RESET "\n", \
              func_name, line, answer, result, "[passed]"); \
    } \
  } while (0)

#define TEST_EQUAL(answer, result, fmt) \
  TEST_EQUAL_CHECK(answer, result, fmt, __func__, __LINE__, (answer == result))

static void TestParseNull() {
  JsonValue val;
  val.type = kJSON_TYPE_TRUE;
  TEST_EQUAL(kJSON_OK, ParseJson(&val, "null"), "d");
  TEST_EQUAL(kJSON_TYPE_NULL, GetType(&val), "d");
}

static void TestParseFalse() {
  JsonValue val;
  val.type = kJSON_TYPE_NULL;
  TEST_EQUAL(kJSON_OK, ParseJson(&val, "false"), "d");
  TEST_EQUAL(kJSON_TYPE_FALSE, GetType(&val), "d");
}

static void TestParseTrue() {
  JsonValue val;
  val.type = kJSON_TYPE_NULL;
  TEST_EQUAL(kJSON_OK, ParseJson(&val, "true"), "d");
  TEST_EQUAL(kJSON_TYPE_TRUE, GetType(&val), "d");
}

static void TestParseNotSingular() {
  JsonValue val;
  val.type = kJSON_TYPE_TRUE;
  TEST_EQUAL(kJSON_PARSE_ROOT_NOT_SINGULAR, ParseJson(&val, "null extra-bytes"), "d");  
  TEST_EQUAL(kJSON_PARSE_ROOT_NOT_SINGULAR, ParseJson(&val, "false extra-bytes"), "d");
  TEST_EQUAL(kJSON_PARSE_ROOT_NOT_SINGULAR, ParseJson(&val, "true extra-bytes"), "d");
  TEST_EQUAL(kJSON_PARSE_ROOT_NOT_SINGULAR, ParseJson(&val, "1.23 extra-bytes"), "d");
}

void TestParseValidNumberImpl(double num, const char* num_str, 
                              const char* func, int line) {
  JsonValue val;
  InitValue(&val);
  Status s = ParseJson(&val, num_str);
  TEST_EQUAL_CHECK(kJSON_OK, s, "d", func, line, (s == kJSON_OK));
  double res = GetNumber(&val);
  TEST_EQUAL_CHECK(num, res, "f", func, line, (num == res));
}

#define TestParseValidNumber(num, num_str) \
    TestParseValidNumberImpl(num, num_str, __func__, __LINE__);

void TestParseInvalidNumberImpl(const char* num_str, 
                              const char* func, int line) {
  JsonValue val;
  InitValue(&val);
  Status s = ParseJson(&val, num_str);
  TEST_EQUAL_CHECK(kJSON_PARSE_INVALID_VALUE, s, "d", func, line, (s == kJSON_PARSE_INVALID_VALUE));
}

#define TestParseInvalidNumber(num_str) \
  TestParseInvalidNumberImpl(num_str, __func__, __LINE__);

static void TestParseNumber() {
  /* test valid number */
  TestParseValidNumber(0.0, "0.0 ");
  TestParseValidNumber(0.0, "-0");
  TestParseValidNumber(1.0, "1");
  TestParseValidNumber(-1.0, "-1");
  TestParseValidNumber(1.2, "1.2");
  TestParseValidNumber(-1.2, "-1.2");
  TestParseValidNumber(1.0, "1.");
  TestParseValidNumber(1e10, "1e10");
  TestParseValidNumber(1E10, "1E10");
  TestParseValidNumber(1e-10, "1e-10");
  TestParseValidNumber(-1e10, "-1e10");
  TestParseValidNumber(-1e-10, "-1e-10");
  TestParseValidNumber(1e+10, "1e+10");
  TestParseValidNumber(1.e-10, "1.e-10");
  TestParseValidNumber(1.2e-10, "1.2e-10");
  TestParseValidNumber(1, "1e");
  TestParseValidNumber(1, "1e-");
  TestParseValidNumber(1, "1e+");
  TestParseValidNumber(1, "1.e");

  /* test invalid number */
  TestParseInvalidNumber("+0");
  TestParseInvalidNumber("+1");
  TestParseInvalidNumber("-");
  TestParseInvalidNumber(".123");
  TestParseInvalidNumber("INF");
  TestParseInvalidNumber("inf");
  TestParseInvalidNumber("NAN");
  TestParseInvalidNumber("nan"); 

  /* test under/overflow number */
  JsonValue val;
  InitValue(&val);
  TEST_EQUAL(kJSON_PARSE_NUMBER_OVERFLOW, ParseJson(&val, "1e+10000"), "d");
  SetNull(&val);
  TEST_EQUAL(kJSON_PARSE_NUMBER_OVERFLOW, ParseJson(&val, "-1e+10000"), "d");
  SetNull(&val);
  TEST_EQUAL(kJSON_PARSE_NUMBER_UNDERFLOW, ParseJson(&val, "1e-10000"), "d");
}

#define TEST_EQUAL_STRING(ans, len1, res, len2) TEST_EQUAL_CHECK(ans, res, "s", __func__, __LINE__, !CompareBytes(ans, len1, res, len2))

void TestSetterAndGetter() {
  JsonValue val;
  InitValue(&val);
  TEST_EQUAL(0.0, (SetNumber(&val, 0.0), GetNumber(&val)), "f");
  TEST_EQUAL(1.0, (SetNumber(&val, 1.0), GetNumber(&val)), "f");
  TEST_EQUAL(-1.2, (SetNumber(&val, -1.2), GetNumber(&val)), "f");
  TEST_EQUAL(kJSON_TYPE_NUMBER, GetType(&val), "d");

  TEST_EQUAL(true, (SetBoolean(&val, true), GetBoolean(&val)), "d");
  TEST_EQUAL(kJSON_TYPE_TRUE, GetType(&val), "d");
  TEST_EQUAL(false, (SetBoolean(&val, false), GetBoolean(&val)), "d");
  TEST_EQUAL(kJSON_TYPE_FALSE, GetType(&val), "d");

  TEST_EQUAL_STRING("Wonderful", 9, (SetString(&val, "Wonderful", 9), GetString(&val)), 9);
  TEST_EQUAL(9, GetStringLength(&val), "d");
  TEST_EQUAL_STRING("Tonight", 7, (SetString(&val, "Tonight", 7), GetString(&val)), 7);
  TEST_EQUAL(7, GetStringLength(&val), "d");
  TEST_EQUAL(kJSON_TYPE_STRING, GetType(&val), "d");
  SetNull(&val); // free the space allocated for string.
}

static void TestParseStringValidImpl(const char* ans, const char* text,
                                     int len, const char* func, int line) {
  JsonValue val;
  InitValue(&val);
  Status s = ParseJson(&val, text);
  TEST_EQUAL_CHECK(kJSON_OK, s, "d", func, line, (kJSON_OK == s));
  int get_len = GetStringLength(&val);
  TEST_EQUAL_CHECK(len, get_len, "d", func, line, (len == get_len));
  const char* get_str = GetString(&val);
  TEST_EQUAL_CHECK(ans, get_str, "s", func, line, (!CompareBytes(ans, len, get_str, get_len)));
  SetNull(&val);
}

#define TestParseStringValid(ans, text, len) \
  TestParseStringValidImpl(ans, text, len, __func__, __LINE__)

static void TestParseStringInvalidImpl(jsonutil::Status error, const char* text, 
                                       int len, const char* func, int line) {
  JsonValue val;
  InitValue(&val);
  Status s = ParseJson(&val, text);
  TEST_EQUAL_CHECK(error, s, "d", func, line, (error == s));
  SetNull(&val);
}

#define TestParseStringInvalid(error, text, len) \
  TestParseStringInvalidImpl(error, text, len, __func__, __LINE__)
static void TestParseString() {
  TestParseStringValid("", "\"\"", 0);
  TestParseStringValid("json", "\"json\"", 4);
  TestParseStringValid("\n", "\"\\n\"", 1);
  TestParseStringValid("\r", "\"\\r\"", 1);
  TestParseStringValid("\f", "\"\\f\"", 1);
  TestParseStringValid("\b", "\"\\b\"", 1);
  TestParseStringValid("\t", "\"\\t\"", 1);
  TestParseStringValid("/", "\"\\/\"", 1);
  TestParseStringValid("\"", "\"\\\"\"", 1);
  TestParseStringValid("\"", "\"\\\"\"", 1);
  TestParseStringValid("\\", "\"\\\\\"", 1);
  
  TestParseStringInvalid(kJSON_PARSE_STRING_NO_END_MARK, "\"json", 4);
  TestParseStringInvalid(kJSON_PARSE_STRING_INVALID_CHAR, "\"\x01\"", 1);
  TestParseStringInvalid(kJSON_PARSE_STRING_INVALID_CHAR, "\"\x1F\"", 1);
  TestParseStringInvalid(kJSON_PARSE_STRING_ESCAPED_INVALID_CHAR, "\"\\\x01\"", 1);
  TestParseStringInvalid(kJSON_PARSE_STRING_ESCAPED_INVALID_CHAR, "\"\\\x1F\"", 1);
  TestParseStringInvalid(kJSON_PARSE_STRING_ESCAPED_INVALID_CHAR, "\"\\v\"", 1);
  TestParseStringInvalid(kJSON_PARSE_STRING_ESCAPED_INVALID_CHAR, "\"\\0\"", 1);
  TestParseStringInvalid(kJSON_PARSE_STRING_NO_END_MARK, "\"json", 4);
  TestParseStringInvalid(kJSON_PARSE_STRING_UNICODE_INVALID_HEX, "\"\\ug\"", 1);
  TestParseStringInvalid(kJSON_PARSE_STRING_UNICODE_INVALID_SURROGATE, "\"\\uD800\"", 1);
  TestParseStringInvalid(kJSON_PARSE_STRING_UNICODE_INVALID_SURROGATE, "\"\\uD800\\uCD00\"", 1);
  TestParseStringInvalid(kJSON_PARSE_STRING_UNICODE_INVALID_SURROGATE, "\"\\uDC00\"", 1);
  TestParseStringInvalid(kJSON_PARSE_STRING_UNICODE_INVALID_SURROGATE, "\"\\u0000\\uDC00\"", 1);
}

static void TestParseValueValidImpl(JsonValue* res, JsonValue* ans, const char* func, int line) {
  JsonType res_t = res->type, ans_t = ans->type;
  bool flag = JsonCompare(res, ans);
  TEST_EQUAL_CHECK("-", "-", "s", func, line, flag);
}

#define TestParseValueValid(array, ans) \
  TestParseValueValidImpl(array, ans, __func__, __LINE__)

static void TestParseArray() {
  JsonValue val, chk_val, str_val;
  InitValue(&val);
  InitValue(&chk_val);
  InitValue(&str_val);
  SetString(&str_val, "abc", 3);
  const char* text0 = "[null, true, false, \"abc\"]";
  Status s = ParseJson(&val, text0);
  if (s != kJSON_OK) abort();

  TEST_EQUAL(4, GetArraySize(&val), "d");
  TestParseValueValid(GetArrayElement(&val, 0), &chk_val);
  TestParseValueValid(GetArrayElement(&val, 1), (SetBoolean(&chk_val, true), &chk_val));
  TestParseValueValid(GetArrayElement(&val, 2), (SetBoolean(&chk_val, false), &chk_val));
  TestParseValueValid(GetArrayElement(&val, 3), &str_val);
  SetNull(&chk_val);
  SetNull(&val);

  JsonValue ary0, ary1;
  InitValue(&ary0);
  InitValue(&ary1);
  if ((s = ParseJson(&ary0, "[]")) != kJSON_OK) abort();
  TEST_EQUAL(kJSON_TYPE_ARRAY, GetType(&ary0), "d");
  TEST_EQUAL(0, GetArraySize(&ary0), "d");

  if ((s = ParseJson(&ary1, "[1, 2, 3]")) != kJSON_OK) abort();
  TEST_EQUAL(kJSON_TYPE_ARRAY, GetType(&ary0), "d");
  TEST_EQUAL(3, GetArraySize(&ary1), "d");
  TestParseValueValid(GetArrayElement(&ary1, 0), (SetNumber(&chk_val, 1), &chk_val));
  TestParseValueValid(GetArrayElement(&ary1, 1), (SetNumber(&chk_val, 2), &chk_val));
  TestParseValueValid(GetArrayElement(&ary1, 2), (SetNumber(&chk_val, 3), &chk_val));
  SetNull(&chk_val);

  const char* text1 = "[[], [1, 2, 3], \"abc\"]";
  if ((s = ParseJson(&val, text1)) != kJSON_OK) abort();
  TEST_EQUAL(kJSON_TYPE_ARRAY, GetType(&val), "d");
  TEST_EQUAL(3, GetArraySize(&val), "d");
  TestParseValueValid(GetArrayElement(&val, 0), &ary0);
  JsonValue* res = GetArrayElement(&val, 1);
  TEST_EQUAL(kJSON_TYPE_ARRAY, GetType(res), "d");
  TEST_EQUAL(kJSON_TYPE_NUMBER, GetType(GetArrayElement(res, 0)), "d");
  
  TestParseValueValid(GetArrayElement(&val, 1), &ary1);
  TestParseValueValid(GetArrayElement(&val, 2), &str_val);
  SetArrayElement(&val, 1, &str_val);
  TestParseValueValid(GetArrayElement(&val, 1), GetArrayElement(&val, 2));

  ArgContext c;
  memset(&c, 0, sizeof(c));
  ContextPushValue(&c, &ary1);
  ContextPushValue(&c, &str_val);
  PushArrayValue(&c, &val);
  TEST_EQUAL(5, GetArraySize(&val), "d");
  TestParseValueValid(GetArrayElement(&val, 3), &ary1);
  TestParseValueValid(GetArrayElement(&val, 4), &str_val);
  FreeContext(&c);

  SetNull(&ary0);
  SetNull(&val);
  SetNull(&ary1);
  SetNull(&str_val);

  JsonValue val0;
  InitValue(&val0);
  TEST_EQUAL(kJSON_PARSE_ARRAY_INVALID_EXTRA_COMMA, ParseJson(&val0, "[1,]"), "d");
  TEST_EQUAL(kJSON_PARSE_ARRAY_MISSING_COMMA, ParseJson(&val0, "[1 1]"), "d");
}

static void TestParseObject() {
  JsonValue obj, va, vb, vc;
  InitValue(&obj);
  InitValue(&va);
  InitValue(&vb);
  InitValue(&vc);
  Status s = ParseJson(&obj, "{\"c\":null, \"b\": [0], \"a\": \"abc\"}");

  TEST_EQUAL(kJSON_OK, s, "d");
  if (kJSON_OK != s) abort();
  TEST_EQUAL(kJSON_TYPE_OBJECT, GetType(&obj), "d");
  TEST_EQUAL(3, GetObjectSize(&obj), "d");
  ParseJson(&va, "\"abc\"");
  ParseJson(&vb, "[0]");
  ParseJson(&vc, "null");
  ObjectMember* mem0 = GetObjectMember(&obj, 0);
  ObjectMember* mem1 = GetObjectMember(&obj, 1);
  ObjectMember* mem2 = GetObjectMember(&obj, 2);
  TEST_EQUAL_STRING("a", 1, mem0->k, mem0->klen);
  TEST_EQUAL_STRING("b", 1, mem1->k, mem1->klen);
  TEST_EQUAL_STRING("c", 1, mem2->k, mem2->klen);
  TestParseValueValid(&va, mem0->v);
  TestParseValueValid(&va, GetValueByKey(&obj, "a", 1));
  TestParseValueValid(&vb, mem1->v);
  TestParseValueValid(&vb, GetValueByKey(&obj, "b", 1));
  TestParseValueValid(&vc, mem2->v);
  TestParseValueValid(&vc, GetValueByKey(&obj, "c", 1));

  ArgContext c;
  memset(&c, 0, sizeof(c));
  ContextPushMember(&c, "aa", 2, &va);
  ContextPushMember(&c, "ab", 2, &vb);
  PushObjectMember(&c, &obj);
  FreeContext(&c);
  TEST_EQUAL(5, GetObjectSize(&obj), "d");
  mem1 = GetObjectMember(&obj, 1);
  mem2 = GetObjectMember(&obj, 2);
  TEST_EQUAL_STRING("aa", 2, mem1->k, mem1->klen);
  TEST_EQUAL_STRING("ab", 2, mem2->k, mem2->klen);
  TestParseValueValid(&va, mem1->v);
  TestParseValueValid(&vb, mem2->v);
  TestParseValueValid(&va, GetValueByKey(&obj, "aa", 2));
  TestParseValueValid(&vb, GetValueByKey(&obj, "ab", 2));

  JsonValue cp;
  InitValue(&cp);
  CopyJsonValue(&cp, &obj);
  TestParseValueValid(&cp, &obj);
  SetNull(&cp);

  /* test recursive Object-type value */
  JsonValue robj;
  s = ParseJson(&robj, "{\"abc\" : {\"c\":null, \"aa\":\"abc\", \"ab\":[0], \"b\":[0], \"a\":\"abc\"}}");
  TEST_EQUAL(kJSON_OK, s, "d");
  if (kJSON_OK != s) abort();
  TEST_EQUAL(kJSON_TYPE_OBJECT, GetType(&robj), "d");
  TEST_EQUAL(1, GetObjectSize(&robj), "d");
  ObjectMember* mem3 = GetObjectMember(&robj, 0);
  TEST_EQUAL_STRING("abc", 3, mem3->k, mem3->klen);
  TestParseValueValid(&obj, mem3->v);
  SetNull(&robj);

  SetNull(&va);
  SetNull(&vb);
  SetNull(&vc);
  SetNull(&obj);

  /* test invalid object */
  JsonValue val0;
  InitValue(&val0);
  TEST_EQUAL(kJSON_PARSE_OBJECT_MISSING_KEY, ParseJson(&val0, "{,}"), "d");
  TEST_EQUAL(kJSON_PARSE_OBJECT_MISSING_KEY, ParseJson(&val0, "{,null}"), "d");
  TEST_EQUAL(kJSON_PARSE_OBJECT_MISSING_KEY, ParseJson(&val0, "{123}"), "d");
  TEST_EQUAL(kJSON_PARSE_OBJECT_MISSING_KEY, ParseJson(&val0, "{\"abc\":null, 123}"), "d");
  TEST_EQUAL(kJSON_PARSE_OBJECT_MISSING_KEY, ParseJson(&val0, "{\"abc\":null, :}"), "d");
  TEST_EQUAL(kJSON_PARSE_OBJECT_MISSING_COLON, ParseJson(&val0, "{\"abc\",}"), "d");
  TEST_EQUAL(kJSON_PARSE_OBJECT_INVALID_EXTRA_COMMA, ParseJson(&val0, "{\"abc\":null,}"), "d");
  TEST_EQUAL(kJSON_PARSE_OBJECT_MISSING_COMMA_OR_CURLY_BRACKET, ParseJson(&val0, "{\"abc\":null"), "d");
  TEST_EQUAL(kJSON_PARSE_OBJECT_MISSING_COMMA_OR_CURLY_BRACKET, ParseJson(&val0, "{\"abc\":null \"cde\"}"), "d");  
}

static void TestJsonStringifyImpl(const char* s, const char* func, int line) {
  JsonValue ans, res;
  InitValue(&ans);
  InitValue(&res);
  ParseJson(&ans, s);
  char* new_s = JsonToString(&ans, NULL);
  ParseJson(&res, new_s);
  TEST_EQUAL_CHECK("-", "-", "s", func, line, JsonCompare(&ans, &res));
  free(new_s);
  SetNull(&ans);
  SetNull(&res);
}

#define TEST_JSON_STRINGIFY(str) \
  TestJsonStringifyImpl(str, __func__, __LINE__)
static void TestJsonStringify() {
  TEST_JSON_STRINGIFY("null");
  TEST_JSON_STRINGIFY("false");
  TEST_JSON_STRINGIFY("true");
  TEST_JSON_STRINGIFY("123");
  TEST_JSON_STRINGIFY("\"abcdef\"");
  TEST_JSON_STRINGIFY("[null, false, true, \"abcdef\"]");
  TEST_JSON_STRINGIFY("{\"abc\" : 123, \"def\" : null}");
}

static void TestParse() {
  TestParseNull();
  TestParseFalse();
  TestParseTrue();
  TestParseNotSingular();
  TestParseNumber();
  TestSetterAndGetter();
  TestParseString();
  TestParseArray();
  TestParseObject();
  TestJsonStringify();
}

int main() {
  TestParse();
}

