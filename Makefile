# NOTE(dkorolev): 99% of the goal of this `Makefile` is to have `vim` jump to errors on `:mak`.
#
# NOTE(dkorolev): Yes, I am well aware it is ugly to have a `Makefile` for a `cmake`-built project.
#                 Just too much of a `vi` user to not leverage `:mak`.
#                 This `Makefile` also proves to be useful with Docker builds.

.PHONY: debug release debug_dir release_dir mark_deps_as_built indent clean

DEBUG_BUILD_DIR=$(shell echo "$${GRPC_PLAYGROUND_DEBUG_BUILD_DIR:-Debug}")
RELEASE_BUILD_DIR=$(shell echo "$${GRPC_PLAYGROUND_RELEASE_BUILD_DIR:-Release}")

NINJA=$(shell ninja --version 2>&1 >/dev/null && echo YES || echo NO)
ifeq ($(NINJA),YES)
  CMAKE_CONFIGURE_OPTIONS=-G Ninja
  CMAKE_BUILD_COMMAND=cmake --build
else
  CMAKE_CONFIGURE_OPTIONS=
  CMAKE_BUILD_COMMAND=MAKEFLAGS=--no-print-directory cmake --build
endif

ifdef GRPC_INSTALL_DIR
  CMAKE_CONFIGURE_COMMAND=cmake -DCMAKE_PREFIX_PATH="$(GRPC_INSTALL_DIR)" -DGRPC_INSTALL_DIR="$(GRPC_INSTALL_DIR)"
else
  CMAKE_CONFIGURE_COMMAND=cmake
endif

OS=$(shell uname)
ifeq ($(OS),Darwin)
  CORES=$(shell sysctl -n hw.physicalcpu)
else
  CORES=$(shell nproc)
endif

CLANG_FORMAT=$(shell echo "$${CLANG_FORMAT:-clang-format-10}")

debug: debug_dir
	${CMAKE_BUILD_COMMAND} "${DEBUG_BUILD_DIR}" -j ${CORES}

debug_dir: ${DEBUG_BUILD_DIR}/.configure_succeeded

${DEBUG_BUILD_DIR}/.configure_succeeded: CMakeLists.txt
	$(CMAKE_CONFIGURE_COMMAND) $(CMAKE_CONFIGURE_OPTIONS) -B "${DEBUG_BUILD_DIR}" .
	touch "${DEBUG_BUILD_DIR}/.configure_succeeded"

release: release_dir
	${CMAKE_BUILD_COMMAND} "${RELEASE_BUILD_DIR}" -j ${CORES}

release_dir: ${RELEASE_BUILD_DIR}/.configure_succeeded

${RELEASE_BUILD_DIR}/.configure_succeeded: CMakeLists.txt
	$(CMAKE_CONFIGURE_COMMAND) -DCMAKE_BUILD_TYPE=Release $(CMAKE_CONFIGURE_OPTIONS) -B "${RELEASE_BUILD_DIR}" .
	touch "${RELEASE_BUILD_DIR}/.configure_succeeded"

mark_deps_as_built:
	[ -d ${DEBUG_BUILD_DIR}/_deps ] && (find ${DEBUG_BUILD_DIR}/_deps -type f | xargs touch -r CMakeLists.txt) || true
	[ -d ${RELEASE_BUILD_DIR}/_deps ] && (find ${RELEASE_BUILD_DIR}/_deps -type f | xargs touch -r CMakeLists.txt) || true

indent:
	${CLANG_FORMAT} -i *.cc *.proto */*.cc */*.proto

clean:
	rm -rf "${DEBUG_BUILD_DIR}" "${RELEASE_BUILD_DIR}"
