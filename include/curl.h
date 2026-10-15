#ifndef CURL_H
#define CURL_H

#include <stdint.h>

typedef void (*curl_output_fn)(const char* line, uint32_t color);

int curl_cli(int argc, char** argv, curl_output_fn out_fn);
int curl_main(int argc, char** argv);

#endif // CURL_H
