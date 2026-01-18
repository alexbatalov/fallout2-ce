#!/bin/bash

docker run --rm \
  -v $(pwd):/app --workdir /app  \
  --user $(id -u):$(id -g) silkeh/clang:18 \
  bash -c 'find src -type f -name \*.cc -o -name \*.h | xargs clang-format -i'

