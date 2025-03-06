# "Sisyphus" Pebble Watchface

I created this watch face for the [March 2025 Rebble
Hackathon](https://rebble.io/hackathon-002/).  It's based on the myth of
Sisyphus, pushing a boulder up a hill eternally.

![A screen shot of the watch face](screenshots/screenshot.png)

There's also [a timelapse movie](screenshots/screenshot.mp4) demonstrating how
the watch changes over the course of a minute.

The red circle is the boulder, and it moves up the hill, only to crash back down
every minute as the second hand strikes zero:

## Requirements

You need a working development environment, and this is not trivial, as the SDK
is ten years old at time of writing. For this project I used the [`rebble`
tool](https://github.com/richinfante/rebbletool) to create the project skeleton,
build, install, et cetera.

There are tons more options, Docker, Nix, VMs.  Check out the docs and Discord
channels provided by the [Rebble community](https://rebble.io/) for more
information.

## Installing

Once you've got a working development, you can build the app from the root of
the repository using a command like:

`rebble build`

After the app is built, you can test it in the emulator using a command like:

`rebble install --emulator basalt`.

You can also install it on your watch.  If you have the pebble app installed on
your phone, enable developer mode, and make a note of the IP adddress.  You can
then install the app using a command like:

`rebble install --phone <YOUR_IP_ADDRESS>`