#include "stack.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

namespace jsonutil {
char* Stack::Pop(int size) {
  assert(top_ >= size && size >= 0);
  top_ -= size;
  return (stk_ + top_);
}

char* Stack::Push(int size) {
  assert(size >= 0);
  int need = top_ + size;
  if (need > size_) {
    if (stk_ == NULL) {
      size_ = JSONUTIL_STACK_INIT_SIZE;
    }
    while (need > size_) {
      size_ += size_ >> 1;
    }
    stk_ = static_cast<char*>(realloc(stk_, size_));
    memset(stk_ + top_, 0, size_ - top_);
  }
  char* ret = stk_ + top_;
  top_ += size;
  return ret;
}

void Stack::Prepare(int size) {
  assert(size >= 0);
  if (size < size_ - top_) return;
  Push(size);
  Pop(size);
}

void Stack::PushUint32(uint32_t u, int bytes) {
  assert(bytes >= 0 && bytes <= 4);
  char* dst = Push(bytes);
  while (--bytes >= 0) {
    *dst++ = static_cast<char>((u >> (8 * bytes)) & 0xFF);  // big endian
  }
}

void Stack::PushHex(uint16_t u) {
  char buf[] = "\\u0000";
  sprintf(buf, "\\u%04X", u);
  memcpy(Push(6), buf, 6);
}

void Stack::PushString(const char* s, int len) {
  assert(s != NULL);
  memcpy(Push(len), s, len);
}

void Stack::Free() {
  if (stk_) {
    free(stk_);
    stk_ = NULL;
    top_ = size_ = 0;
  }
}  
} // namespace jsonutil
