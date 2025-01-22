#ifndef NVS_COMPONENT_H_
#define NVS_COMPONENT_H_
#include "freertos/task.h"
#include "esp_system.h"

/* Save value in NVS
   Return an error if anything goes wrong
   during this process.
 */
esp_err_t set_version(char *value);

/* Read from NVS and print version.
   The version is saved in arg, you must free it
   Return a NULL if anything goes wrong
   during this process.
 */
char *get_version(void);

/* Read from NVS and compare ther arg version.
   return 0 if are equal in other cases
 */
uint8_t get_compared_version(char *version);

#endif