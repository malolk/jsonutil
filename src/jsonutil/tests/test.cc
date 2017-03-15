#include "jsonutil/json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>
#include <sstream>
#include <string>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_RESET   "\x1b[0m"

using namespace jsonutil;
using namespace std;

#define TEST_EQUAL_CHECK_HELPLER(answer, result, fmt, func_name, line, flag, color) \
  do {                                                                              \
    std::ostringstream out_ans, out_res;                                            \
    out_ans << answer;                                                              \
    out_res << result;                                                              \
    std::string as = out_ans.str();                                                 \
    std::string rs = out_res.str();                                                 \
    std::string str = flag ? "[PASSED]" : "[FAILED]";                               \
    fprintf(stdout, "%20s:%-10d Answer: %-20s"  " Result: %-20s"                    \
            color " %10s" ANSI_COLOR_RESET "\n",                                    \
            func_name, line, as.c_str(), rs.c_str(), str.c_str());                  \
  } while (0)
  
#define TEST_EQUAL_CHECK(answer, result, fmt, func_name, line, check)              \
  do {                                                                             \
    if (!check) {                                                                  \
      TEST_EQUAL_CHECK_HELPLER(                                                    \
        answer, result, fmt, func_name, line, false, ANSI_COLOR_RED);              \
    } else {                                                                       \
      TEST_EQUAL_CHECK_HELPLER(                                                    \
        answer, result, fmt, func_name, line, true, ANSI_COLOR_GREEN);             \
    }                                                                              \
  } while (0)                                                            

#define TEST_EQUAL(answer, result, fmt) \
  TEST_EQUAL_CHECK(answer, result, fmt, __func__, __LINE__, (answer == result))

#define TEST_EQUAL_INT(answer, result, fmt) \
  TEST_EQUAL_CHECK(answer, result, fmt, __func__, __LINE__, \
    (static_cast<int>(answer) == static_cast<int>(result)))

void TestParseNull() {
  Value val;
  val.SetBoolean(true);
  TEST_EQUAL_INT(kJSON_OK, val.Parse("null", 4), "f");
  TEST_EQUAL_INT(kJSON_NULL, val.Type(), "f");
}

void TestParseFalse() {
  Value val;
  TEST_EQUAL_INT(kJSON_OK, val.Parse("false", 5), "f");
  TEST_EQUAL_INT(kJSON_FALSE, val.Type(), "f");
}

void TestParseTrue() {
  Value val;
  TEST_EQUAL_INT(kJSON_OK, val.Parse("true", 4), "f");
  TEST_EQUAL_INT(kJSON_TRUE, val.Type(), "f");
}

void TestParseNotSingular() {
  Value val;
  TEST_EQUAL_INT(kJSON_PARSE_ROOT_NOT_SINGULAR, val.Parse("null extra-bytes", 16), "f");  
  TEST_EQUAL_INT(kJSON_PARSE_ROOT_NOT_SINGULAR, val.Parse("false extra-bytes", 17), "f");
  TEST_EQUAL_INT(kJSON_PARSE_ROOT_NOT_SINGULAR, val.Parse("true extra-bytes", 16), "f");
  TEST_EQUAL_INT(kJSON_PARSE_ROOT_NOT_SINGULAR, val.Parse("1.23 extra-bytes", 16), "f");
}

void TestParseValidNumberImpl(double num, const char* num_str, int len,  
                              const char* func, int line) {
  Value val;
  Status s = val.Parse(num_str, len);
  TEST_EQUAL_CHECK(kJSON_OK, s, "f", func, line, (s == kJSON_OK));
  double res = val.GetNumber();
  TEST_EQUAL_CHECK(num, res, "f", func, line, (num == res));
}

#define TestParseValidNumber(num, num_str) \
    TestParseValidNumberImpl(num, num_str, static_cast<int>(strlen(num_str)), __func__, __LINE__)

void TestParseInvalidNumberImpl(const char* num_str, int len, 
                              const char* func, int line) {
  Value val;
  Status s = val.Parse(num_str, len);
  TEST_EQUAL_CHECK(kJSON_PARSE_INVALID_VALUE, s, "f", func, line, (s == kJSON_PARSE_INVALID_VALUE));
}

#define TestParseInvalidNumber(num_str) \
  TestParseInvalidNumberImpl(num_str, static_cast<int>(strlen(num_str)), __func__, __LINE__)

void TestParseNumber() {
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
  Value val;
  TEST_EQUAL_INT(kJSON_PARSE_NUMBER_OVERFLOW, val.Parse("1e+10000", 8), "f");
  val.Reset();
  TEST_EQUAL_INT(kJSON_PARSE_NUMBER_OVERFLOW, val.Parse("-1e+10000", 9), "f");
  val.Reset();
  TEST_EQUAL_INT(kJSON_PARSE_NUMBER_UNDERFLOW, val.Parse("1e-10000", 8), "f");
}

#define TEST_EQUAL_STRING(ans, len1, res, len2) \
  TEST_EQUAL_CHECK(ans, res, "s", __func__, __LINE__, !Compare(ans, len1, res, len2))

void TestSetterAndGetter() {
  Value val;
  
  TEST_EQUAL(0.0, (val.SetNumber(0.0), val.GetNumber()), "f");
  TEST_EQUAL(1.0, (val.SetNumber(1.0), val.GetNumber()), "f");
  TEST_EQUAL(-1.2, (val.SetNumber(-1.2), val.GetNumber()), "f");
  TEST_EQUAL_INT(kJSON_NUMBER, val.Type(), "f");

  TEST_EQUAL(true, (val.SetBoolean(true), val.GetBoolean()), "f");
  TEST_EQUAL_INT(kJSON_TRUE, val.Type(), "f");
  TEST_EQUAL(false, (val.SetBoolean(false), val.GetBoolean()), "f");
  TEST_EQUAL_INT(kJSON_FALSE, val.Type(), "f");

  TEST_EQUAL_STRING("Wonderful", 9, (val.SetString("Wonderful", 9), val.GetString()), 9);
  TEST_EQUAL_INT(9, val.GetStringLength(), "f");
  TEST_EQUAL_STRING("Tonight", 7, (val.SetString("Tonight", 7), val.GetString()), 7);
  TEST_EQUAL_INT(7, val.GetStringLength(), "f");
  TEST_EQUAL_INT(kJSON_STRING, val.Type(), "f");
}

void TestParseStringValidImpl(const char* ans, const char* text,
                                     int t_len, int len, const char* func, int line) {
  Value val;
  
  Status s = val.Parse(text, t_len);
  TEST_EQUAL_CHECK(kJSON_OK, s, "f", func, line, (kJSON_OK == s));
  int get_len = val.GetStringLength();
  TEST_EQUAL_CHECK(len, get_len, "f", func, line, (len == get_len));
  const char* get_str = val.GetString();
  TEST_EQUAL_CHECK(ans, get_str, "s", func, line, (!Compare(ans, len, get_str, get_len)));
}

#define TestParseStringValid(ans, text, len) \
  TestParseStringValidImpl(ans, text, static_cast<int>(strlen(text)), len, __func__, __LINE__)

void TestParseStringInvalidImpl(jsonutil::Status error, const char* text, int t_len, int len, const char* func, int line) {
  Value val;
  Status s = val.Parse(text, t_len);
  TEST_EQUAL_CHECK(error, s, "f", func, line, (error == s));
}

#define TestParseStringInvalid(error, text, len) \
  TestParseStringInvalidImpl(error, text, static_cast<int>(strlen(text)), len, __func__, __LINE__)
void TestParseString() {
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

void TestParseValueValidImpl(Value* res, Value* ans, const char* func, int line) {
  bool flag = Compare(res, ans);
  TEST_EQUAL_CHECK("-", "-", "s", func, line, flag);
}

#define TestParseValueValid(array, ans) \
  TestParseValueValidImpl(array, ans, __func__, __LINE__)

void TestParseArray() {
  Value val, chk_val, str_val;
  str_val.SetString("abc", 3);
  const char* text0 = "[null, true, false, \"abc\"]";
  Status s = val.Parse(text0, static_cast<int>(strlen(text0)));
  if (s != kJSON_OK) abort();
  TEST_EQUAL_INT(4, val.GetArraySize(), "f");
  TestParseValueValid(val.GetArrayValue(0), &chk_val);
  TestParseValueValid(val.GetArrayValue(1), (chk_val.SetBoolean(true), &chk_val));
  TestParseValueValid(val.GetArrayValue(2), (chk_val.SetBoolean(false), &chk_val));
  TestParseValueValid(val.GetArrayValue(3), &str_val);
  chk_val.Reset();
  val.Reset();

  Value ary0, ary1;
  
  if ((s = ary0.Parse("[]", 2)) != kJSON_OK) abort();
  TEST_EQUAL_INT(kJSON_ARRAY, ary0.Type(), "f");
  TEST_EQUAL_INT(0, ary0.GetArraySize(), "f");

  if ((s = ary1.Parse("[1, 2, 3]", 9)) != kJSON_OK) abort();
  TEST_EQUAL_INT(kJSON_ARRAY, ary0.Type(), "f");
  TEST_EQUAL_INT(3, ary1.GetArraySize(), "f");
  TestParseValueValid(ary1.GetArrayValue(0), (chk_val.SetNumber(1), &chk_val));
  TestParseValueValid(ary1.GetArrayValue(1), (chk_val.SetNumber(2), &chk_val));
  TestParseValueValid(ary1.GetArrayValue(2), (chk_val.SetNumber(3), &chk_val));
  chk_val.Reset();

  const char* text1 = "[[], [1, 2, 3], \"abc\"]";
  if ((s = val.Parse(text1, static_cast<int>(strlen(text1)))) != kJSON_OK) abort();
  TEST_EQUAL_INT(kJSON_ARRAY, val.Type(), "f");
  TEST_EQUAL_INT(3, val.GetArraySize(), "f");
  TestParseValueValid(val.GetArrayValue(0), &ary0);
  Value* res = val.GetArrayValue(1);
  TEST_EQUAL_INT(kJSON_ARRAY, res->Type(), "f");
  TEST_EQUAL_INT(kJSON_NUMBER, (res->GetArrayValue(0))->Type(), "f");
  
  TestParseValueValid(val.GetArrayValue(1), &ary1);
  TestParseValueValid(val.GetArrayValue(2), &str_val);
  val.SetArrayValue(1, &str_val);
  TestParseValueValid(val.GetArrayValue(1), val.GetArrayValue(2));

  Builder<Value> store;
  store << ary1;
  store << str_val;
  val.MergeArrayBuilder(store);
  TEST_EQUAL_INT(5, val.GetArraySize(), "f");
  TestParseValueValid(val.GetArrayValue(3), &ary1);
  TestParseValueValid(val.GetArrayValue(4), &str_val);

  ary0.Reset();
  val.Reset();
  ary1.Reset();
  str_val.Reset();

  Value val0;
  
  TEST_EQUAL_INT(kJSON_PARSE_ARRAY_INVALID_EXTRA_COMMA, val0.Parse("[1,]", 4), "f");
  TEST_EQUAL_INT(kJSON_PARSE_ARRAY_MISSING_COMMA, val0.Parse("[1 1]", 5), "f");
}

Status ParseImpl(Value& val, const char* text) {
  int len = static_cast<int>(strlen(text));
  return val.Parse(text, len);
}

void TestParseObject() {
  Value obj, va, vb, vc;
  
  const char* text0 = "{\"c\":null, \"b\": [0], \"a\": \"abc\"}";
  int text0_len = static_cast<int>(strlen(text0));
  Status s = obj.Parse(text0, text0_len);
  TEST_EQUAL_INT(kJSON_OK, s, "f");
  if (kJSON_OK != s) abort();
  TEST_EQUAL_INT(kJSON_OBJECT, obj.Type(), "f");
  TEST_EQUAL_INT(3, obj.GetObjectSize(), "f");
  va.Parse("\"abc\"", 5);
  vb.Parse("[0]", 3);
  vc.Parse("null", 4);
  Member* mem0 = obj.GetObjectMember(0);
  Member* mem1 = obj.GetObjectMember(1);
  Member* mem2 = obj.GetObjectMember(2);
  TEST_EQUAL_STRING("a", 1, mem0->Key(), mem0->KLen());
  TEST_EQUAL_STRING("b", 1, mem1->Key(), mem1->KLen());
  TEST_EQUAL_STRING("c", 1, mem2->Key(), mem2->KLen());
  TestParseValueValid(&va, mem0->Val());
  TestParseValueValid(&va, obj.GetValueByKey("a", 1));
  TestParseValueValid(&vb, mem1->Val());
  TestParseValueValid(&vb, obj.GetValueByKey("b", 1));
  TestParseValueValid(&vc, mem2->Val());
  TestParseValueValid(&vc, obj.GetValueByKey("c", 1));

  Builder<Member> store;
  Member b0, b1;
  b0.Set("aa", 2, &va);
  b1.Set("ab", 2, &vb);
  store << b0;
  store << b1;
  obj.MergeObjectBuilder(store);
  TEST_EQUAL_INT(5, obj.GetObjectSize(), "f");
  mem1 = obj.GetObjectMember(1);
  mem2 = obj.GetObjectMember(2);
  TEST_EQUAL_STRING("aa", 2, mem1->Key(), mem1->KLen());
  TEST_EQUAL_STRING("ab", 2, mem2->Key(), mem2->KLen());
  TestParseValueValid(&va, mem1->Val());
  TestParseValueValid(&vb, mem2->Val());
  TestParseValueValid(&va, obj.GetValueByKey("aa", 2));
  TestParseValueValid(&vb, obj.GetValueByKey("ab", 2));

  Value cp(obj); 
  TestParseValueValid(&cp, &obj);
  cp.Reset();

  /* test recursive Object-type value */
  Value robj;
  const char* text1 = "{\"abc\" : {\"c\":null, \"aa\":\"abc\", \"ab\":[0], \"b\":[0], \"a\":\"abc\"}}";
  s = robj.Parse(text1, static_cast<int>(strlen(text1)));
  TEST_EQUAL_INT(kJSON_OK, s, "f");
  if (kJSON_OK != s) abort();
  TEST_EQUAL_INT(kJSON_OBJECT, robj.Type(), "f");
  TEST_EQUAL_INT(1, robj.GetObjectSize(), "f");
  Member* mem3 = robj.GetObjectMember(0);
  TEST_EQUAL_STRING("abc", 3, mem3->Key(), mem3->KLen());
  TestParseValueValid(&obj, mem3->Val());
  robj.Reset();

  va.Reset();
  vb.Reset();
  vc.Reset();
  obj.Reset();
  /* test invalid object */
  
  Value val0;
  TEST_EQUAL_INT(kJSON_PARSE_OBJECT_MISSING_KEY, ParseImpl(val0, "{,}"), "f");
  TEST_EQUAL_INT(kJSON_PARSE_OBJECT_MISSING_KEY, ParseImpl(val0, "{,null}"), "f");
  TEST_EQUAL_INT(kJSON_PARSE_OBJECT_MISSING_KEY, ParseImpl(val0, "{123}"), "f");
  TEST_EQUAL_INT(kJSON_PARSE_OBJECT_MISSING_KEY, ParseImpl(val0, "{\"abc\":null, 123}"), "f");
  TEST_EQUAL_INT(kJSON_PARSE_OBJECT_MISSING_KEY, ParseImpl(val0, "{\"abc\":null, :}"), "f");
  TEST_EQUAL_INT(kJSON_PARSE_OBJECT_MISSING_COLON, ParseImpl(val0, "{\"abc\",}"), "f");
  TEST_EQUAL_INT(kJSON_PARSE_OBJECT_INVALID_EXTRA_COMMA, ParseImpl(val0, "{\"abc\":null,}"), "f");
  TEST_EQUAL_INT(kJSON_PARSE_OBJECT_MISSING_COMMA_OR_CURLY_BRACKET, ParseImpl(val0, "{\"abc\":null"), "f");
  TEST_EQUAL_INT(kJSON_PARSE_OBJECT_MISSING_COMMA_OR_CURLY_BRACKET, ParseImpl(val0, "{\"abc\":null \"cde\"}"), "f");  
}

void TestJsonStringifyImpl(const char* s, const char* func, int line) {
  Value ans, res;
  ans.Parse(s, static_cast<int>(strlen(s)));
  std::string new_s = ans.ToString();

  TEST_EQUAL_CHECK(std::string(s), new_s, "s", func, line, new_s.compare(s));
  ParseImpl(res, new_s.c_str());
  TEST_EQUAL_CHECK("-", "-", "s", func, line, Compare(&ans, &res));
  ans.Reset();
  res.Reset();
}

#define TEST_JSON_STRINGIFY(str) \
  TestJsonStringifyImpl(str, __func__, __LINE__)
void TestJsonStringify() {
  TEST_JSON_STRINGIFY("null");
  TEST_JSON_STRINGIFY("false");
  TEST_JSON_STRINGIFY("true");
  TEST_JSON_STRINGIFY("123");
  TEST_JSON_STRINGIFY("\"abcdef\"");
  TEST_JSON_STRINGIFY("[null, false, true, \"abcdef\"]");
  TEST_JSON_STRINGIFY("{\"abc\" : 123, \"def\" : null}");
}

bool CompareElem(const string& s, const Value* v) {
  return s.compare(string(v->GetString(), v->GetStringLength())) == 0;
}

bool CompareElem(double num, const Value* v) {
  return num == v->GetNumber();
}

#define TestSerializeVectorAux(v, vec, func, line)       \
  do {                                                   \
    TEST_EQUAL_INT(kJSON_ARRAY, v.Type(), "f");          \
    TEST_EQUAL_INT(vec.size(), v.GetArraySize(), "f");   \
    int size = static_cast<int>(vec.size());             \
    for (int i = 0; i < size; ++i) {                     \
      Value* p = v.GetArrayValue(i);                     \
      TEST_EQUAL_CHECK("-", "-", "s",                    \
        func, line, CompareElem(vec[i], p));             \
    }                                                    \
  } while (0)

void TestSerializeVectorImpl(vector<double>& vec, const char* func, int line) {
  Value v;
  v << vec;
  TestSerializeVectorAux(v, vec, func, line);
  vector<double> ret;
  v >> ret;
  TestSerializeVectorAux(v, ret, func, line);
}

void TestSerializeVectorImpl(vector<string>& vec, const char* func, int line) {
  Value v;
  v << vec;
  TestSerializeVectorAux(v, vec, func, line);
  vector<string> ret;
  v >> ret;
  TestSerializeVectorAux(v, ret, func, line);
}

#define TestSerializeMapAux(v, m, func, line)           \
  do {                                                  \
    TEST_EQUAL_INT(kJSON_OBJECT, v.Type(), "f");            \
    TEST_EQUAL_INT(m.size(), v.GetObjectSize(), "f");       \
    int size = static_cast<int>(m.size());                \
    for (int i = 0; i < size; ++i) {                \
      const Member* p = v.GetObjectMember(i);           \
      string key = string(p->Key(), p->KLen());          \
      TEST_EQUAL_CHECK("-", "-", "s", func, line,       \
        m.count(key) && CompareElem(m[key], p->Val())); \
    }                                                   \
  } while (0)

void TestSerializeMapImpl(map<string, double>& m, const char* func, int line) {
  Value v;
  v << m;
  TestSerializeMapAux(v, m, func, line);
  map<string, double> ret;
  v >> ret;
  TestSerializeMapAux(v, ret, func, line);
}

void TestSerializeMapImpl(map<string, string>& m, const char* func, int line) {
  Value v;
  v << m;
  TestSerializeMapAux(v, m, func, line);
  map<string, string> ret;
  v >> ret;
  TestSerializeMapAux(v, ret, func, line);
}

void TestSerializeBuiltinImpl(double num, const char* func, int line) {
  Value v;
  v << num;
  TEST_EQUAL_CHECK("-", "-", "s", func, line, CompareElem(num, &v));
  double ret = 0;
  v >> ret;
  TEST_EQUAL_CHECK("-", "-", "s", func, line, ret == num);
}

void TestSerializeBuiltinImpl(const string& data, const char* func, int line) {
  Value v;
  v << data;
  TEST_EQUAL_CHECK("-", "-", "s", func, line, CompareElem(data, &v));
  string ret;
  v >> ret;
  TEST_EQUAL_CHECK("-", "-", "s", func, line, ret.compare(data) == 0);
}

void TestSerializeBuilderChainingImpl(const char* func, int line) {
  Value store(kJSON_ARRAY);
  TEST_EQUAL_INT(kJSON_ARRAY, store.Type(), "f");
  Builder<Value> batch;
  std::string s = "123";
  batch << 1 << 2 << 3 << s << 4;
  store.MergeArrayBuilder(batch);
  std::cout << store.ToString() << std::endl;
  TEST_EQUAL_INT(kJSON_ARRAY, store.Type(), "f");
  TEST_EQUAL_INT(5, store.GetArraySize(), "f");
  TEST_EQUAL_CHECK("-", "-", "s", func, line, (store.GetArrayValue(0))->GetNumber() == 1);
  TEST_EQUAL_CHECK("-", "-", "s", func, line, (store.GetArrayValue(1))->GetNumber() == 2);
  TEST_EQUAL_CHECK("-", "-", "s", func, line, (store.GetArrayValue(2))->GetNumber() == 3);
  TEST_EQUAL_CHECK("-", "-", "s", func, line, s.compare((store.GetArrayValue(3))->GetString()) == 0);
  TEST_EQUAL_CHECK("-", "-", "s", func, line, (store.GetArrayValue(4))->GetNumber() == 4);
}

#define TestSerializeVector(vec)                    \
  TestSerializeVectorImpl(vec, __func__, __LINE__)
#define TestSerializeMap(m)                         \
  TestSerializeMapImpl(m, __func__, __LINE__)
#define TestSerializeBuiltin(data)                  \
  TestSerializeBuiltinImpl(data, __func__, __LINE__)
#define TestSerializeBuilderChaining()              \
  TestSerializeBuilderChainingImpl(__func__, __LINE__)

void TestSerialize() {
#if 0
  TestSerializeBuiltin(10.0);
  TestSerializeBuiltin(string(""));
  TestSerializeBuiltin(string("deadbeef"));
  vector<double> iv = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  vector<string> sv = {"", " ", "abc", "def", "ghi"};
  TestSerializeVector(iv); 
  TestSerializeVector(sv); 

  map<string, double> sdm = {
    {"", 0}, {" ", 1}, {"abc", 2}, {"def", 3}, {"ghi", 4}
  };
  TestSerializeMap(sdm);
  map<string, string> ssm = {
    {"", "0"}, {" ", "1"}, {"abc", "2"}, {"def", "3"}, {"ghi", "4"}
  };
  TestSerializeMap(ssm);
#endif
  TestSerializeBuilderChaining();
}

void Test() {
#if 0
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
#endif
  TestSerialize();
}

int main() {
  Test();
}

