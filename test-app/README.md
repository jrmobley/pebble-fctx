# pebble-fctx/test-app

This is a simple test and demonstration app for the pebble-fctx library.  It should produce a watchface that looks like so:

![Aplite](screenshots/aplite.png) ![Basalt](screenshots/basalt.png) ![Chalk](screenshots/chalk.png)

### Font Resource

The file `resources/din-condensed.ffont` had been generated automatically from `font/din-condensed.svg`.  The command to re-generate the font is

    ./node_modules/.bin/fctx-compiler font/din-condensed.svg

It may be necessary to run `pebble build` at least once in order to install the package dependencies before this command is run.
