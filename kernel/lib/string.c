#include <string.h>

void* memcpy(void* dest, const void* src, uint32_t len) {
	uint8_t* ptrd = (uint8_t*) dest;
	const uint8_t* ptrs = (uint8_t*) src;
	for (; len != 0; len--)
		*ptrd++ = *ptrs++;
	// note what we're returning, dumbass
	return dest;
}

void* memset(void* dest, int val, uint32_t len) {
	uint8_t* ptr = (uint8_t*) dest;
	for (; len != 0; len--)
		*ptr++ = val;
	return dest;
}

int32_t memcmp(const void* s1, const void* s2, size_t n) {
	const uint8_t* us1 = (const uint8_t*) s1;
	const uint8_t* us2 = (const uint8_t*) s2;
	while (n-- != 0) {
		if (*us1 != *us2)
			return (*us1 < *us2) ? -1 : +1;
		us1++;
		us2++;
	}
	return 0;
}

int strcmp(const char* str1, const char* str2) {
	for (; *str1 == *str2; ++str1, ++str2)
		if (*str1 == 0)
			return 0;
	return *(const uint8_t *) str1 - *(const uint8_t *) str2;
}

char* strcpy(char* dest, const char* src) {
	char* ptr = dest;
	do
		*ptr++ = *src++;
	while (*src != 0);
	return dest;
}

char* strcat(char* dest, const char* src) {
	while (*dest != 0)
		dest++;
	do
		*dest++ = *src++;
	while (*src != 0);
	return dest;
}

int strlen(char* str) {
	const char* ptr = str;
	for (; *ptr; ++ptr)
		;
	return (ptr - str);
}
