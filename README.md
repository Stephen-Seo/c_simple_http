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
    ./c_simple_http --config=example_config/example.config
    # If built with cmake:
    ./buildDebug/c_simple_http --config=example_config/example.config
    
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
