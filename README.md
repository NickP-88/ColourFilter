## Compatibility

**Colour Filter** is a night light and colour filter tray app for X11 Linux systems. 

**It does not currently support multiple displays or Wayland systems**

You will need the following dependencies, then you can build with the command:

gcc -std=gnu11 -O2 -Wall -Wextra -o "Colour Filter" ColourFilter.c $(pkg-config --cflags --libs gtk+-3.0 ayatana-appindicator3-0.1 x11 xrandr libconfig)

## Using the Colour Filter

The icon image should appear in the tray automatically upon opening the app. Right click the icon to open the **Settings** window.

The four sliders will set the colour balance and brightness. If the filter is off then it will turn on for a few seconds to preview your new setting. Colour settings can be turned down to 0 individually, however note that you are prevented from turning them all to 0 at the same time, as this would render the display completely blank. 

Filter mode is set by the middle button at the bottom of the window: 
**Time** will turn the filter on and off based on the **Start Time** and **End Time**, which can each be set with their drop-down boxes. 
**On** means the filter is always on regardless of the time.
**Off** means the filter is always off regardless of the time.

## Config Settings

There are 3 additional settings the user may want to adjust in the **config.cfg** file:
**CheckSeconds** is how frequently the time will be checked to see if the filter should be turned on or off. By default it is every 20 seconds. 
**EaseSteps** is how many steps the easing process will take. More steps means smoother easing. A value of 1 will turn the filter directly on and off with no easing.
**StepTime** is the length of time in seconds between ease steps. 50 ease steps at 0.1 seconds each will mean it takes 5 seconds to fully transition the filter on or off. You may choose larger values if you want a longer and smoother transition, or smaller values if you want a faster transition. 
Note that this ease time also applies to fading out previewed values.

## In Case of Emergency

Colour Filter adjusts X11 display settings directly, so if the process is terminated abruptly it may leave the display in an undesirable state. In this case you may open the app again, open the **Settings** then **Help** windows, there you will see the **Reset** button. This will reset your display settings to a typical value, reset the config file to its default values, remove any currently applied filter, and exit the app. 

## Who Are You?

This was made as a personal project which is on hold now since I'm no longer using the Linux build which required it. If you are interested in using this app then feel free to send me a message on GitHub or via [my website](https://nick.org.nz/), I would be happy to see someone making use of it and would consider further development if necessary.

## License

Released under the MIT license.
