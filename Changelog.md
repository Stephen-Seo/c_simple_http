# Changelog

## Upcoming Changes

Implemented "IF", "ELSEIF", "ELSE", "ENDIF", and "INDEX" for templates.

IF is used like: `{{{!IF Variable==SomeString}}}`.
ELSEIF is used like: `{{{!ELSEIF Variable==AnotherString}}}`.
ELSE is used like: `{{{!ELSE}}}`.
ENDIF is used like: `{{{!ENDIF}}}`.
INDEX is used like: `{{{!INDEX ArrayVar[2]}}}`.

Implemented "FOREACH" and "ENDFOREACH" for templates.

FOREACH is used like:

    HTML='''
        {{{!FOREACH ArrayVar}}}
            {{{ArrayVar}}}
        {{{!ENDFOREACH}}}'''
    ArrayVar=FirstValue
    ArrayVar=SecondValue
    ArrayVar=ThirdValue

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
