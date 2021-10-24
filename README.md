Northpaw Information
====================

Northpaw was a kit sold by [Sensebridge: making the invisible visible](https://sensebridge.net/projects/northpaw/northpaw-downloads/)
which has unfortunately gone out of business. I am keeping some of the information here as I'm hoping to find time to build
a version of the Northpaw myself. The information below and in this repo is copied from the Sensebridge site:

- https://sensebridge.net/projects/northpaw/
- https://sensebridge.net/projects/northpaw/northpaw-downloads/

---

A North Paw is an anklet that tells the wearer which way is North. The anklet holds eight cellphone vibrator motors around your ankle. A control unit senses magnetic north and turns on and off the motors. At any given time only one motor is on and this motor is the closest to North. The skin senses the vibration, and the wearer’s brain learns to associate the vibration with direction, giving the wearer an intuitive sense of which way is North. Most people “get it” mere seconds after putting it on, and can then reliably point north when asked.

What makes it way more awesome than a regular compass? Persistence. With a regular compass the owner only knows the direction when he or she checks it. With this compass, the information enters the wearer’s brain at a subconscious level, giving the wearer a true feeling of absolute direction, rather than an intellectual knowledge as with a regular compass.

Because of the plasticity of the brain, it has been shown that most wearers gain a new sense of absolute direction, giving them a superhuman ability to navigate their surroundings. The original idea for North Paw comes from research done at University of Osnabrück in Germany. In this study, rather than an anklet, the researchers used a belt. They wore the belt non-stop for six weeks, and reported successive stages of integration.

We would never have gotten this far without the generous help of many friends, wise acquaintances, and other hackers. Sensebridge was created as part of a community, and we want to keep that spirit.

At Sensebridge we believe in keeping things open source. All versions of North Paw electronics are essentially Arduino clones with some extra stuff onboard, you can program them using the Arduino IDE. Use Board “Arduino Pro or Pro Mini (3.3V, 8MHz) w/ ATMEGA168”. Download all of our design files and code here:

EagleCAD board and schematic files: North_Paw_V2p0.brd (348KB), North_Paw_V2p0.sch (78KB)

Arduino 1.0 Code: NorthPaw_V2p2.ino (16k) (this 2.2 update makes the firmware compatible with recent changes in the Pololu LSM303 library (removal of setMagGain function, change of vector to a collection, change in handing of LSB placement for accelerometer data). No functionality changes, if you have an older copy of the LSM303 library, no update required.

NorthPaw_V2p1_final.ino (16K) (this V2.1 update includes fixes to compass sensitivity and adds a variable for simple user control of motor strength)

Bill of Materials: NorthPaw_V2p0_BOM.csv

Older V1.5 files:

EagleCAD board and schematic files: NorthPaw_V1p5.sch (76KB) NorthPaw_v1p5.brd (357KB)

Bill of materials: NorthPaw_V1p5_BOM.csv (2KB)

Arduino code: North_Paw_v1p5.pde (15KB)

Older V1.0 downloads:

EagleCAD board and schematic files: North_Paw_v1p0.sch (133KB) North_Paw_v1p0.brd (355KB)

Bill of materials: North_Paw_BOM_v1p0.csv (1KB)

Arduino code: North_Paw_v1p0.pde (10KB)