#include "json_status.h"

namespace jsonutil {

namespace {
const char* status_string[] = { 
  "Json OK",                                           // kJSON_OK = 0,
  "Json parse expect value",                           // kJSON_PARSE_EXPECT_VALUE,
  "Json parse invalid value",                          // kJSON_PARSE_INVALID_VALUE,
  "Json parse root not singular",                      // kJSON_PARSE_ROOT_NOT_SINGULAR,
  "Json parse number overflow",                        // kJSON_PARSE_NUMBER_OVERFLOW,
  "Json parse number underflow",                       // kJSON_PARSE_NUMBER_UNDERFLOW,
  "Json parse string no end mark",                     // kJSON_PARSE_STRING_NO_END_MARK,
  "Json parse string escaped invalid char",            // kJSON_PARSE_STRING_ESCAPED_INVALID_CHAR,
  "Json parse string invalid char",                    // kJSON_PARSE_STRING_INVALID_CHAR,
  "Json parse string unicode invalid hex",             // kJSON_PARSE_STRING_UNICODE_INVALID_HEX,
  "Json parse string unicode invalid surrogate",       // kJSON_PARSE_STRING_UNICODE_INVALID_SURROGATE,
  "Json parse string unicode invalid range",           // kJSON_PARSE_STRING_UNICODE_INVALID_RANGE,
  "Json parse array invalid extra comma",              // kJSON_PARSE_ARRAY_INVALID_EXTRA_COMMA,
  "Json parse array missing comma",                    // kJSON_PARSE_ARRAY_MISSING_COMMA,
  "Json parse object missing key",                     // kJSON_PARSE_OBJECT_MISSING_KEY,
  "Json parse object missing colon",                   // kJSON_PARSE_OBJECT_MISSING_COLON,
  "Json parse object invalid extra comma",             // kJSON_PARSE_OBJECT_INVALID_EXTRA_COMMA,
  "Json parse object missing comma or curly bracket",  // kJSON_PARSE_OBJECT_MISSING_COMMA_OR_CURLY_BRACKET,
  "Json out of memory"                                 // kJSON_OUT_OF_MEMORY
};
}

std::string JsonStatus::ToString() {
  return status_string[static_cast<int>(status_)];
}
} // namespace jsonutil
