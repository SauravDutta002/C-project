/*
 * storage.h - Persist the station to disk and export reports
 */
#ifndef IO_STORAGE_H
#define IO_STORAGE_H

#include "core/station.h"

int storage_save(const Station *st, const char *path);
int storage_load(Station *st, const char *path);
int storage_export_board(Station *st, const char *path);   /* returns rows written, -1 on error */

#endif /* IO_STORAGE_H */
