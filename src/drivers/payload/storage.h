#ifndef PAYLOAD_STORAGE_H
#define PAYLOAD_STORAGE_H

/* Initialize SDMMC2 in four-bit mode, mount drive 0, and run the original
 * directory, license-file read, and CSAT.TXT write diagnostics. */
void payload_storage_init(void);

#endif
