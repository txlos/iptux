
#pragma once

#include <Winsock2.h>
#include <Ws2tcpip.h>

#undef ERROR
#undef CreateWindow

#include <locale>
#include <iostream>
#include <algorithm>
#include <io.h>

//int inet_pton(int af, const char *src, void *dst);
//extern void close(int s);

#define SHUT_RDWR SD_BOTH
//#define F_OK 00
#define O_LARGEFILE 0


using namespace std;

// templated version of my_equal so it could work with both char and wchar_t
template<typename charT>
struct my_equal {
  my_equal(const std::locale& loc) : loc_(loc) {}
  bool operator()(charT ch1, charT ch2) {
    return std::toupper(ch1, loc_) == std::toupper(ch2, loc_);
  }
private:
  const std::locale& loc_;
};

// find substring (case insensitive)
template<typename T>
int ci_find_substr(const T& str1, const T& str2, const std::locale& loc = std::locale())
{
  typename T::const_iterator it = std::search(str1.begin(), str1.end(),
    str2.begin(), str2.end(), my_equal<typename T::value_type>(loc));
  if (it != str1.end()) return it - str1.begin();
  else return -1; // not found
}

char *strcasestr(const char *haystack, const char *needle);

static void PlatformInit() { }
