#!/bin/bash
# Phone side, run by launchd at boot (job com.surya.netboot, installed by
# tools/surya-netboot-install.py): default route over the USB link and the
# DNS servers published to configd (suryanet). Retries for up to 5 minutes,
# because configd and the USB link come up at different times. Log: /var/root/netboot.log
exec >>/var/root/netboot.log 2>&1
echo "$(date) surya-netboot start"
for i in $(seq 1 60); do
	/var/root/bin/route -n add default 192.168.7.1 >/dev/null 2>&1
	if /var/root/bin/route -n get default 2>/dev/null | grep -q 192.168.7.1 &&
	   /var/root/bin/suryanet >/dev/null 2>&1; then
		echo "$(date) route + DNS set (try $i)"
		exit 0
	fi
	sleep 5
done
echo "$(date) gave up after 60 tries"
exit 1
