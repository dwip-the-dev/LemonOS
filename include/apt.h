#ifndef APT_H
#define APT_H

#include <stdint.h>
#include <stddef.h>

// Universal APT package database capacity (supports hundreds of packages)
#define APT_MAX_PACKAGES 512

typedef struct {
    char name[32];
    char version[16];
    char architecture[12];
    char section[24];
    char description[80];
    char depends[64];
    uint32_t size_kb;
    char md5[33];
    char sha256[65];
    char source[128];
    char lem_filename[48];
    int  installed;
} apt_pkg_t;

typedef void (*apt_output_fn)(const char* line, uint32_t color);

void apt_init_subsystem(void);
void apt_set_output_handler(apt_output_fn fn);
void apt_reset_output_handler(void);
void apt_print(const char* text, uint32_t color);

int  apt_update(void);
int  apt_list(int only_installed, char* out_buf, size_t max);
int  apt_search(const char* term, char* out_buf, size_t max);
int  apt_show(const char* pkg_name, char* out_buf, size_t max);
int  apt_install(const char* pkg_name);
int  apt_remove(const char* pkg_name);
int  apt_upgrade(void);
int  apt_clean(void);
int  apt_is_installed(const char* pkg_name);
int  apt_pkg_exists(const char* pkg_name);

// Universal package resolution & query API
apt_pkg_t* apt_find_pkg(const char* pkg_name);
apt_pkg_t* apt_resolve_pkg(const char* pkg_name);
int        apt_pkg_count(void);
apt_pkg_t* apt_get_pkg(int index);

// Lemon Package (.lem) Management API
int  lem_install_file(const char* lem_path);
int  lem_install_pkg(const char* pkg_name);
int  lem_pull_github(const char* url_or_pkg);
int  lem_build_pkg(const char* name, const char* version, const char* bin_path, const char* desc, const char* out_path);
int  lem_cli(int argc, char** argv);
int  lem_repo_add(const char* url);
int  lem_repo_list(char* out_buf, size_t max);
int  dpkg_install_lem(const char* lem_path);

int  dpkg_list(char* out_buf, size_t max);
int  dpkg_status(const char* pkg_name, char* out_buf, size_t max);
int  dpkg_list_files(const char* pkg_name, char* out_buf, size_t max);
int  dpkg_install_deb(const char* deb_path);

int  apt_cli(int argc, char** argv);
int  dpkg_cli(int argc, char** argv);

// Universal execution for any installed tool, package, compiler, or script
int  apt_run_tool(const char* name, const char* arg, apt_output_fn out_fn);

// Verification tests
void apt_run_tests(void);

#endif
