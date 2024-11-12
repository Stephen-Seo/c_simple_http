# C Simple HTTP

A simple HTTP/1.1 server written in C.

## Usage

    Usage:
      -p <port> | --port <port>
      --config=<config_file>
      --disable-peer-addr-print
      --req-header-to-print=<header> (can be used multiple times)
        For example: --req-header-to-print=User-Agent
        Note that this option is case-insensitive
      --enable-reload-config-on-change
      --enable-cache-dir=<DIR>
      --cache-entry-lifetime-seconds=<SECONDS>
      --enable-static-dir=<DIR>
      --generate-dir=<DIR>
      --generate-enable-overwrite

## Changelog

See the [Changelog.](https://git.seodisparate.com/stephenseo/c_simple_http/src/branch/main/Changelog.md)

## Before Compiling

Make sure that the git submodule(s) are loaded:

    git submodule update --init --recursive --depth=1 --no-single-branch

Without this, the project will fail to build.

## Example

    # Build the project.
    make c_simple_http
    
    # Alternatively, build with cmake.
    cmake -S . -B buildDebug && make -C buildDebug
    
    # Run it with the example config.
    # Note that the example config was designed such that it must be referred
    # to from its parent directory.
    ./c_simple_http --config=example_config/example.config --enable-static-dir=example_static_dir
    # If built with cmake:
    ./buildDebug/c_simple_http --config=example_config/example.config --enable-static-dir=example_static_dir
    
    # If port is not specified, the server picks a random port.
    # This program should print which TCP port it is listening on.
    # Sometimes the program will fail to rebind to the same port due to how TCP
    # works. Either wait some time or choose a different port.
    
    # Access the website.
    # This assumes the server is hosted on port 3000.
    curl localhost:3000

## Other Notes

The config file can be reloaded if the program receives the SIGUSR1 signal.  
The `--enable-reload-config-on-change` option automatically reloads the config
file if the config file has changed.

The `--enable-cache-dir=<DIR>` option enables caching and sets the "cache-dir"
at the same time. `--cache-entry-lifetime-seconds=<SECONDS>` determines when a
cache entry expires.
[The default expiry time of a cache entry is 1 week.](https://git.seodisparate.com/stephenseo/c_simple_http/src/commit/3f1be0cf496eab7242ab997262d85af11337039b/src/constants.h#L28)
