/*
 * suryablur: turn on the accessibility settings that let the UI draw without a
 * GPU, so the blurred "materials" (App Library background, folders, the dock, an
 * app opening over the home screen) stop falling back to plain white and become
 * solid fills the CPU can draw.
 *
 *   suryablur [0|1]        default 1 (on); 0 turns the settings back off
 *
 * Calls libAccessibility's own setters (what Settings does), so the choice is
 * kept in its preferences and picked up by SpringBoard and apps:
 *   _AXSSetEnhanceBackgroundContrastEnabled  (Settings' "Increase Contrast";
 *       HakiOS uses this one to remove the blurs - the GPU spends most of an
 *       app opening on them)
 *   _AXSSetReduceTransparencyEnabled         (Settings' "Reduce Transparency",
 *       when the library has it)
 * Then respring (kill SpringBoard; launchd restarts it) for it to take hold.
 * Needs root. Built on Linux (tools/suryablur/build.sh), ad-hoc signed.
 */
typedef unsigned char Boolean;
#define NULL ((void *)0)
#define RTLD_NOW 2
typedef struct __sFILE FILE;
extern FILE *__stderrp;
extern int fprintf(FILE *, const char *, ...);
extern int printf(const char *, ...);
extern void *dlopen(const char *, int);
extern void *dlsym(void *, const char *);

int
main(int argc, char **argv)
{
	Boolean on = (argc < 2) || (argv[1][0] != '0');
	void *lib = dlopen("/usr/lib/libAccessibility.dylib", RTLD_NOW);
	if (lib == NULL) {
		fprintf(__stderrp, "suryablur: cannot open libAccessibility.dylib\n");
		return 1;
	}
	static const char *const setters[] = {
		"_AXSSetEnhanceBackgroundContrastEnabled",
		"_AXSSetReduceTransparencyEnabled",
	};
	int done = 0;
	for (unsigned i = 0; i < sizeof(setters) / sizeof(setters[0]); i++) {
		void (*set)(Boolean) = (void (*)(Boolean))dlsym(lib, setters[i]);
		if (set == NULL) {
			printf("suryablur: %s not in this libAccessibility (skipped)\n", setters[i]);
			continue;
		}
		set(on);
		printf("suryablur: %s(%d)\n", setters[i], on ? 1 : 0);
		done++;
	}
	if (done == 0) {
		fprintf(__stderrp, "suryablur: no setters found; nothing changed\n");
		return 1;
	}
	printf("suryablur: done (%s). Respring for it to take hold: kill SpringBoard.\n",
	    on ? "blurs -> solid fills" : "back to blurs");
	return 0;
}
