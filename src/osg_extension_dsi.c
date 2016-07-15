
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


static void
osg_command(
    globus_gfs_operation_t              op,
    globus_gfs_command_info_t *         cmd_info,
    void *                              user_arg)
{
    original_command_function(op, cmd_info, user_arg);
}

static int
osg_activate(void)
{
    GlobusGFSName(osg_activate);
    globus_result_t result = GLOBUS_SUCCESS;

    memset(&osg_dsi_iface, '\0', sizeof(globus_gfs_storage_iface_t));

    // Code adapted from globus_i_gfs_data.c in Globus Toolkit.
    void *new_dsi = (globus_gfs_storage_iface_t *) globus_extension_lookup(
        &osg_dsi_handle, GLOBUS_GFS_DSI_REGISTRY, "file");
    if (new_dsi == NULL)
    {
        result = globus_extension_activate("globus_gridftp_server_file");
        if (result != GLOBUS_SUCCESS)
        {
            result = GlobusGFSErrorWrapFailed("DSI activation", result);
            return result;
        }
    }
    new_dsi = (globus_gfs_storage_iface_t *) globus_extension_lookup(
        &osg_dsi_handle, GLOBUS_GFS_DSI_REGISTRY, "file");
    if(new_dsi == NULL)
    {
        GlobusGFSErrorGenericStr(result,
            ("DSI 'file' is not available in the module."));
        return result;
    }

    memcpy(&osg_dsi_iface, new_dsi, sizeof(globus_gfs_storage_iface_t));
    original_command_function = osg_dsi_iface.command_func;
    osg_dsi_iface.command_func = osg_command;

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


