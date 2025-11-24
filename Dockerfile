FROM ubuntu:25.10 AS builder

ARG LIBMINIZINC_URL="https://github.com/MiniZinc/libminizinc.git"
ARG MINIZINC_URL="https://github.com/MiniZinc/MiniZincIDE/releases/download/2.9.4/MiniZincIDE-2.9.4-bundle-linux-x86_64.tgz"

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        bison ca-certificates cmake doxygen flex g++ git libboost-atomic-dev \
        libboost-context-dev libboost-filesystem-dev libboost-process1.88-dev \
        libegl1 libfontconfig1 libgl1 make nlohmann-json3-dev wget \
    && rm -rf /var/cache/apt/archives /var/lib/apt/lists/*

WORKDIR /dependencies

RUN git clone "$LIBMINIZINC_URL" libminizinc \
    && cd libminizinc \
    && cmake -S . -B build \
        -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --target mzn

WORKDIR /dependencies/minizinc

RUN wget -q "$MINIZINC_URL" -O minizinc.tgz \
    && tar -xzf minizinc.tgz --strip-components=1 \
    && rm minizinc.tgz

WORKDIR /app

COPY . .

RUN cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -Dlibminizinc_DIR=/dependencies/libminizinc/build \
    -DMUMINIZINC_ENABLE_CLANG_TIDY=OFF \
    -DMUMINIZINC_ENABLE_CPPCHECK=OFF \
    -DMUMINIZINC_BUILD_DOCS=OFF \
    -DMUMINIZINC_USE_GCOVR=OFF \
    -DMUMINIZINC_USE_SANITIZERS=OFF \
    && cmake --build build --config Release

RUN strip --strip-all build/muminizinc

ENV PATH="/dependencies/minizinc/bin:${PATH}"
WORKDIR /app/build
RUN ctest --output-on-failure

FROM ubuntu:25.10

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        libboost-process1.88.0 libegl1 libfontconfig1 libgl1 \
    && rm -rf /var/cache/apt/archives /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /dependencies/minizinc .
COPY --from=builder /app/build/muminizinc bin/muminizinc

RUN chown ubuntu:ubuntu bin/muminizinc && chmod +x bin/muminizinc
USER ubuntu

ENV PATH="/app/bin:${PATH}"

ENTRYPOINT ["./bin/muminizinc"]
