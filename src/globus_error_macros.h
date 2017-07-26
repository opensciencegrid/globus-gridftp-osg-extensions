
// Messages should not end with newline
#define MESSAGE_BUFFER_SIZE 1024

#define SomeError(user, host, msg) \
    char * formatted_msg = (char *)globus_malloc(MESSAGE_BUFFER_SIZE); \
    snprintf(formatted_msg, MESSAGE_BUFFER_SIZE, "%s (host=%s, user=%s)", msg, host, user); \
    formatted_msg[MESSAGE_BUFFER_SIZE-1] = '\0'; \
    globus_gfs_log_message(GLOBUS_GFS_LOG_ERR, "%s\n", formatted_msg);


#define GenericError(user, host, msg, rc) \
    SomeError(user, host, msg) \
    rc = GlobusGFSErrorGeneric(formatted_msg); \
    globus_free(formatted_msg);


#define SystemError(user, host, msg, rc) \
    SomeError(user, host, msg) \
    rc = GlobusGFSErrorSystemError(formatted_msg, errno); \
    globus_free(formatted_msg);


#define MemoryError(user, host, msg, rc) \
    SomeError(user, host, msg) \
    rc = GlobusGFSErrorMemory(formatted_msg); \
    globus_free(formatted_msg);

