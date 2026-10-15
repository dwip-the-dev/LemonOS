#ifndef SPEEDTEST_H
#define SPEEDTEST_H

#include <stdint.h>
#include <stddef.h>

// Output callback function pointer
typedef void (*speedtest_out_fn)(const char* line, uint32_t color);

// Speedtest results structure
typedef struct {
    uint32_t ping_ms_x100;       // e.g. 72 = 0.72 ms
    uint32_t jitter_ms_x100;     // e.g. 12 = 0.12 ms
    uint32_t download_mbps_x100; // e.g. 54210 = 542.10 Mbps
    uint32_t upload_mbps_x100;   // e.g. 48750 = 487.50 Mbps
    uint32_t bytes_transferred;  // Total bytes benchmarked
    uint32_t test_duration_ms;   // Total duration in ms
    char     isp[64];
    char     server[64];
    char     client_ip[32];
    char     result_url[80];
} speedtest_result_t;

// Live measurement APIs
int speedtest_measure_live(uint32_t* out_ping_x100, uint32_t* out_jitter_x100, uint32_t* out_dl_x100, uint32_t* out_ul_x100);

// CLI and Core functions
int speedtest_run(speedtest_out_fn out_fn, speedtest_result_t* out_res, int simple_mode, int bytes_mode, int json_mode);
int speedtest_cli(int argc, char** argv, speedtest_out_fn out_fn);
int speedtest_main(int argc, char** argv);

#endif
