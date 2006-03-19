//
// Copyright (C) 2004-2006 SIPfoundry Inc.
// Licensed by SIPfoundry under the LGPL license.
//
// Copyright (C) 2004-2006 Pingtel Corp.  All rights reserved.
// Licensed to SIPfoundry under a Contributor Agreement.
//
// $$
///////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_RESPARSE_NS_NAME_H
#define INCLUDE_RESPARSE_NS_NAME_H
#ifdef __cplusplus
extern "C" {
#endif

int ns_name_uncompress(const u_char *msg, const u_char *eom, const u_char *src, char *dst, size_t dstsiz);
int ns_name_compress(const char *src, u_char *dst, size_t dstsiz, const u_char **dnptrs, const u_char **lastdnptr);
int ns_name_skip(const u_char **ptrptr, const u_char *eom);

#ifdef __cplusplus
}
#endif
#endif /* INCLUDE_RESPARSE_NS_NAME_H */
