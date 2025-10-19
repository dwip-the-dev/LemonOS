#ifndef SYSTEM_INFO_H
#define SYSTEM_INFO_H

void system_info_command(void);
void get_cpu_vendor(char* vendor);
void get_cpu_brand(char* brand);
unsigned int get_conventional_memory(void);
unsigned int get_extended_memory(void);

#endif
