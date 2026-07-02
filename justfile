set shell := ["bash", "-euo", "pipefail", "-c"]

docs:
    bash scripts/check-docs.sh

cpp:
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
    cmake --build build --parallel
    ctest --test-dir build --output-on-failure

sanitizers:
    cmake -S . -B build-sanitized -DCMAKE_BUILD_TYPE=Debug -DTICKLINE_ENABLE_SANITIZERS=ON
    cmake --build build-sanitized --parallel
    ctest --test-dir build-sanitized --output-on-failure

python:
    PYTHONPATH=tools/python python3 -m unittest discover -s tools/python/tests -v

docker:
    docker build -f infra/docker/Dockerfile .

check:
    bash scripts/check-local.sh

check-fast:
    SKIP_SANITIZERS=1 SKIP_DOCKER=1 bash scripts/check-local.sh

clean:
    bash scripts/clean.sh
