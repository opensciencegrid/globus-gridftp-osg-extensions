
#include "globus_gridftp_server.h"
#include "version.h"
#include "gridftp_hdfs_error.h"

#ifdef VOMS_FOUND
#include "voms_apic.h"
#endif  // VOMS_FOUND

#include <string.h>

static int osg_activate(void);
static int osg_deactivate(void);

// From globus_i_gridftp_server.h
#define GlobusGFSErrorGenericStr(_res, _fmt)                           \
do                                                                     \
{                                                                      \
        char *                          _tmp_str;                      \
        _tmp_str = globus_common_create_string _fmt;                   \
        _res = globus_error_put(                                       \
            globus_error_construct_error(                              \
                GLOBUS_NULL,                                           \
                GLOBUS_NULL,                                           \
                GLOBUS_GFS_ERROR_GENERIC,                              \
                __FILE__,                                              \
                _gfs_name,                                             \
                __LINE__,                                              \
                "%s",                                                  \
                _tmp_str));                                            \
        globus_free(_tmp_str);                                         \
                                                                       \
} while(0)

// from gridftp_hdfs.h
typedef struct globus_l_gfs_hdfs_handle_s
{
    char *                              pathname;
    char *                              username;
    char *                              local_host;  // Our local hostname.
} globus_l_gfs_hdfs_handle_t;
typedef globus_l_gfs_hdfs_handle_t hdfs_handle_t;

globus_result_t
check_connection_limits(const hdfs_handle_t *hdfs_handle,
    int user_transfer_limit, int transfer_limit);

static void
get_connection_limits_params(const hdfs_handle_t **hdfs_handle_p,
    int *user_transfer_limit_p, int *transfer_limit_p,
    globus_gfs_operation_t op, const globus_gfs_session_info_t *session_info);

static globus_version_t osg_local_version =
{
    OSG_EXTENSIONS_VERSION_MAJOR, /* major version number */
    OSG_EXTENSIONS_VERSION_MINOR, /* minor/bug version number */
    OSG_EXTENSIONS_TIMESTAMP,
    0 /* branch ID */
};


GlobusExtensionDefineModule(globus_gridftp_server_osg) =
{
    "globus_gridftp_server_osg",
    osg_activate,
    osg_deactivate,
    NULL,
    NULL,
    &osg_local_version
};


// Most functions will be populated from the underlying 'file' DSI
static globus_gfs_storage_iface_t osg_dsi_iface;

static globus_extension_handle_t osg_dsi_handle = NULL;

static globus_gfs_storage_command_t original_command_function = NULL;
static globus_gfs_storage_init_t original_init_function = NULL;

enum {
	GLOBUS_GFS_OSG_CMD_SITE_USAGE = GLOBUS_GFS_MIN_CUSTOM_CMD,
};


static void
osg_extensions_init(globus_gfs_operation_t op, globus_gfs_session_info_t * session)
{
    GlobusGFSName(osg_extensions_init);

    globus_result_t result = globus_gridftp_server_add_command(op, "SITE USAGE",
                                 GLOBUS_GFS_OSG_CMD_SITE_USAGE,
                                 3,
                                 5,
                                 "SITE USAGE <sp> [TOKEN <sp> $name] <sp> $location: Get usage information for a location.",
                                 GLOBUS_FALSE,
                                 GFS_ACL_ACTION_LOOKUP);

    if (result != GLOBUS_SUCCESS)
    {
        result = GlobusGFSErrorWrapFailed("Failed to add custom 'SITE USAGE' command", result);
        globus_gridftp_server_finished_session_start(op,
                                                 result,
                                                 NULL,
                                                 NULL,
                                                 NULL);
        return;
    }

#ifdef VOMS_FOUND

    struct vomsdata *vdata = VOMS_Init(NULL, NULL);
    if (vdata)
    {
        int error;
        if (!VOMS_RetrieveFromCred(session->del_cred, RECURSE_CHAIN, vdata, &error))
        {
            globus_gfs_log_message(GLOBUS_GFS_LOG_TRANSFER, "No VOMS info in credential.\n");
        }
        else
        {
            struct voms *vext;
            int idx;
            for (idx = 0; vdata->data[idx] != NULL; idx++)
            {
                char msg[1024];
                char *pos = msg;
                int char_remaining = 1022;
                vext = vdata->data[idx];
                int this_round;
                if ((char_remaining > 0) && vext->voname)
                {
                    this_round = snprintf(pos, char_remaining, "VO %s ", vext->voname);
                    pos += this_round;
                    char_remaining -= this_round;
                }
                char *fqan;
                int count = 0;
                int idx2 = 0;
                for (idx2 = 0; vext->fqan[idx2] != NULL; idx2++)
                {
                    fqan = vext->fqan[idx2];
                    if (char_remaining > 0)
                    {
                        count ++;
                        this_round = snprintf(pos, char_remaining, "%s,", fqan);
                        pos += this_round;
                        char_remaining -= this_round;
                    }
                }
                if (count && char_remaining >= 0) {pos--;}
                if (char_remaining >= 0)
                {
                    *pos = '\n';
                    *(pos+1) = '\0';
                }
                else
                {
                    msg[1023] = '\0';
                    msg[1022] = '\n';
                }
                globus_gfs_log_message(GLOBUS_GFS_LOG_TRANSFER, msg);
            }
        }
        VOMS_Destroy(vdata);
    }

#endif  // VOMS_FOUND

    const hdfs_handle_t *hdfs_handle;
    int user_transfer_limit;
    int transfer_limit;
    get_connection_limits_params(&hdfs_handle, &user_transfer_limit,
            &transfer_limit, op, session);

    check_connection_limits(hdfs_handle, user_transfer_limit, transfer_limit);

    original_init_function(op, session);
}

void
get_connection_limits_params(
        const hdfs_handle_t **hdfs_handle_p,
        int *user_transfer_limit_p,
        int *transfer_limit_p,
        globus_gfs_operation_t op,
        const globus_gfs_session_info_t *session_info)
{
    hdfs_handle_t* hdfs_handle;
    globus_gfs_finished_info_t finished_info;
    GlobusGFSName(hdfs_start);
    globus_result_t rc;

    int user_transfer_limit = -1;
    int transfer_limit = -1;

    hdfs_handle = (hdfs_handle_t *)globus_malloc(sizeof(hdfs_handle_t));
    memset(hdfs_handle, 0, sizeof(hdfs_handle_t));

    memset(&finished_info, 0, sizeof(globus_gfs_finished_info_t));
    finished_info.type = GLOBUS_GFS_OP_SESSION_START;
    finished_info.result = GLOBUS_SUCCESS;
    finished_info.info.session.session_arg = hdfs_handle;
    finished_info.info.session.username = session_info->username;
    finished_info.info.session.home_dir = "/";

    if (!hdfs_handle) {
        MemoryError(hdfs_handle, "Unable to allocate a new HDFS handle.", rc);
        finished_info.result = rc;
        globus_gridftp_server_operation_finished(op, rc, &finished_info);
        return;
    }


    // Copy the username from the session_info to the HDFS handle.
    size_t strlength = strlen(session_info->username)+1;
    strlength = strlength < 256 ? strlength  : 256;
    hdfs_handle->username = globus_malloc(sizeof(char)*strlength);
    if (hdfs_handle->username == NULL) {
        //gridftp_user_name[0] = '\0';
        finished_info.result = GLOBUS_FAILURE;
        globus_gridftp_server_operation_finished(
            op, GLOBUS_FAILURE, &finished_info);
        return;
    }
    strncpy(hdfs_handle->username, session_info->username, strlength);

    // also copy username to global variable gridftp_user_name
    //strncpy(gridftp_user_name, session_info->username, strlength);


    // Pull configuration from environment.

    char * global_transfer_limit_char = getenv("GRIDFTP_TRANSFER_LIMIT");
    char * default_user_limit_char = getenv("GRIDFTP_DEFAULT_USER_TRANSFER_LIMIT");

    char specific_limit_env_var[256];

    snprintf(specific_limit_env_var, 255, "GRIDFTP_%s_USER_TRANSFER_LIMIT", hdfs_handle->username);
    specific_limit_env_var[255] = '\0';
    int idx;
    for (idx=0; idx<256; idx++) {
        if (specific_limit_env_var[idx] == '\0') {break;}
        specific_limit_env_var[idx] = toupper(specific_limit_env_var[idx]);
    }
    char * specific_user_limit_char = getenv(specific_limit_env_var);

    if (!specific_user_limit_char) {
        specific_user_limit_char = default_user_limit_char;
    }
    if (specific_user_limit_char) {
        user_transfer_limit = atoi(specific_user_limit_char);
    }
    if (global_transfer_limit_char) {
        transfer_limit = atoi(global_transfer_limit_char);
    }

    // Get our hostname
    hdfs_handle->local_host = globus_malloc(256);
    if (hdfs_handle->local_host) {
        memset(hdfs_handle->local_host, 0, 256);
        if (gethostname(hdfs_handle->local_host, 255)) {
            strcpy(hdfs_handle->local_host, "UNKNOWN");
        }
    }

    *hdfs_handle_p = hdfs_handle;
    *user_transfer_limit_p = user_transfer_limit;
    *transfer_limit_p = transfer_limit;
}


/*************************************************************************
 * check_connection_limits
 * -----------------------
 * Make sure the number of concurrent connections to HDFS is below a certain
 * threshold.  If we are over-threshold, wait for a fixed amount of time (1 
 * minute) and fail the transfer.
 * Implementation baed on named POSIX semaphores.
 *************************************************************************/
globus_result_t
check_connection_limits(const hdfs_handle_t *hdfs_handle, int user_transfer_limit, int transfer_limit)
{
    GlobusGFSName(check_connection_limit);
    globus_result_t result = GLOBUS_SUCCESS;

    int user_lock_count = 0;
    if (user_transfer_limit > 0) {
        char user_sem_name[256];
        snprintf(user_sem_name, 255, "/dev/shm/gridftp-hdfs-%s-%d", hdfs_handle->username, user_transfer_limit);
        user_sem_name[255] = '\0';
        int usem = dumb_sem_open(user_sem_name, O_CREAT, 0600, user_transfer_limit);
        if (usem == -1) {
            SystemError(hdfs_handle, "Failure when determining user connection limit", result);
            return result;
        }
        if (-1 == (user_lock_count = dumb_sem_timedwait(usem, user_transfer_limit, 60))) {
            if (errno == ETIMEDOUT) {
                globus_gfs_log_message(GLOBUS_GFS_LOG_INFO, "Failing transfer for %s due to user connection limit of %d.\n", hdfs_handle->username, user_transfer_limit);
                char * failure_msg = (char *)globus_malloc(1024);
                snprintf(failure_msg, 1024, "Server over the user connection limit of %d", user_transfer_limit);
                failure_msg[1023] = '\0';
                GenericError(hdfs_handle, failure_msg, result);
                globus_free(failure_msg);
            } else {
                SystemError(hdfs_handle, "Failed to check user connection semaphore", result);
            }
            return result;
        }
        // NOTE: We now purposely leak the semaphore.  It will be automatically closed when
        // the server process finishes this connection.
    }

    int global_lock_count = 0;
    if (transfer_limit > 0) {
        char global_sem_name[256];
        snprintf(global_sem_name, 255, "/dev/shm//gridftp-hdfs-overall-%d", transfer_limit);
        global_sem_name[255] = '\0';
        int gsem = dumb_sem_open(global_sem_name, O_CREAT, 0666, transfer_limit);
        if (gsem == -1) {
            SystemError(hdfs_handle, "Failure when determining global connection limit", result);
            return result;
        }
        if (-1 == (global_lock_count=dumb_sem_timedwait(gsem, transfer_limit, 60))) {
            if (errno == ETIMEDOUT) {
                globus_gfs_log_message(GLOBUS_GFS_LOG_INFO, "Failing transfer for %s due to global connection limit of %d (user has %d transfers).\n", hdfs_handle->username, transfer_limit, user_lock_count);
                char * failure_msg = (char *)globus_malloc(1024);
                snprintf(failure_msg, 1024, "Server over the global connection limit of %d (user has %d transfers)", transfer_limit, user_lock_count);
                failure_msg[1023] = '\0';
                GenericError(hdfs_handle, failure_msg, result);
                globus_free(failure_msg);
            } else {
                SystemError(hdfs_handle, "Failed to check global connection semaphore", result);
            }
            return result;
        }
        // NOTE: We now purposely leak the semaphore.  It will be automatically closed when
        // the server process finishes this connection.
    }
    if ((transfer_limit > 0) || (user_transfer_limit > 0)) {
        globus_gfs_log_message(GLOBUS_GFS_LOG_INFO, "Proceeding with transfer; user %s has %d active transfers (limit %d); server has %d active transfers (limit %d).\n", hdfs_handle->username, user_lock_count, user_transfer_limit, global_lock_count, transfer_limit);
    }

    return result;
}

int
dumb_sem_open(const char *fname, int flags, mode_t mode, int value) {
    int fd = open(fname, flags | O_RDWR, mode);
    if (-1 == fd) {
        return fd;
    }
    if (-1 == posix_fallocate(fd, 0, value)) {
        return -1;
    }
    fchmod(fd, mode);
    return fd;
}
int
dumb_sem_timedwait(int fd, int value, int secs) {
    struct timespec start, now, sleeptime;
    clock_gettime(CLOCK_MONOTONIC, &start);
    sleeptime.tv_sec = 0;
    sleeptime.tv_nsec = 500*1e6;
    while (1) {
        int idx = 0;
        int lock_count = 0;
        int need_lock = 1;
        for (idx=0; idx<value; idx++) {
            struct flock mylock; memset(&mylock, '\0', sizeof(mylock));
            mylock.l_type = F_WRLCK;
            mylock.l_whence = SEEK_SET;
            mylock.l_start = idx;
            mylock.l_len = 1;
            if (0 == fcntl(fd, need_lock ? F_SETLK : F_GETLK, &mylock)) {
                if (need_lock) {  // We now have the lock.
                    need_lock = 0;
                    lock_count++;
                } else if (mylock.l_type != F_UNLCK) {  // We're just seeing how many locks are taken.
                    lock_count++;
                }
                continue;
            }
            if (errno == EAGAIN || errno == EACCES || errno == EINTR) {
                lock_count++;
                continue;
            }
            return -1;
        }
        if (!need_lock) {  // we were able to take a lock.
            return lock_count;
        }
        nanosleep(&sleeptime, NULL);
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec > start.tv_sec + secs) {
            errno = ETIMEDOUT;
            return -1;
        }
    }
}

static void
site_usage(globus_gfs_operation_t op,
           globus_gfs_command_info_t *cmd_info)
{
    GlobusGFSName(site_usage);

    int argc = 0;
    char **argv;

    globus_result_t result = globus_gridftp_server_query_op_info(
        op,
        cmd_info->op_info,
        GLOBUS_GFS_OP_INFO_CMD_ARGS,
        &argv,
        &argc
        );
    if (result != GLOBUS_SUCCESS)
    {
        result = GlobusGFSErrorGeneric("Incorrect invocation of SITE USAGE command");
        globus_gridftp_server_finished_command(op, result, "550 Incorrect invocation of SITE USAGE.\r\n");
        return;
    }
    if ((argc != 3) && (argc != 5))
    {
        result = GlobusGFSErrorGeneric("Incorrect number of arguments to SITE USAGE command");
        globus_gridftp_server_finished_command(op, result, "550 Incorrect number of arguments to SITE USAGE command.\r\n");
        return;
    }
    const char *token_name = "default";
    if ((argc == 5) && strcasecmp("TOKEN", argv[2]))
    {
        result = GlobusGFSErrorGeneric("Incorrect format for SITE USAGE command");
        globus_gridftp_server_finished_command(op, result, "550 Expected format: SITE USAGE [TOKEN name] path.\r\n");
        return;
    }
    if (argc == 5) {
        token_name = argv[3];
    }

    const char *script_pathname = getenv("OSG_SITE_USAGE_SCRIPT");
    if (!script_pathname)
    {
        result = GlobusGFSErrorGeneric("Site usage script not configured");
        globus_gridftp_server_finished_command(op, result, "550 Server is not configured to provide site usage.\r\n");
        return;
    }
    char cmd[256];
    snprintf(cmd, 256, "%s %s %s", script_pathname, token_name, cmd_info->pathname);
    cmd[255] = '\0';
    FILE *fp = popen(cmd, "r");
    if (fp == NULL) {
        result = GlobusGFSErrorSystemError("usage script", errno);
        globus_gridftp_server_finished_command(op, result, "550 Server failed to start usage query.\r\n");
        return;
    }
    char output[1024];
    while (fgets(output, 1024, fp) != NULL) {}

    int status = pclose(fp);
    if ((status == -1) || (status > 0))
    {
        if (status == -1) {result = GlobusGFSErrorSystemError("Usage script failed", errno);}
        else {result = GlobusGFSErrorGeneric("Site usage script failed");}
        globus_gridftp_server_finished_command(op, result, "550 Server usage query failed.\r\n");
        return;
    }
    char *newline_char = strchr(output, '\n');
    if (newline_char) {*newline_char = '\0';}

    long long usage, free, total;
    int output_count = sscanf(output, "%lld %lld %lld", &usage, &free, &total);
    if (output_count < 2)
    {
        result = GlobusGFSErrorGeneric("Invalid output from site usage script");
        globus_gridftp_server_finished_command(op, result, "550 Invalid output from site usage script.\r\n");
        return;
    }
    if (output_count == 2) {total = usage + free;}

    char final_output[1024];
    snprintf(final_output, 1024, "250 USAGE %lld FREE %lld TOTAL %lld\r\n", usage, free, total);
    final_output[1023] = '\0';
    globus_gridftp_server_finished_command(op, result, final_output);
}

static void
osg_command(
    globus_gfs_operation_t              op,
    globus_gfs_command_info_t *         cmd_info,
    void *                              user_arg)
{
    switch (cmd_info->command)
    {
    case GLOBUS_GFS_OSG_CMD_SITE_USAGE:
        site_usage(op, cmd_info);
        return;
    default:
        // Anything not explicitly OSG-centric is passed to the
        // underlying module.
        break;
    }

    original_command_function(op, cmd_info, user_arg);
}


static int
osg_activate(void)
{
    GlobusGFSName(osg_activate);
    globus_result_t result = GLOBUS_SUCCESS;

    memset(&osg_dsi_iface, '\0', sizeof(globus_gfs_storage_iface_t));

    char * dsi_name = getenv("OSG_EXTENSIONS_OVERRIDE_DSI");
    dsi_name = dsi_name ? dsi_name : "file";

    // Code adapted from globus_i_gfs_data.c in Globus Toolkit.
    void *new_dsi = (globus_gfs_storage_iface_t *) globus_extension_lookup(
        &osg_dsi_handle, GLOBUS_GFS_DSI_REGISTRY, dsi_name);
    if (new_dsi == NULL)
    {
        char module_name[1024];
        snprintf(module_name, 1024, "globus_gridftp_server_%s", dsi_name);
        module_name[1023] = '\0';
        result = globus_extension_activate(module_name);
        if (result != GLOBUS_SUCCESS)
        {
            result = GlobusGFSErrorWrapFailed("DSI activation", result);
            return result;
        }
    }
    new_dsi = (globus_gfs_storage_iface_t *) globus_extension_lookup(
        &osg_dsi_handle, GLOBUS_GFS_DSI_REGISTRY, dsi_name);
    if(new_dsi == NULL)
    {
        GlobusGFSErrorGenericStr(result,
            ("DSI '%s' is not available in the module.", dsi_name));
        return result;
    }

    memcpy(&osg_dsi_iface, new_dsi, sizeof(globus_gfs_storage_iface_t));
    original_command_function = osg_dsi_iface.command_func;
    original_init_function = osg_dsi_iface.init_func;
    osg_dsi_iface.command_func = osg_command;
    osg_dsi_iface.init_func = osg_extensions_init;

    globus_extension_registry_add(
        GLOBUS_GFS_DSI_REGISTRY,
        "osg",
        GlobusExtensionMyModule(globus_gridftp_server_osg),
        &osg_dsi_iface);
    
    return result;
}


static int
osg_deactivate(void)
{
    globus_extension_registry_remove(
        GLOBUS_GFS_DSI_REGISTRY, "osg");

    globus_extension_release(osg_dsi_handle);
}


