# NOTE(dkorolev): 99% of the goal of this `Makefile` is to have `vim` jump to errors on `:mak`.
#
# NOTE(dkorolev): Yes, I am well aware it is ugly to have a `Makefile` for a `cmake`-built project.
#                 Just too much of a `vi` user to not leverage `:mak`.
#                 This `Makefile` also proves to be useful with Docker builds.

.PHONY: debug release debug_dir release_dir mark_deps_as_built indent clean

DEBUG_BUILD_DIR=$(shell echo "$${GRPC_PLAYGROUND_DEBUG_BUILD_DIR:-Debug}")
RELEASE_BUILD_DIR=$(shell echo "$${GRPC_PLAYGROUND_RELEASE_BUILD_DIR:-Release}")

OS=$(shell uname)
ifeq ($(OS),Darwin)
  CORES=$(shell sysctl -n hw.physicalcpu)
else
  CORES=$(shell nproc)
endif

CLANG_FORMAT=$(shell echo "$${CLANG_FORMAT:-clang-format-10}")

debug: debug_dir
	MAKEFLAGS=--no-print-directory cmake --build "${DEBUG_BUILD_DIR}" -j ${CORES}

debug_dir: ${DEBUG_BUILD_DIR}

${DEBUG_BUILD_DIR}: CMakeLists.txt
	cmake -B "${DEBUG_BUILD_DIR}" .

release: release_dir
	MAKEFLAGS=--no-print-directory cmake --build "${RELEASE_BUILD_DIR}" -j ${CORES}

release_dir:	${RELEASE_BUILD_DIR}

${RELEASE_BUILD_DIR}: CMakeLists.txt
	cmake -DCMAKE_BUILD_TYPE=Release -B "${RELEASE_BUILD_DIR}" .

mark_deps_as_built:
	[ -d ${DEBUG_BUILD_DIR}/_deps ] && (find ${DEBUG_BUILD_DIR}/_deps -type f | xargs touch -r CMakeLists.txt) || true
	[ -d ${RELEASE_BUILD_DIR}/_deps ] && (find ${RELEASE_BUILD_DIR}/_deps -type f | xargs touch -r CMakeLists.txt) || true

indent:
	${CLANG_FORMAT} -i *.cc *.proto */*.cc */*.proto

clean:
	rm -rf "${DEBUG_BUILD_DIR}" "${RELEASE_BUILD_DIR}"
