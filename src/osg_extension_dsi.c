
#include "globus_gridftp_server.h"
#include "version.h"

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

    original_init_function(op, session);
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


