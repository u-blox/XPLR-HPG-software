![Thingstream](./../media/shared/logos/thingstream_logo.jpg)

<br>
<br>

# Thingstream - Creating a device profile for Point Perfect

## Description 
A device profile is used in order to be able to use **Zero Touch Provisioning (ZTP)** in combination with **Point Perfect**.\
A device profile can be used then to **[get the needed settings](./README_thingstream_ztp.md)** to perform a ZTP request

## Creating the profile
### Steps
1. Go to **Location Services -> Device Profiles**
2. Click on **Create Profile** located in the top right corner of the window.
3. Input a **Device Profile Name**
4. Select **Device Type** and chose **PointPerfect**
5. Enable **Auto Activate Devices** by checking the box on the left.
6. Chose a **Plan** by clicking on **Select a Plan** and select your desired plan.
7. Enable **Hardware Code Mandatory** by checking the box on the left.
8. Select **Action to take if hardware code already exists** and select **Return existing device details**
9. Click on **Create** to finish your device profile creation

<br>

### Video
Follow the video below for a more detailed explanation of the steps above:\
[![Thingstream - Creating a device profile for Point Perfect](./../media/shared/misc/vid_rsz.jpg)](https://youtu.be/N8m14aViwg8)

<br>

### Further reading
You can also have a look at this **[guide](https://developer.thingstream.io/guides/location-services/pointperfect-zero-touch-provisioning#h.oikkx2rxauu1)** on the official **Thingstream** documentation site.

<br>
<br>

## Caveats
You must first have an account and services subscription on **Thingstream** before start using the services described.