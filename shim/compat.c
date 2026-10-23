/* Only linked by build-surya.sh (clang): clang lowers __builtin_strcmp to a
 * libcall where GCC inlines it, and the shim has no libc. */
int strcmp(const char *a, const char *b);
int strcmp(const char *a, const char *b)
{
	while (*a && *a == *b) {
		a++;
		b++;
	}
	return (unsigned char)*a - (unsigned char)*b;
}

/* clang emits memset for zero-initialised local arrays (surya_touch.c). */
void *memset(void *d, int c, unsigned long n);
void *memset(void *d, int c, unsigned long n)
{
	volatile unsigned char *p = d;	/* volatile: keep clang from calling memset here */
	while (n--)
		*p++ = (unsigned char)c;
	return d;
}
