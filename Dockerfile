# syntax=docker/dockerfile:1.7
#
# Multi-stage build for the ESA-CCSDS LSIS-AFS Goonhilly interop workshop.
# Builder compiles the C++ extension + Python wheel; runtime image installs
# only the wheel into python:3.12-slim — no src/, no cpp/, no .git, no
# toolchain leaks into the shipped layers.
#
# Build:   docker build -t lunalink:workshop .
# Ship:    docker save lunalink:workshop | gzip > lunalink-workshop.tar.gz
# Load:    docker load < lunalink-workshop.tar.gz
# Run:     docker run --rm -v "$(pwd)":/data lunalink:workshop \
#              decode --prn 1 --format iq32 --rate 1023000 --input /data/signal.iq32

ARG PYTHON_VERSION=3.12

# ─── Builder ─────────────────────────────────────────────────────────────
FROM python:${PYTHON_VERSION}-slim AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
        g++ \
        cmake \
        ninja-build \
        git \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# BUILD_TESTING=OFF: the wheel doesn't ship C++ unit-test binaries.
# Version provisioning falls back to setuptools_scm.fallback_version
# ("0.0.0.dev0") when .git isn't in the build context (e.g. OSS export).
RUN pip install --no-cache-dir --upgrade pip build \
 && SKBUILD_CMAKE_DEFINE="BUILD_TESTING=OFF" \
    python -m build --wheel --outdir /wheels

# ─── Runtime ─────────────────────────────────────────────────────────────
FROM python:${PYTHON_VERSION}-slim

COPY --from=builder /wheels/lunalink-*.whl /tmp/
RUN pip install --no-cache-dir /tmp/lunalink-*.whl \
 && rm -rf /tmp/*.whl /root/.cache

ENTRYPOINT ["lunalink"]
