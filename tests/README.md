# Protocol parser tests

The parser has no ESP-IDF dependency and can be tested on the host:

```bash
g++ -std=c++17 -Wall -Wextra -Werror \
    -Icomponents/communication/protocol/include \
    components/communication/protocol/src/parser.cpp \
    tests/parser_test.cpp \
    -o parser_test
./parser_test
```
