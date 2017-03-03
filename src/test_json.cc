#include "json.h"

#include <iostream>

using namespace jsonutil;
using namespace std;

int main() {
    Value v;
    cout << v.Type() << endl;
    
    Member m;
    m.Set("abc", 4, &v);
    cout << m.Key() << endl;

}
