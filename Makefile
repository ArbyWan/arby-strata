# arby-strata build.  WSL2/Linux, one AMD GPU (gfx1201) through HIP.
#
#   git submodule update --init
#   make build        # build-hip/bin/{strata, arby-strata}
#   make test         # the front end against a fake engine; no GPU, no model
#
# The front end needs no GPU, no ROCm and no model: `make frontend` builds it alone on any box with CMake and a
# C++20 compiler.  Nothing here downloads anything; ggml, llama, common, mtmd and cpp-httplib all come from
# third_party/llama.cpp.
#
# HIP_ARCHS: gfx1100, gfx1101, gfx1200, gfx1201 or a list such as "gfx1100;gfx1201".  Our box is gfx1201.
# JOBS: capped at 16 because the box has 16 cores and the memory per HIP compile unit is large.

BUILD_DIR     ?= build-hip
FE_BUILD_DIR  ?= build-frontend
HIP_ARCHS     ?= gfx1201
# ROCm's own prefix. CMake finds the HIP *compiler* on its own, but find_package(hip CONFIG) does not search
# /opt/rocm, so the prefix has to be handed to configure explicitly or everything downstream of it fails.
ROCM_PATH     ?= /opt/rocm
JOBS          ?= $(shell n=$$(nproc 2>/dev/null || echo 8); [ $$n -gt 16 ] && n=16; echo $$n)
TARGETS       ?= strata arby-strata

.PHONY: all configure build frontend test clean

all: build

configure:
	@test -f third_party/llama.cpp/ggml/CMakeLists.txt || \
	  { echo "third_party/llama.cpp is empty: run 'git submodule update --init' first"; exit 1; }
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release \
	      -DSTRATA_ENABLE_HIP=ON -DSTRATA_ENABLE_CUDA=OFF \
	      -DSTRATA_NATIVE_EXPERTS=ON -DSTRATA_BUILD_TESTS=OFF \
	      -DCMAKE_HIP_ARCHITECTURES=$(HIP_ARCHS) \
	      -DCMAKE_PREFIX_PATH=$(ROCM_PATH) \
	      -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=$(abspath $(BUILD_DIR))/bin

build: configure
	cmake --build $(BUILD_DIR) -j $(JOBS) --target $(TARGETS)
	@echo "built:"; ls -1 $(BUILD_DIR)/bin/strata $(BUILD_DIR)/bin/arby-strata

# The front end alone, without HIP and without CUDA: a laptop or a CI box can run this.
frontend:
	cmake -S . -B $(FE_BUILD_DIR) -DCMAKE_BUILD_TYPE=Release \
	      -DSTRATA_ENABLE_CUDA=OFF -DSTRATA_BUILD_TESTS=OFF \
	      -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=$(abspath $(FE_BUILD_DIR))/bin
	cmake --build $(FE_BUILD_DIR) -j $(JOBS) --target arby-strata

# The adapter's cases (cancel, drain, backpressure, child exit, ERR) and the HTTP surface a client touches,
# against frontend/tests/fake-engine.py.  Needs python3 and curl; no GPU and no model.
test: frontend
	cmake --build $(FE_BUILD_DIR) -j $(JOBS) --target engine_test literals_test picture_cache_test
	$(FE_BUILD_DIR)/bin/engine_test frontend/tests/fake-engine.py
	$(FE_BUILD_DIR)/bin/literals_test
	$(FE_BUILD_DIR)/bin/picture_cache_test
	./frontend/tests/e2e.sh $(FE_BUILD_DIR)/bin/arby-strata

clean:
	rm -rf $(BUILD_DIR) $(FE_BUILD_DIR)
