# Microcontroller-TCPIP-stack
A size-optimised stack for (primarily Atmel/Arduino) microcontrollers with UDP and TCP, and zero configuration networking.

Uses DHCP to obtain an IP address, or APIPA or a static IP address.

Is discoverable via zero-configuration protocols (LLMNR, mDNS).  

Supports ARP, limited ICMP (PING) and IP, UDP and TCP.  At the application layer, is a NTP client, a DNS client, and a HTTP client or server. 

Is capable of sending TCP packets larger than the 2k microcontroller memory (e.g. by constructing both the boilerplate and possibly the payload on the fly)
Is capable of receiving TCP packets also larger than the microcontroller memory by incrementally downloading them from the network card (which has typically 8k)

Functionality can be included/excluded at compile time to trade code size for capability
