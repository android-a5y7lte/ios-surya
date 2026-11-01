#!/bin/sh
# Share the PC's internet with the phone over the USB Ethernet link (sweetusb=2).
#
#   tools/surya-usb-internet.sh
#
# Run while tools/sweet-usbnet.sh has the link up (PC 192.168.7.1, phone 192.168.7.2).
# PC: IPv4 forwarding + NAT from 192.168.7.0/24 to the default-route interface.
# Phone (over SSH): default route via 192.168.7.1, using route from Elucubratus
# network-cmds copied to /var/root/bin (see SURYA_NOTES.md "Internet over USB").
# Temporary: gone after a reboot of the PC or the phone; run it again then.
set -e
IF=$(ip -o -4 addr show | awk '/192\.168\.7\.1\//{print $2; exit}')
WAN=$(ip route show default | awk '{print $5; exit}')
[ -n "$IF" ] || { echo "no interface with 192.168.7.1 (is tools/sweet-usbnet.sh running?)" >&2; exit 1; }
[ -n "$WAN" ] || { echo "the PC has no default route" >&2; exit 1; }
echo "usb=$IF internet=$WAN"
sudo sysctl -q -w net.ipv4.ip_forward=1
sudo iptables -t nat -C POSTROUTING -s 192.168.7.0/24 -o "$WAN" -j MASQUERADE 2>/dev/null ||
	sudo iptables -t nat -A POSTROUTING -s 192.168.7.0/24 -o "$WAN" -j MASQUERADE
sudo iptables -C FORWARD -i "$IF" -o "$WAN" -j ACCEPT 2>/dev/null ||
	sudo iptables -A FORWARD -i "$IF" -o "$WAN" -j ACCEPT
sudo iptables -C FORWARD -i "$WAN" -o "$IF" -m state --state RELATED,ESTABLISHED -j ACCEPT 2>/dev/null ||
	sudo iptables -A FORWARD -i "$WAN" -o "$IF" -m state --state RELATED,ESTABLISHED -j ACCEPT
ssh root@192.168.7.2 '/var/root/bin/route -n add default 192.168.7.1 2>&1 | tail -1;
	exec 3<>/dev/tcp/1.1.1.1/80 && echo "phone: TCP to 1.1.1.1 works"; exec 3>&-'
