# Tests guide

## Protocol parser tests

The parser has no ESP-IDF dependency and can be tested on the host:

```bash
g++ -std=c++17 -Wall -Wextra -Werror \
    -Icomponents/communication/protocol/include \
    components/communication/protocol/src/parser.cpp \
    tests/parser_test.cpp \
    -o tests/bin/parser_test
./tests/bin/parser_test
```

## Protocol dispatcher tests

The dispatcher has no ESP-IDF dependency and can be tested on the host:

```bash
g++ -std=c++17 -Wall -Wextra -Werror \
    -Icomponents/communication/protocol/include \
    components/communication/protocol/src/dispatcher.cpp \
    tests/dispatcher_test.cpp \
    -o tests/bin/dispatcher_test
./tests/bin/parser_test
```

## Test registry tests

```bash
g++ -std=c++17 \
    -Wall -Wextra -Werror \
    -Icomponents/testing/test_manager/include \
    components/testing/test_manager/src/test_registry.cpp \
    tests/test_registry_test.cpp \
    -o tests/bin/test_registry_test
./tests/bin/test_registry_test
```

## Test manager tests

```bash
g++ -std=c++17 \
    -Wall -Wextra -Werror \
    -Icomponents/testing/test_manager/include \
    components/testing/test_manager/src/test_registry.cpp \
    components/testing/test_manager/src/test_manager.cpp \
    tests/test_manager_test.cpp \
    -o tests/bin/test_manager_test

./tests/bin/test_manager_test
```
