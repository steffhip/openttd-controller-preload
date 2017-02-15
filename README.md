# A simple and minimalistic preload to be able to play Openttd with gamepad.

This preload allows users to play openttd with gamepad under linux without patching the sourcecode of openttd.

This is done by intercepting the calls to the SDL libraray and changing the returned events.
This preload might also work (with minor changes) with other games which use SDL 1.

## Limitations

This program only works under Linux.

Only the Delete- and Ctrl-Key and mousemovements and -clicks are mapped.

## Usage:

To run it first compile this program. This is done by simply typing "make".
This also creates a helperscript which can be used to launch openttd with controller support.
(This only works if openttd is installed in /usr/bin/openttd. Otherwise the path can be adjusted in the Makefile, or provided via the environment like: OPENTTD_BINARY=/path/to/openttd/openttd make)

To run openttd with controller support, simply start it with the generated helperscript or preload (LD_PRELOAD=/path/to/preload.so /path/to/openttd) the generated library file.

