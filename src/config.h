#ifndef CONFIG_H
#define CONFIG_H

#include "types.h"

// Current configuration version
#define CURRENT_CONFIG_VERSION 2

// Configuration management interface
void config_init(Config* config);
void config_load(Config* config);
void config_save(Config* config);
void config_migrate(Config* config);

#endif // CONFIG_H