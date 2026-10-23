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
