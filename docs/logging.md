# Arancini Logging

Arancini includes a user-controllable logger that can be used to gather
information about the behaviour of the translator during program runtime.

In order for a user to interact with the logger, Arancini must have been compiled
with an enabled logger (the default). The setting controlling whether the logger
is enabled or disabled at compile-time is given by CMake flag `ENABLE_LOGGING`.
If Arancini was compiled with a logger, translated programs will include logging
functionality that can be controlled by the user. Otherwise, no logging is
possible.

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

Note that these flags apply when directly running the translated binary, not
Arancini itself. If correctly specified, a stream of messages should be visible.

## Verbose Translations

Arancini also supports logging different components. For instance, Arancini may
print the exact instructions that it translated from a given set of x86
instructions when the `ENABLE_VERBOSE_CODE_GEN` environment variable is set to
`true`.

