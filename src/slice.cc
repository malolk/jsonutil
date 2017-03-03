#include "slice.h"

namespace jsonutil {
int Compare(const char* lhs, int llen, const char* rhs, int rlen) {
  assert(lhs && rhs);
  int min_len = llen < rlen ? llen : rlen;
  for (int i = 0; i < min_len; ++i) {
    if (lhs[i] != rhs[i]) {
      return (lhs[i] < rhs[i]) ? -1 : 1;
    }
  }
  if (llen == rlen) {
    return 0;
  } else {
    return llen < rlen ? -1 : 1;
  }
}

int Compare(const Slice& l, const Slice& r) {
  return Compare(l.Ptr(), l.Len(), r.Ptr(), r.Len());
}
} // namespace jsonutil
