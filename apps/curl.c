// curl HTTP Transfer Tool for LemonOS
// Author: Dwip (dwip-the-dev) <dwip@dwip.dedyn.io>

#include "curl.h"
#include "net.h"
#include "filesystem.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"

static void vga_curl_printer(const char* line, uint32_t color) {
    (void)color;
    printf("%s\n", line);
}

int curl_cli(int argc, char** argv, curl_output_fn out_fn) {
    if (!out_fn) out_fn = vga_curl_printer;

    if (argc < 2) {
        out_fn("curl: try 'curl --help' for more information", 0xFFE0AF68);
        return 2;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        out_fn("Usage: curl [options...] <url>", 0xFFF9E2AF);
        out_fn("Options:", 0xFF6C7086);
        out_fn(" -d, --data <data>          HTTP POST data", 0xFFCDD6F4);
        out_fn(" -i, --include              Include protocol response headers in output", 0xFFCDD6F4);
        out_fn(" -o, --output <file>        Write to file instead of stdout", 0xFFCDD6F4);
        out_fn(" -s, --silent               Silent mode", 0xFFCDD6F4);
        out_fn(" -v, --verbose              Make the operation more talkative", 0xFFCDD6F4);
        out_fn(" -X, --request <command>    Specify request command to use (GET, POST)", 0xFFCDD6F4);
        return 0;
    }

    int include_headers = 0;
    int silent = 0;
    int verbose = 0;
    const char* output_file = NULL;
    const char* method = "GET";
    const char* post_data = NULL;
    const char* url = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--include") == 0) {
            include_headers = 1;
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--silent") == 0) {
            silent = 1;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else if ((strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) && i + 1 < argc) {
            output_file = argv[++i];
        } else if ((strcmp(argv[i], "-X") == 0 || strcmp(argv[i], "--request") == 0) && i + 1 < argc) {
            method = argv[++i];
        } else if ((strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--data") == 0) && i + 1 < argc) {
            post_data = argv[++i];
            method = "POST";
        } else if (argv[i][0] != '-') {
            url = argv[i];
        }
    }

    if (!url) {
        out_fn("curl: no URL specified!", 0xFFF38BA8);
        return 2;
    }

    static char url_norm[256];
    if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) {
        strcpy(url_norm, "http://");
        strncat(url_norm, url, sizeof(url_norm) - 8);
        url_norm[sizeof(url_norm) - 1] = '\0';
        url = url_norm;
    }

    if (verbose) {
        char msg[256];
        strcpy(msg, "* Requesting: "); strcat(msg, url);
        out_fn(msg, 0xFF89B4FA);
        strcpy(msg, "> "); strcat(msg, method); strcat(msg, " HTTP/1.1");
        out_fn(msg, 0xFFCDD6F4);
        out_fn("> User-Agent: curl/8.5.0 (LemonOS Baremetal i386)", 0xFFCDD6F4);
        out_fn("> Accept: */*", 0xFFCDD6F4);
        if (post_data) {
            strcpy(msg, "> Content-Length: ");
            char len_s[16]; int lp = 0, lv = (int)strlen(post_data);
            if (lv == 0) len_s[lp++] = '0';
            while (lv > 0) { len_s[lp++] = '0' + (lv % 10); lv /= 10; }
            for (int k = lp - 1; k >= 0; k--) msg[strlen(msg)] = len_s[k];
            msg[strlen(msg)] = '\0';
            out_fn(msg, 0xFFCDD6F4);
            out_fn("> Content-Type: application/x-www-form-urlencoded", 0xFFCDD6F4);
        }
        out_fn(">", 0xFFCDD6F4);
    }

    static char headers_buf[2048];
    static char body_buf[16384];
    headers_buf[0] = '\0';
    body_buf[0] = '\0';
    int status_code = 0;
    int bytes = 0;

    if (strcmp(method, "POST") == 0) {
        bytes = net_http_post(url, "application/x-www-form-urlencoded", post_data ? post_data : "", post_data ? strlen(post_data) : 0, body_buf, sizeof(body_buf), &status_code);
    } else {
        bytes = net_http_get_full(url, headers_buf, sizeof(headers_buf), body_buf, sizeof(body_buf), &status_code);
    }

    if (bytes <= 0 && status_code == 0) {
        char err[128];
        strcpy(err, "curl: (7) Failed to connect to host: Connection timed out or host unreachable");
        out_fn(err, 0xFFF38BA8);
        return 7;
    }

    if (verbose || include_headers) {
        if (headers_buf[0]) {
            char line[256]; int lp = 0;
            for (int c = 0; headers_buf[c]; c++) {
                if (headers_buf[c] == '\n' || lp >= 250) {
                    line[lp] = '\0';
                    char hline[280];
                    strcpy(hline, "< "); strcat(hline, line);
                    out_fn(hline, 0xFF89B4FA);
                    lp = 0;
                } else if (headers_buf[c] != '\r') {
                    line[lp++] = headers_buf[c];
                }
            }
            if (lp > 0) {
                line[lp] = '\0';
                char hline[280];
                strcpy(hline, "< "); strcat(hline, line);
                out_fn(hline, 0xFF89B4FA);
            }
            out_fn("< ", 0xFF89B4FA);
        } else {
            char s_msg[64];
            strcpy(s_msg, "< HTTP/1.1 ");
            char code_s[8]; int cp = 0, cv = status_code;
            if (cv == 0) code_s[cp++] = '0';
            while (cv > 0) { code_s[cp++] = '0' + (cv % 10); cv /= 10; }
            for (int k = cp - 1; k >= 0; k--) s_msg[strlen(s_msg)] = code_s[k];
            s_msg[strlen(s_msg)] = '\0';
            strcat(s_msg, (status_code == 200) ? " OK" : ((status_code == 404) ? " Not Found" : " Response"));
            out_fn(s_msg, (status_code >= 200 && status_code < 300) ? 0xFFA6E3A1 : 0xFFF38BA8);
        }
    }

    if (output_file) {
        fs_create_file(output_file, body_buf);
        if (!silent) {
            char ok[128];
            strcpy(ok, "curl: Saved ");
            char b_s[16]; int bp = 0, bv = bytes;
            if (bv == 0) b_s[bp++] = '0';
            while (bv > 0) { b_s[bp++] = '0' + (bv % 10); bv /= 10; }
            for (int k = bp - 1; k >= 0; k--) ok[strlen(ok)] = b_s[k];
            ok[strlen(ok)] = '\0';
            strcat(ok, " bytes to '"); strcat(ok, output_file); strcat(ok, "'");
            out_fn(ok, 0xFFA6E3A1);
        }
    } else {
        char line[256]; int lp = 0;
        for (int c = 0; body_buf[c]; c++) {
            if (body_buf[c] == '\n' || lp >= 250) {
                line[lp] = '\0';
                out_fn(line, 0xFFCDD6F4);
                lp = 0;
            } else if (body_buf[c] != '\r') {
                line[lp++] = body_buf[c];
            }
        }
        if (lp > 0) {
            line[lp] = '\0';
            out_fn(line, 0xFFCDD6F4);
        }
    }

    return (status_code >= 200 && status_code < 400) ? 0 : 22;
}

int curl_main(int argc, char** argv) {
    return curl_cli(argc, argv, vga_curl_printer);
}
