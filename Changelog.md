# Changelog

## Upcoming Changes

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