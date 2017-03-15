#ifndef JSONUTIL_SRC_STACK_H__
#define JSONUTIL_SRC_STACK_H__

#include <stdint.h>
#include <stddef.h>

#ifndef JSONUTIL_STACK_INIT_SIZE
  #define JSONUTIL_STACK_INIT_SIZE 256
#endif

namespace jsonutil {
class Stack {
 public:
  Stack() : stk_(NULL), top_(0), size_(0) {
  }

  ~Stack() {
    Free();
  }
  char* Pop(int size);
  char* Push(int size);
  char* Dump() { return Pop(top_); }
  void Prepare(int size);
  void PushUint32(uint32_t u, int bytes);
  void PushHex(uint16_t u);
  void PushString(const char* s, int len);
  int Top() const { return top_; }
  int Size() const { return size_; }
  void Free();

 private:
  /* Stack is noncopyable. */
  Stack(const Stack&);
  const Stack& operator=(const Stack&);

  char* stk_;
  int top_;
  int size_;
};

} // namespace jsonutil
#endif // JSONUTIL_SRC_CONTEXT_H__
