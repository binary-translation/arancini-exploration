# Arancini Logging

Arancini includes a user-controllable logger that can be used to gather
information about the behaviour of the static and dynamic translation.

The logger is enabled by default only on debug builds of Arancini (as defined by
`CMAKE_BUILD_TYPE`), but it may also be enabled by specifying the `ENABLE_LOGGING`
CMake flag when building Arancini. When the logger is enabled, additional
environment variables may be specified in the invocation of the `txlat` and the
invocation of the translated binary.

If the logger is enabled, it can print messages at different levels of
granularity, depending on the type of the message. The following levels are
supported debug logging, informational message logging, warning logging, error
logging and fatal error logging. The default setting is to print warnings, errors
and fatal errors, when the logger is enabled. This setting is controlled with
`ARANCINI_LOG_LEVEL`.

In order to enable the logger, invoke the binary translated by Arancini with the
following flags:

- `ARANCINI_ENABLE_LOG=true`

- `ARANCINI_LOG_LEVEL=(debug|info|warn|error|fatal)`

Note that for all options to both `ARANCINI_ENABLE_LOG` and `ARANCINI_LOG_LEVEL`,
it is possible to specify their setting in a case-insensitive manner. For
instance, `ARANCINI_LOG_LEVEL=debug` and `ARANCINI_LOG_LEVEL=DEBUG` both work.

The same flags apply when logging `txlat` itself, but they do not affect the
produced binary.

In either case, a stream of message may become visible, usually prefixed with the
logging level.

## Verbose Translations

Arancini also supports logging different components. For instance, Arancini may
print the exact instructions that it translated from a given set of x86
instructions when the `ENABLE_VERBOSE_CODE_GEN` environment variable is set to
`true`.

