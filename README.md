# ANNOUNCING BYZANTIUM LINUX V0.3a (Beach Cat)
### Approved for: GENERAL RELEASE, DISTRIBUTION UNLIMITED

Project Byzantium, a working group of HacDC (http://hacdc.org/) is proud to announce the release of v0.3 alpha of Byzantium Linux, a live distribution of Linux which makes it fast and easy to construct an ad-hoc wireless mesh network which can augment or replace the existing telecommunications infrastructure in the event that it is knocked offline (for example, due to a natural disaster) or rendered untrustworthy (through widespread surveillance or disconnection by hostile entities). This release was developed in the days following Hurricane Sandy, and was perfected while the core development team was assisting with disaster relief efforts in the Red Hook neighborhood of New York City in November of 2012.

Byzantium Linux is designed to run on any x86 computer with at least one 802.11 a/b/g/n wireless interface. Byzantium can be burned to a CD- or DVD-ROM (the .iso image is around 300 megabytes in size), booted from an external hard drive, or can even be installed in parallel with an existing operating system without risk to the user's data and software. Byzantium Linux will act as a node of the mesh and will automatically connect to other mesh nodes and act as an access point for wifi-enabled mobile devices. This release of Byzantium Linux also incorporates seamless interoperability with mesh networks constructed using the Commotion Wireless (https://commotionwireless.net/) firmware.

**THIS IS AN ALPHA RELEASE!**
Do *NOT* expect Byzantium to be perfect. Some features are not ready yet, others need work. Things are going to break in weird ways and we need to know what those ways are so we can fix them. Please, for the love of LOLcats, do not deploy Byzantium in situations where lives are at stake.

**THIS IS ALSO A VERY SPECIAL RELEASE!**
We built this release in the days following Hurricane Sandy and deployed it in New York City. We streamlined it a lot - the web-based control panel was removed; so were the network applications.  This release was designed to be lean and mean, providing only the bare essentials (namely, the ability to set up a wireless mesh network, and the ability to connect that mesh to the global Net). Those applications will return in a later release in a very different form.  This version was designed to require as little user interaction as possible and will automatically configure itself without assistance. Once Byzantium Linux is online it is a fully operational mesh node.

#### FEATURES:
- Binary compatible with Slackware-CURRENT. Existing Slackware packages can be converted with a single command.
- Can act as a gateway to the Internet if a link is available (via Ethernet or tethered smartphone).
- Linux kernel v3.3.4
- Drivers for dozens of wireless chipsets
- KDE Trinity R14.0.0
- LXDE (2010 release of all components)
- Mplayer
- GCC v4.5.2
- Perl v5.14
- Python v2.7.3
- Firefox v13.0.1
- X.org

#### SYSTEM REQUIREMENTS:
##### To Use
- Minimum of 1GB of RAM (512MB without copy2ram boot option)
- i586 CPU or better
- CD- or DVD-ROM drive
- BIOS must boot removable media
- At least one (1) 802.11 a/b/g/n interface

#### SYSTEM REQUIREMENTS:
##### For Persistent Changes
- The above requirements to use Byzantium
- 2+GB of free space on thumbdrive or harddrive

#### WHAT WE NEED:
- Developers.
- Developers!
- DEVELOPERS!
- No more Bill Ballmer impersonations.
- People testing Byzantium to find bugs.
- People reporting bugs on our Github page (https://github.com/Byzantium/Byzantium/issues). We can't fix what we don't know about!
- Patches.
- People booting Byzantium and setting up small meshes (2-5 clients) to tell us how well it works for you with your hardware. We have a hardware compatibility list on our wiki that needs to be expanded.
- Help translating the user interface. We especially need people fluent in dialects of Chinese, Arabic, Farsi, and Urdu.
- Help us write and translate documentation.

#### LINKS:
- [Homepage](http://project-byzantium.org/)
- [Download sites](http://project-byzantium.org/download/)

*This announcement is published under a Creative Commons By Attribution / Noncommercial / Share Alike v3.0 License. (http://creativecommons.org/licenses/by-nc-sa/3.0/)*
