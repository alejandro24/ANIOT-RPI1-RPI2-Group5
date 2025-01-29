#ifndef THINGSBOARD_TYPES_H
#define THINGSBOARD_TYPES_H
#include <stddef.h>
#include <stdint.h>

typedef struct {
    char* uri;
    uint16_t port;
} thingsboard_address_t;
typedef struct {
    char* certificate;
    size_t certificate_len;
} thingsboard_verification_t;
typedef struct {
    char* certificate;
    size_t certificate_len;
    char* key;
    size_t key_len;
} thingsboard_authentication_t;
typedef struct {
    thingsboard_authentication_t authentication;
} thingsboard_credentials_t;
typedef struct {
    thingsboard_address_t address;
    thingsboard_verification_t verification;
    thingsboard_credentials_t credentials;
} thingsboard_cfg_t;

#endif /* !THINGSBOARD_TYPES_H*/
