# MuMiniZinc
MuMiniZinc is a mutation test tool for [MiniZinc](https://www.minizinc.org/).

## Building instructions

### Building libminizinc
1. `git clone https://github.com/MiniZinc/libminizinc.git`
2. `cd libminizinc`
3. `cmake -S . -B build`
4. `cmake --build build --target mzn`

### Building MuMiniZinc
1. `cmake -DCMAKE_MODULE_PATH=/path/to/libminizinc/cmake/modules/ -S . -B build`
2. `cmake --build build`
