# Tests guide

## Protocol parser tests

The parser has no ESP-IDF dependency and can be tested on the host:

```bash
g++ -std=c++17 -Wall -Wextra -Werror \
    -Icomponents/communication/protocol/include \
    components/communication/protocol/src/parser.cpp \
    tests/parser_test.cpp \
    -o tests/bin/parser_test
./parser_test
```

## Protocol dispatcher tests

The dispatcher has no ESP-IDF dependency and can be tested on the host:

```bash
g++ -std=c++17 -Wall -Wextra -Werror \
    -Icomponents/communication/protocol/include \
    components/communication/protocol/src/dispatcher.cpp \
    tests/dispatcher_test.cpp \
    -o tests/bin/dispatcher_test
./parser_test
```
