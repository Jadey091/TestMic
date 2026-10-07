#ifndef BONGO_CAT_CONFIG_INTERNAL_H
#define BONGO_CAT_CONFIG_INTERNAL_H

#include "bongo_cat/config.h"
#include <yyjson.h>

#define BONGO_CAT_CONFIG_VERSION 2

yyjson_doc *bongo_cat_config_read_document(const char *path,
    const char *format, BongoCatError *error);
BongoCatResult bongo_cat_config_write_document(const char *path,
    yyjson_mut_doc *document, const char *description, BongoCatError *error);

#endif
