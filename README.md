This plane uses WiFi UDP for video transmission, which results in high latency and packet losses. Im currently looking for alternatives regarding wireless protocol or improvements to this current code.
Perhaps i could start by switching to JPEG instead of using RGB565 directly.
I could also switch to RPI 2 model B V1.1 with the RPI camera, but then i would need some kind of wireless module that is compatible with esp32.


<img width="1154" height="859" alt="image" src="https://github.com/user-attachments/assets/7eb35404-c071-42d4-bc89-e2387a4def40" />
The schematic is not 100% accurate, nor it is complete nor pretty. just a quick sketch to show the basic design

This is how the packet loss looks:
![image](https://github.com/user-attachments/assets/80a35a22-2f73-4b94-86cb-ff86767ad97c)

Those missing lines are worse and worse the faster camera is moving.
