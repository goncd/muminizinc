# MuMiniZinc
MuMiniZinc is a mutation test tool for [MiniZinc](https://www.minizinc.org/).

## Building

### Requirements
- A C++23 compiler, like GCC, Clang or MSVC.
- CMake 3.30 or higher.
- Boost 1.88 or higher.
- nlohmann_json 3.12.0 or higher.

### Optional requirements
- For static analysis and formatting: clang-tidy, clang-format, cppcheck
- For building the internal documentation: Doxygen
- For performing coverage testing: gcovr

### Instructions
First, build `libminizinc` as shown:
```console
$ git clone https://github.com/MiniZinc/libminizinc.git
$ cd libminizinc
$ cmake -S . -B build
$ cmake --build build --target mzn
```

Then, build this project as shown:
```
$ git clone https://github.com/goncd/muminizinc.git
$ cd muminizinc
$ cmake -DCMAKE_MODULE_PATH=/path/to/libminizinc/cmake/modules/ -S . -B build
$ cmake --build build
```

### Available CMake options
You can customize the behaviour of the build system by setting the following options:
| Option                         | Description                       | Default value |
|--------------------------------|-----------------------------------|---------------|
| `MUMINIZINC_ENABLE_CLANG_TIDY` | Enable clang-tidy static analysis | `ON`          |
| `MUMINIZINC_ENABLE_CPPCHECK`   | Enable cppcheck static analysis   | `ON`          |
| `MUMINIZINC_BUILD_TESTS`       | Build tests                       | `ON`          |
| `MUMINIZINC_BUILD_DOCS`        | Build internal documentation      | `ON`          |
| `MUMINIZINC_USE_GCOVR`         | Enable coverage testing           | `ON`          |
| `MUMINIZINC_USE_SANITIZERS`    | Use sanitizers on debug builds    | `ON`          |

## Implemented mutation operators
| Operator | Description                      |
|----------|----------------------------------|
| ROR      | Relational operator replacement  |
| AOR      | Arithmetic operator replacement  |
| SOR      | Set operator replacement         |
| COR      | Conditional operator replacement |
| UOD      | Unary operator deletion          |
| FCR      | Function call replacement        |
| FAS      | Function call argument swap      |

## License
This project is licensed under the [MIT license](LICENSE).