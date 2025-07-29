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
      --generate-static-enable-overwrite

## Changelog

See the [Changelog.](https://github.com/Stephen-Seo/c_simple_http/blob/main/Changelog.md)

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

## Template Usage

Variables defined in the config will be expanded in the `HTML` or `HTML_FILE`.

`HTML_FILE` is used like so:

    PATH=/
    HTML_FILE='''other.html'''
    MY_VARIABLE='''Some text'''

    <!-- other.html -->
    <html>
        <p>A variable is {{{MY_VARIABLE}}}</p>
    </html>

Note that any variable that ends in `_FILE` will cause the templating system to
insert the contents of the filename specified by the variable.

A variable can be specified in the html file like: `{{{MY_VARIABLE}}}`.

To specify an array, use the variable multiple times:

    MY_VARIABLE=FirstItem
    MY_VARIABLE=SecondItem
    MY_VARIABLE=ThirdItem

Using `{{{MY_VARIABLE}}}` should only return the first element of the array if
specified, and using `INDEX` can select one of the items by index.

These are the statements that can be used in templating:

 - IF
 - ELSEIF
 - ELSE
 - ENDIF
 - INDEX
 - FOREACH
 - ENDFOREACH

An example of using `IF`:

    PATH=/
    HTML='''
        {{{!IF MY_VARIABLE==AString}}}
            <p>MY_VARIABLE is AString</p>
        {{{!ELSEIF MY_VARIABLE!=OtherString}}}
            <p>MY_VARIABLE is not OtherString</p>
        {{{!ELSE}}}
            <p>MY_VARIABLE is something else</p>
        {{{!ENDIF}}}
    '''

An example of using `INDEX`:

    PATH=/
    HTML='''
        {{{!INDEX MY_VARIABLE[0]}}}
        {{{!INDEX MY_VARIABLE[1]}}}
        {{{!INDEX MY_VARIABLE[2]}}}
    '''
    MY_VARIABLE=FirstItem
    MY_VARIABLE=SecondItem
    MY_VARIABLE=ThirdItem

An example of using `FOREACH`:

    PATH=/
    HTML='''
        {{{!FOREACH MY_VARIABLE!OTHER_VARIABLE}}}
            <p>MY_VARIABLE is {{{MY_VARIABLE}}} and OTHER_VARIABLE is {{{OTHER_VARIABLE}}}</p>
        {{{!ENDFOREACH}}}
    '''
    MY_VARIABLE=FirstMy
    MY_VARIABLE=SecondMy
    OTHER_VARIABLE=FirstOther
    OTHER_VARIABLE=SecondOther

`FOREACH` statements can be nested.

    PATH=/
    HTML='''
    {{{!FOREACH ArrayValue}}}
      {{{ArrayValue}}}
      {{{!FOREACH ArrayValueSecond}}}
        {{{ArrayValueSecond}}}
        {{{!FOREACH ArrayValueThird}}}
          {{{ArrayValueThird}}}
          {{{!FOREACH ArrayValueFourth}}}
            {{{ArrayValueFourth}}}
            {{{!FOREACH Each_FILE}}}
              {{{Each_FILE}}}
            {{{!ENDFOREACH}}}
          {{{!ENDFOREACH}}}
        {{{!ENDFOREACH}}}
      {{{!ENDFOREACH}}}
    {{{!ENDFOREACH}}}
    '''
    ArrayValue=FirstArr0
    ArrayValue=FirstArr1
    ArrayValueSecond=SecondArr0
    ArrayValueSecond=SecondArr1
    ArrayValueThird=ThirdArr0
    ArrayValueThird=ThirdArr1
    ArrayValueFourth=FourthArr0
    ArrayValueFourth=FourthArr1

`IF` statements should work regardless of whether or not it is nested.

## Other Notes

The config file can be reloaded if the program receives the SIGUSR1 signal.  
The `--enable-reload-config-on-change` option automatically reloads the config
file if the config file has changed.

The `--enable-cache-dir=<DIR>` option enables caching and sets the "cache-dir"
at the same time. `--cache-entry-lifetime-seconds=<SECONDS>` determines when a
cache entry expires.
[The default expiry time of a cache entry is 1 week.](https://github.com/Stephen-Seo/c_simple_http/blob/fca624550f3be0452b8334978392cd679db30fa1/src/constants.h#L31)
