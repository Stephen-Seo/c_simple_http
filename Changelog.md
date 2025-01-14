# Changelog

## Upcoming Changes

## Version 1.7.2

Update third-party submodule SimpleArchiver.

## Version 1.7.1

Fix submodule url (SimpleArchiver).

## Version 1.7

Fix usage of indexing in `IF`.  
Note this is used like `{{{!IF SomeVar[2]==true}}}`.

## Version 1.6

Fix usage of `IF` with a `FOREACH` variable when the `IF` is nested in the
`FOREACH` and is referring to a variable expanded by `FOREACH`.

## Version 1.5

Add flag `--generate-static-enable-overwrite`. This flag enables overwriting of
files from static-dir to generate-dir (if static-dir was specified). Previous
implementation relied on `--generate-enable-ovewrite` for this behavior.

Minor refactorings related to `printf` and `uintX_t`/`size_t` types.

## Version 1.4

Implemented "IF", "ELSEIF", "ELSE", "ENDIF", and "INDEX" for templates.

IF is used like: `{{{!IF Variable==SomeString}}}`.  
Not equals can also be used: `{{{!IF Variable!=OtherString}}}`.  
ELSEIF is used like: `{{{!ELSEIF Variable==AnotherString}}}`.  
Not equals can also be used: `{{{!ELSEIF Variable!=AnotherOtherString}}}`.  
ELSE is used like: `{{{!ELSE}}}`.  
ENDIF is used like: `{{{!ENDIF}}}`.  
INDEX is used like: `{{{!INDEX ArrayVar[2]}}}`.

Implemented "FOREACH" and "ENDFOREACH" for templates.

FOREACH is used like:

    PATH=/
    HTML='''
        {{{!FOREACH ArrayVar}}}
            {{{ArrayVar}}}
        {{{!ENDFOREACH}}}'''
    ArrayVar=FirstValue
    ArrayVar=SecondValue
    ArrayVar=ThirdValue

For multiple variables to expand in FOREACH:

    PATH=/
    HTML='''
        {{{!FOREACH ArrayVar!ArrayVarSecond!ArrayVarThird}}}
            {{{ArrayVar}}}
            {{{ArrayVarSecond}}}
            {{{ArrayVarThird}}}
        {{{!ENDFOREACH}}}'''
    ArrayVar=FirstVarOnce
    ArrayVar=FirstVarTwice
    ArrayVarSecond=SecondVarOnce
    ArrayVarSecond=SecondVarTwice
    ArrayVarThird=ThirdVarOnce
    ArrayVarThird=ThirdVarTwice

Implemented nestable "IF" and "FOREACH" expressions in templates. In other
words, there can be `{{{!IF}}}` inside other IF/FOREACH blocks, and vice versa.

## Version 1.3

Fix internal erronous buffer declaration.

Fix internal missing NULL check.

## Version 1.2

Add the `--generate-dir=<DIR>` option, which will generate all html into the
given directory. This requires `--config=<CONFIG_FILE>`.
`--generate-enable-overwrite` is required to overwrite existing files when using
`--generate-dir=<DIR>`.

If `--enable-static-dir=<DIR>` is also specified with generate, then the files
in the given directory will be copied into the directory specified with
`--generate-dir=<DIR>`.

## Version 1.1

Some refactoring of code handling parsing the config file.

Remove buffer-size-limit on config file entries.

## Version 1.0

First release.

Features:

  - Serve templated html files via a config.
  - Reload configuration on SIGUSR1 or by listening (enabled by cmd parameter).
  - Cache served html (enabled by cmd parameter).
  - Serve static files from "static-dir" (enabled by cmd parameter).

<!--
    vim: textwidth=80 et sw=2 ts=2 sts=2
-->
