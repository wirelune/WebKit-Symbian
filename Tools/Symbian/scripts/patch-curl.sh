#!/usr/bin/env bash
# The port's changes to curl 7.88.1, for the phone (build-deps.sh) and the
# Windows test host (host/build-net.ps1): patch-curl.sh CURL_SOURCE_DIR. Each
# change is checked; running it twice changes nothing.
set -euo pipefail
dir=${1:?usage: patch-curl.sh CURL_SOURCE_DIR}
# The CA bundle is read by shims/mbedtls-fs.c, the SDK's mbedTLS having no
# file functions (no MBEDTLS_FS_IO); both builds define SYMBIAN_MBEDTLS_FS.
vtls=$dir/lib/vtls/mbedtls.c
grep -q 'mbedtls-fs.h' "$vtls" || sed -i -e 's/^#ifdef MBEDTLS_FS_IO$/#if defined(MBEDTLS_FS_IO) || defined(SYMBIAN_MBEDTLS_FS)/' \
  -e 's|^#include <mbedtls/sha256.h>$|&\n#ifdef SYMBIAN_MBEDTLS_FS\n#include "mbedtls-fs.h"\n#endif|' "$vtls"
grep -q 'mbedtls-fs.h' "$vtls"
# The SDK's mbedTLS verifies signatures through PSA (MBEDTLS_USE_PSA_CRYPTO),
# which curl 7.88 never initializes: every handshake failed with
# MBEDTLS_ERR_PK_BAD_INPUT_DATA (PSA_ERROR_BAD_STATE).
grep -q 'psa_crypto_init' "$vtls" || perl -0pi -e 's{(static int mbedtls_init\(void\)\n\{\n)}{$1#ifdef MBEDTLS_USE_PSA_CRYPTO\n  if(psa_crypto_init() != PSA_SUCCESS)\n    return 0;\n#endif\n}; s{(#include <mbedtls/sha256.h>\n)}{$1#ifdef MBEDTLS_USE_PSA_CRYPTO\n#include <psa/crypto.h>\n#endif\n}' "$vtls"
grep -q 'psa_crypto_init' "$vtls"
# Every connection uses one parsed copy of the CA bundle (shims/mbedtls-fs.c)
# instead of parsing its own.
grep -q 'mbedtls_fs_shared_ca_chain' "$vtls" || perl -0pi -e 's{(  mbedtls_x509_crt_init\(&backend->cacert\);\n)}{$1  mbedtls_x509_crt *cachain = &backend->cacert;\n}; s{mbedtls_x509_crt_parse_file\(&backend->cacert, ssl_cafile\)}{mbedtls_fs_shared_ca_chain(ssl_cafile, &cachain)}; s{(mbedtls_ssl_conf_ca_chain\(&backend->config,\n\s*)&backend->cacert,}{$1cachain,}' "$vtls"
[ "$(grep -c 'cachain' "$vtls")" = 3 ]
# TLS 1.3, which the SDK's mbedTLS has (MBEDTLS_SSL_PROTO_TLS1_3) and curl
# 7.88 never asks mbedTLS for: the highest version becomes 1.3, and reads go
# on past the session tickets a TLS 1.3 server sends after the handshake.
grep -q 'MBEDTLS_SSL_MINOR_VERSION_4' "$vtls" || perl -0pi -e 's{(    case CURL_SSLVERSION_TLSv1_3:\n)(      break;\n)}{$1#ifdef MBEDTLS_SSL_PROTO_TLS1_3\n      *mbedver = MBEDTLS_SSL_MINOR_VERSION_4;\n      return CURLE_OK;\n#endif\n$2}g; s{(      ssl_version_max = CURL_SSLVERSION_MAX_TLSv1_)2;\n}{#ifdef MBEDTLS_SSL_PROTO_TLS1_3\n${1}3;\n#else\n${1}2;\n#endif\n}; s{(  ret = mbedtls_ssl_read\(&backend->ssl, \(unsigned char \*\)buf,\n\s*buffersize\);\n)}{  do {\n  $1  }\n#ifdef MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET\n  while(ret == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET);\n#else\n  while(0);\n#endif\n}' "$vtls"
[ "$(grep -c 'MBEDTLS_SSL_MINOR_VERSION_4' "$vtls")" = 2 ]
grep -q 'CURL_SSLVERSION_MAX_TLSv1_3;' "$vtls"
grep -q 'while(ret == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET)' "$vtls"
# file: URLs name drive letters (file:///C:/Data/page.html), as on Windows.
sed -i 's/^#if defined(WIN32) || defined(MSDOS) || defined(__EMX__)$/& || defined(__SYMBIAN32__)/' \
  "$dir/lib/file.c"
sed -i 's/^#if !defined(MSDOS) \&\& !defined(WIN32) \&\& !defined(__CYGWIN__)$/& \&\& !defined(__SYMBIAN32__)/' \
  "$dir/lib/urlapi.c"
