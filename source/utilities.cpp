#include "utilities.hpp"

int string_from_wide(wchar_t* from, char* to, int to_len) {
	return WideCharToMultiByte(CP_UTF8, 0, from, (int)wcslen(from), to, to_len, 0, 0);
}

int string_to_wide(char* from, wchar_t* to, int to_len) {
	return MultiByteToWideChar(CP_UTF8, 0, from, (int)strlen(from), to, to_len);
}

void memzero(void* dst, size_t size) {
	memset(dst, 0, size);
}
