#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <efi.h>
#include <efilib.h>

typedef struct APP_BOOT_ENTRY_t {
    CHAR16 *Key;
    CHAR16 *File;
    CHAR16 *Title;
    CHAR16 *Icon;
    
    struct APP_BOOT_ENTRY_t *Next;
} APP_BOOT_ENTRY;

typedef struct {
    CHAR16 *DefaultFile;
    
    APP_BOOT_ENTRY *FirstBootEntry;
} APP_CONFIG;

extern APP_CONFIG GlobalConfig;

extern APP_BOOT_ENTRY* find_last_boot_entry();
extern APP_BOOT_ENTRY* find_boot_entry(CONST CHAR16 *Key);

extern EFI_STATUS load_config(void);

#endif // _CONFIG_H_
