#ifndef THINGSBOARD_TYPES_H
#define THINGSBOARD_TYPES_H
#include <stdint.h>

typedef char thingsboard_url_t[100];
typedef struct {
    char* uri;
    uint8_t port;
} thingsboard_address_t;
typedef struct {
    char* certificate;
} thingsboard_verification_t;
typedef struct {
    char* certificate;
    char* key;
} thingsboard_authentication_t;
typedef struct {
    thingsboard_authentication_t authentication;
} thingsboard_credentials_t;
typedef struct {
    thingsboard_address_t address;
    thingsboard_verification_t verification;
    thingsboard_credentials_t credentials;
} thingsboard_cgf_t;

#endif // !THINGSBOARD_TYPES_H
