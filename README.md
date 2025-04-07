# Arduino GIGA Display - GUI Templates Library
This is a public repository for Arduino GIGA / LVGL GUI templates.

![a render of the motor controller template](/assets/images/motor_controller_1.jpg)

Do you have an Arduino project that you would love to control via touchscreen display?\
Why are there no simple, clean, plug-and-play GUI templates out there, which you can quickly adapt to your project?

This library aims to change that, by providing clean GUI templates, with plug-and-play Arduino functionality.\
As of now, templates are primarily built for the [Arduino GIGA Display](https://store-usa.arduino.cc/products/giga-display-shield), although they may be easily adapted for other displays.

Also includes STL files for a 3D printed enclosure.

## Getting Started

- Download the Arduino IDE
- Install the LVGL library (version 8.3.11) in the Arduino IDE
- Install the "Arduino Mbed OS GIGA Boards" board manager
- Download and open one of the templates in this library
- Modify and add to the template, as your project requires
- Upload to your Arduino GIGA Display
- (Optional) Download, 3D print, and assemble the display enclosure files
- Rejoice!

![a render of the home automation template](/assets/images/home_auto_1.png)

## Notices

- As of this writing, Arduino does not support the latest releases of LVGL. To ensure these templates compile properly, you must install LVGL version 8.3.11 in the Arduino IDE.

## Enclosure

This repo contains additional files for a 3-D printed enclosure for the GIGA Display, designed by me.\
The enclosure comprises a "main-body" part, as well as a top and bottom cap. To complete the assembly, you will need to mount M3 heat-set inserts into the main body.

In addition to the GIGA's output ports, I've included a separate slot in the main body for an [8-position Molex connector](https://www.digikey.com/en/products/detail/molex/2147571081/12180338), which can be used as a clean port for some of the GIGA's GPIO pins.\
There is also a cutout on the bottom of the main body for a power switch.

Finally, this enclosure can be configured as a standalone powered device. I've accomplished this by packing a 3.7V LiPo battery pack and [5V Step-up regulator](https://www.pololu.com/product/4941) into the enclosure, for example.

---

[Interested in product design services in the Twin Cities?](https://www.psoup.com/)
