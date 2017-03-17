#ifndef JSONUTIL_SRC_JSON_STATUS_H__
#define JSONUTIL_SRC_JSON_STATUS_H__

#include <assert.h>

#include <string>

namespace jsonutil {

class JsonStatus {
 public:
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

  JsonStatus(Status s = kJSON_OK): status_(s) { 
    assert(s >= kJSON_OK && s <= kJSON_OUT_OF_MEMORY);
  }
 
  bool operator==(const JsonStatus& rhs) { return status_ == rhs.status_; }
  bool operator!=(const JsonStatus& rhs) { return status_ != rhs.status_; }
  bool Ok() { return status_ == kJSON_OK; }
  std::string ToString();

  /* For testing. */
  Status Code() { return status_; }
 private:
  Status status_;
};

} // namespace jsonutil

#endif // JSONUTIL_SRC_JSON_STATUS_H__

