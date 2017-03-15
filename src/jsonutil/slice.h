#ifndef JSONUTIL_SRC_SLICE_H__
#define JSONUTIL_SRC_SLICE_H__

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

namespace jsonutil {
class Slice {
 public:
  explicit Slice(const char* t = NULL, int l = 0) : text_(t), len_(l) {
    if (!(t == NULL && l == 0) && !(t && l >= 0)) {
      fprintf(stderr, "%s\n", "Slice constructor error: invalid arguments\n");
      abort();
    }
  }
 
  const char* Ptr() const { return text_; }
  int Len() const { return len_; }
  void Move(int step) {
    assert(step >= 0 && step <= len_);
    text_ += step;
    len_ -= step;
  }

 private:
  friend int Compare(const Slice& l, const Slice& r);
  const char* text_; /* Memory refered to by text_ is not owned by Slice. */
  int len_;
};

int Compare(const char* l, int llen, const char* r, int rlen);
int Compare(const Slice& l, const Slice& r);

} // namespace jsonutil
#endif // JSONUTIL_SRC_SLICE_H__
