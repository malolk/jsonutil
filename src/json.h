#ifndef JSONUTIL_SRC_JSON_H__
#define JSONUTIL_SRC_JSON_H__

#include "stack.h"
#include "slice.h"

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

  void Reset();
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
} // namespace jsonutil
#endif // JSONUTIL_SRC_JSON_H__
