# `grpc_playground`

## Objective

A relatively trivial example to:

* Build a C++ project that uses gRPC.
* By means of `cmake`, in a cross-platform way.
* Gather and `protoc`-compile all the `.proto` files.
* Build all the `*.cc` sources into the respective targets.
* Employ a `Makefile` to have the "IDE" Vi-friendly with `:mak`.
* Also, use `googletest`, from `C5T/current`, to test the above.
* Use a pre-installed gRPC in the system, if `GRPC_INSTALL_DIR` is set.

## Usage

From an Ubuntu shell, just `make` should do the job. Run `Debug/test_{smoke,add,mul}` tests to confirm:

```
git clone https://github.com/dkorolev/grpc_playground.git && \
(cd grpc_playground; make && ./Debug/test_smoke && ./Debug/test_add)
```

Also, from Ubuntu, "opening" the `CMakeLists.txt` file with QT Creator should work too.

Finally, on Windows, "Open Folder" should work with the respective folder.

## Pre-Installed C++ gRPC

Having a pre-installed gRPC would help quite a bit. Please follow the instructions [here](https://grpc.io/docs/languages/cpp/quickstart/), then set an environmental variable `GRPC_INSTALL_DIR` to the value of `CMAKE_INSTALL_PREFIX` you have used through the steps above. You may also want to set `CMAKE_BUILD_TYPE=Release` while building and installing gRPC.

If C++ gRPC is not installed in the system, the first "build" will be slow, as it needs to:

* Fetch 300MB+ of gRPC code, and
* Build this very gRPC code.

After this stage is done, the further builds (one-line changes) are quick.

## Further Work

* A Docker Container to build the above code.
* NOTE(dkorolev): Done in September 2022. ~~With a pre-fetched, and perhaps pre-built `gRPC`.~~
* Integrated with this repo via a GitHub action.
* Performance tests.
* And maybe compare performance of gRPC services across languages (C++ vs. JVM vs. Go).

## Credits

* **[@abakay](https://github.com/abakay)**, for making me use `cmake` (although he doesn't endorse me running it via `make` from my `vi`).
* **[@vladsadovsky](https://github.com/vladsadovsky)**, for the reference gRPC example, [test-grpc-cmake](https://github.com/vladsadovsky/test-grpc-cmake).
