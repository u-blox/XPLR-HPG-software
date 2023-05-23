![Thingstream](./../media/shared/logos/thingstream_logo.jpg)

<br>
<br>

# Thingstream - Getting certificates and key

## Description 
Certificates are used when we want to manually provide the required settings for an MQTT service.\
In this case we have to get our settings from our **Location Things** list by finding the device we are interested in.
There are 3 required files:
1. **Client Key**
2. **Client Certificate**
3. **Root Certificate**\
   The **Root Certificate** is accessible by anyone, there's no need to keep it secret.

## Getting the certificates and key
### Steps
1. Login to your account.
2. On the left navigation menu go to **Location Services** and then **Location Things**.\
   If you do not have any location things check **[this guide](./README_thingstream_create_locthing.md)** on how to create one.
3. Scroll through the list or use the search bar at the top to find your device.
4. Click on the desired device.
5. On the pop up from the right go to **Credentials**.
6. Under **Credentials** download the following 3 files:
   - **Client Key**
   - **Client Certificate**
   - **Root Certificate**
7. Use your files as required.

<br>

### Video
Follow the video below for a more detailed explanation of the steps above:\
[![Thingstream - Getting certificates](./../media/shared/misc/vid_rsz.jpg)](https://youtu.be/7nhpz09wYqE)

<br>
<br>

## Caveats
Please try to keep your **Client Key** and **Client Certificate** secret and do not in any circumstance share it with anybody else.




