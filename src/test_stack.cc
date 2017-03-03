#include "stack.h"

#include <iostream>

using namespace jsonutil;
using namespace std;

int main() {
    Stack s;
    s.PushString("abc", 4);
    cout << (s.Top() == 4) << endl;
    cout << s.Dump() << endl;
}
