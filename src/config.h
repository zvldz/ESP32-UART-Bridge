#ifndef CONFIG_H
#define CONFIG_H

#include "types.h"

// Configuration management interface
void config_init(Config* config);
void config_load(Config* config);
void config_save(Config* config);

#endif // CONFIG_H