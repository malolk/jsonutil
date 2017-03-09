#ifndef JSONUTIL_SRC_JSON_H__
#define JSONUTIL_SRC_JSON_H__

#include "stack.h"
#include "slice.h"

#include <string>
#include <vector>
#include <map>
#include <string.h>

namespace jsonutil {
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

typedef enum {
  kJSON_NULL, 
  kJSON_FALSE, 
  kJSON_TRUE, 
  kJSON_NUMBER, 
  kJSON_STRING, 
  kJSON_ARRAY, 
  kJSON_OBJECT
} ValueType;

class Member;
template <class T>
class Builder;

class Value {
 public:
  Value(): type_(kJSON_NULL), val_({{NULL, 0}}) {
  }
  explicit Value(ValueType t): type_(t), val_({{NULL, 0}}) {
  }

  Value(const Value& rhs);
  const Value& operator=(const Value&);
  ~Value();

  Status Parse(const char* text, int len);

  bool GetBoolean() const;
  void SetBoolean(bool b);

  double GetNumber() const;
  void SetNumber(double num);

  char* GetString();
  const char* GetString() const;
  int GetStringLength() const;
  void SetString(const char* s, int len);

  int GetArraySize() const;
  Value* GetArrayValue(int index);
  const Value* GetArrayValue(int index) const;
  void SetArray(const Value* src);
  void SetArrayValue(int index, Value* src);
  void MergeArrayBuilder(Builder<Value>& b);

  int GetObjectSize() const;
  Member* GetObjectMember(int index);
  const Member* GetObjectMember(int index) const;
  Value* GetValueByKey(const char* k, int len);
  const Value* GetValueByKey(const char* k, int len) const;
  Member* GetMemberByKey(const char* k, int len);
  const Member* GetMemberByKey(const char* k, int len) const;
  void SetObject(const Value* src);
  bool SetObjectKeyValue(const char* k, int len, Value* v);
  void MergeObjectBuilder(Builder<Member>& b);

  void Reset(ValueType t = kJSON_NULL);
  ValueType Type() const { return type_; }
  /* Returned string should be freed by caller. */ 
  /* Returned string is null-terminated. */
  char* ToString(int* len = NULL);
  
 private:
  void Free();
  void FreeOnError(Stack& s);
  Status ParseObject(Stack& stk, Slice& s);
  Status ParseValue(Stack& stk, Slice& s);
  Status ParseLiteral(Slice& s, char c);
  Status ParseString(Stack& stk, Slice& s);
  Status ParseArray(Stack& stk, Slice& s);
  Status ParseNumber(Slice& s);
  union {
    struct {
      Member* m;
      int size;
    } o; // object
    struct {
      Value* a;
      int size;
    } a; // array
    struct {
      char* s;
      int len;
    } s; // string
    double num; // number
  } val_;
  ValueType type_;
};

bool Compare(const Value* lhs, const Value* rhs);

class Member {
 public:
  Member() : k_(NULL), len_(0), v_(NULL) {
  }

  Member(const Member& rhs);
  const Member& operator=(const Member& rhs);
  ~Member();

  char* Key() { return k_; }
  const char* Key() const { return k_; }
  int KLen() const { return len_; }
  Value* Val() { return v_; }
  const Value* Val() const { return v_; }
  void Set(const char* k, int len, const Value* v);
  /* Move the ownership of key-value to this object. */
  void Move(const char* k, int len, const Value* v);
  void Free();
 private:
  char* k_;
  int len_;
  Value* v_;
};

template <class T>
class Builder {
 public:
  Builder() {
  }
  void Add(T& elem) {
    T* p = reinterpret_cast<T*>(stk_.Push(sizeof(T)));
    memset(p, 0, sizeof(T));
    *p = elem;
  }
  T* Dump(int& num) {
    assert(stk_.Top() % sizeof(T) == 0);
    num = stk_.Top() / sizeof(T);
    return reinterpret_cast<T*>(stk_.Pop(stk_.Top()));
  }
  /* Default copy constructor, assign-operator and destructor is ok. */
 private:
  Stack stk_;
};

Value& Serialize(Value& v, double num);
Value& Serialize(Value& v, const std::string& s);
void DeSerialize(const Value& v, double& num);
void DeSerialize(const Value& v, std::string& num);
template <typename T>
Value& Serialize(Value& v, const T& data);
template <typename T>
void DeSerialize(const Value& v, T& data);
template <typename T>
Value& Serialize(Value& v, const std::vector<T>& a);
template <typename T>
Value& Serialize(Value& v, const std::map<std::string, T>& m);
template <typename T>
void DeSerialize(const Value& v, std::vector<T>& a);
template <typename T>
void DeSerialize(const Value& v, std::map<std::string, T>& m);

template <typename T>
Value& Serialize(Value& v, const T& data) {
  return Serialize(v, data);
}

template <typename T>
void DeSerialize(const Value& v, T& data) {
  DeSerialize(v, data);
}

template <typename T>
Value& Serialize(Value& v, const std::vector<T>& a) {
  v.Reset(kJSON_ARRAY);
  int size = a.size();
  if (size == 0) return v;
  Builder<Value> batch;
  for (int i = 0; i < size; ++i) {
    Value elem;
    Serialize<T>(elem, a[i]);
    batch.Add(elem);
  }
  v.MergeArrayBuilder(batch);
  return v;
}

template <typename T>
Value& Serialize(Value& v, const std::map<std::string, T>& m) {
  v.Reset(kJSON_OBJECT);
  int size = m.size();
  if (size == 0) return v;
  Builder<Member> batch;
  for (typename std::map<std::string, T>::const_iterator 
    it = m.cbegin(); it != m.cend(); ++it) {
    Member elem;
    Value val;
    Serialize<T>(val, it->second);
    elem.Set((it->first).c_str(), (it->first).size(), &val);
    batch.Add(elem);
  }
  v.MergeObjectBuilder(batch);
  return v;
}

template <typename T>
void DeSerialize(const Value& v, std::vector<T>& a) {
  assert(v.Type() == kJSON_ARRAY);
  int size = v.GetArraySize();
  a.reserve(a.size() + size);
  for (int i = 0; i < size; ++i) {
    const Value* p = v.GetArrayValue(i);
    T elem;
    DeSerialize<T>(*p, elem);
    a.push_back(elem);
  }
}

template <typename T>
void DeSerialize(const Value& v, std::map<std::string, T>& m) {
  assert(v.Type() == kJSON_OBJECT);
  int size = v.GetObjectSize();
  for (int i = 0; i < size; ++i) {
    const Member* p = v.GetObjectMember(i);
    const char* k = p->Key();
    int len = p->KLen();
    T elem;
    DeSerialize<T>(*(p->Val()), elem);
    m[std::string(k, len)] = elem;
  }
}

} // namespace jsonutil
#endif // JSONUTIL_SRC_JSON_H__
