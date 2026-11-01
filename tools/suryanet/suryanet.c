/*
 * suryanet: publish the USB Ethernet link (ue0, sweetusb=2) to configd, so iOS
 * resolves names (mDNSResponder takes its DNS servers from configd) and has a
 * primary IPv4 service. What `scutil` would do; iOS ships no scutil.
 *
 *   suryanet [DNS ...]        (default DNS 1.1.1.1 8.8.8.8)
 *
 * Writes State:/Network/Service/surya-usb/IPv4 {Addresses 192.168.7.2,
 * SubnetMasks 255.255.255.0, Router 192.168.7.1, InterfaceName ue0} and
 * .../DNS {ServerAddresses}. Needs root. Values live until configd restarts
 * (reboot). Built on Linux with tools/suryanet/build.sh (no SDK: the few CF/SC
 * declarations are below, the libraries are .tbd stubs).
 */
/* No SDK headers: the libc pieces used here. */
typedef unsigned int uint32_t;
#define NULL ((void *)0)
typedef struct __sFILE FILE;
extern FILE *__stderrp;
#define stderr __stderrp
extern int printf(const char *, ...);
extern int fprintf(FILE *, const char *, ...);

typedef const void *CFTypeRef;
typedef const struct __CFString *CFStringRef;
typedef const struct __CFDictionary *CFDictionaryRef;
typedef const struct __CFArray *CFArrayRef;
typedef const struct __CFAllocator *CFAllocatorRef;
typedef struct __SCDynamicStore *SCDynamicStoreRef;
typedef long CFIndex;
typedef uint32_t CFStringEncoding;
typedef unsigned char Boolean;
typedef struct { CFIndex v; void *a, *b, *c, *d, *e; } CFDictionaryKeyCallBacks;
typedef struct { CFIndex v; void *a, *b, *c, *d; } CFDictionaryValueCallBacks;
typedef struct { CFIndex v; void *a, *b, *c, *d; } CFArrayCallBacks;

extern const CFDictionaryKeyCallBacks kCFTypeDictionaryKeyCallBacks;
extern const CFDictionaryValueCallBacks kCFTypeDictionaryValueCallBacks;
extern const CFArrayCallBacks kCFTypeArrayCallBacks;
extern CFStringRef CFStringCreateWithCString(CFAllocatorRef, const char *, CFStringEncoding);
extern CFArrayRef CFArrayCreate(CFAllocatorRef, const void **, CFIndex, const CFArrayCallBacks *);
extern CFDictionaryRef CFDictionaryCreate(CFAllocatorRef, const void **, const void **, CFIndex,
    const CFDictionaryKeyCallBacks *, const CFDictionaryValueCallBacks *);
extern void CFRelease(CFTypeRef);
extern void CFShow(CFTypeRef);
extern SCDynamicStoreRef SCDynamicStoreCreate(CFAllocatorRef, CFStringRef, void *, void *);
extern Boolean SCDynamicStoreSetValue(SCDynamicStoreRef, CFStringRef, CFTypeRef);
extern CFTypeRef SCDynamicStoreCopyValue(SCDynamicStoreRef, CFStringRef);
extern int SCError(void);
extern const char *SCErrorString(int);

#define UTF8 0x08000100u

static CFStringRef s(const char *c) { return CFStringCreateWithCString(NULL, c, UTF8); }

static CFArrayRef
list(const char **items, int n)
{
	const void *v[16];
	for (int i = 0; i < n && i < 16; i++) v[i] = s(items[i]);
	return CFArrayCreate(NULL, v, n < 16 ? n : 16, &kCFTypeArrayCallBacks);
}

static int
set(SCDynamicStoreRef st, const char *key, const void **k, const void **v, int n)
{
	CFDictionaryRef d = CFDictionaryCreate(NULL, k, v, n,
	    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFStringRef ks = s(key);
	if (!SCDynamicStoreSetValue(st, ks, d)) {
		fprintf(stderr, "suryanet: set %s failed: %s\n", key, SCErrorString(SCError()));
		return 1;
	}
	CFTypeRef back = SCDynamicStoreCopyValue(st, ks);
	printf("%s:\n", key);
	if (back) { CFShow(back); CFRelease(back); }
	return 0;
}

int
main(int argc, char **argv)
{
	const char *dflt[] = { "1.1.1.1", "8.8.8.8" };
	const char **dns = argc > 1 ? (const char **)argv + 1 : dflt;
	int ndns = argc > 1 ? argc - 1 : 2;
	const char *addr[] = { "192.168.7.2" }, *mask[] = { "255.255.255.0" };

	SCDynamicStoreRef st = SCDynamicStoreCreate(NULL, s("suryanet"), NULL, NULL);
	if (!st) {
		fprintf(stderr, "suryanet: no configd session: %s\n", SCErrorString(SCError()));
		return 1;
	}
	const void *k4[] = { s("Addresses"), s("SubnetMasks"), s("Router"), s("InterfaceName") };
	const void *v4[] = { list(addr, 1), list(mask, 1), s("192.168.7.1"), s("ue0") };
	const void *kd[] = { s("ServerAddresses") };
	const void *vd[] = { list(dns, ndns) };
	int err = set(st, "State:/Network/Service/surya-usb/IPv4", k4, v4, 4);
	err |= set(st, "State:/Network/Service/surya-usb/DNS", kd, vd, 1);
	return err;
}
