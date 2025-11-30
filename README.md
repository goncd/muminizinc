# MuMiniZinc
MuMiniZinc is an open-source mutation test tool for [MiniZinc](https://www.minizinc.org/).

## Basic usage
To analyse a model and determine what mutants can be generated:
```bash
$ muminizinc analyse <MODEL>
```

To generate the mutants and store them:
```bash
$ muminizinc applyall <MODEL>
```

To run the mutants and get the mutation matrix:
```bash
$ muminizinc run <MODEL>
```

To get more information about the commands:
```bash
$ muminizinc help
$ muminizinc help <COMMAND>
```

## Obtaining MuMiniZinc
Most users will want to use the Docker image, as it's an automated process and there are fewer requirements to satisfy.

### Using the Docker image (recommended)
First, install Docker. Then, add the `docker` folder to your `PATH` environment variable. Then, make sure the appropiate script is executable by your user.

You will be able to run MuMiniZinc as follows:
```bash
$ muminizinc
```

The current directory will be automatically mounted to the container, so you can work with models that are in your current directory or below it.

The official MiniZinc package is automatically downloaded, so you don't need to bring your own copy of the compiler or the standard library.

You can change the download path of both libminizinc and the MiniZinc package by setting the `LIBMINIZINC_URL` and `MINIZINC_URL` arguments respectively when building the image.

### Manually building the project

#### Requirements
- A C++23 compiler, like GCC, Clang or MSVC.
- CMake 3.30 or higher.
- Boost 1.88 or higher.
- nlohmann_json 3.12.0 or higher.

#### Optional requirements
- For static analysis and formatting: clang-tidy, clang-format, cppcheck
- For building the internal documentation: doxygen
- For performing coverage testing: gcovr

#### Instructions
First, build libminizinc as shown:
```bash
$ git clone https://github.com/MiniZinc/libminizinc.git
$ cd libminizinc
$ cmake -S . -B build
$ cmake --build build --target mzn
```

Then, build this project as shown:
```bash
$ git clone https://github.com/goncd/muminizinc.git
$ cd muminizinc
$ cmake -DCMAKE_MODULE_PATH=/path/to/libminizinc/cmake/modules/ -S . -B build
$ cmake --build build
```

Once it's built, the MuMiniZinc executable should be at `build/muminizinc`.

To run the mutants and the models, as well as to analyse some models, you will need to have a copy of MiniZinc and its standard library, which you can get on its official website. Then, add the `bin` path to the `PATH` environment variable or set the `--compiler-path` argument. You may also need to specify the standard library path with `--include` if it's not autodetected. **You don't need to download anyting if you are using the Docker image, as it already downloads the official MiniZinc package for you**.

#### Available CMake options
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