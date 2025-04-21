#include <iostream>

#include <openssl/ssl.h>
#include <openssl/err.h>

int main() {
    // loading the method
    const SSL_METHOD *method = TLS_method();
    // configuration inforgmation for tls
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (ctx == NULL) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    // TODO add exiting messages for server


    return 0;
}