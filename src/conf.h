#ifndef BL_CONF_H
#define BL_CONF_H

// Config file entries
#define CONF_LIB_DIR_KEY "/lib_dir"
#define CONF_VERSION     "/version"
#define CONF_FILEPATH    "@filepath"

struct config;

struct config *confload(const char *filepath);
void           confdelete(struct config *conf);
const char    *confreads(struct config *conf, const char *path, const char *default_value);

#endif
