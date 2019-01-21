#pragma once

int string_from_wide(wchar_t* from, char* to, int to_len);

template<int to_len>
int string_from_wide(wchar_t* from, char (&to)[to_len]) {
	return string_from_wide(from, to, to_len);
}

int string_to_wide(char* from, wchar_t* to, int to_len);

template<int to_len>
int string_to_wide(char* from, wchar_t (&to)[to_len]) {
	return string_to_wide(from, to, to_len);
}

void memzero(void* dst, size_t size);

template<typename T>
void memzero(T* dst) {
	memzero(dst, sizeof(T));
}