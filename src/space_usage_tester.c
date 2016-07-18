
#include <stdio.h>

#include <globus_ftp_control.h>
#include <globus_error.h>

#define PrintError(result) \
  { \
    int _generic_msg = 1; \
    globus_object_t *_err_obj = globus_error_peek(result); \
    if (_err_obj && (_err_obj != GLOBUS_ERROR_NO_INFO)) \
    { \
      char * _msg = globus_error_print_friendly(_err_obj); \
      if (_msg) { \
        fprintf(stderr, "%s\n", _msg); \
        free(_msg); \
        _generic_msg = 0; \
      } \
    } \
    if (_generic_msg) { \
      fprintf(stderr, "Unknown error;\n"); \
    } \
  }

#define PrintErrorObj(_err_obj) \
  { \
    char * _msg = globus_error_print_friendly(_err_obj); \
    if (_msg) { \
      fprintf(stderr, "%s\n", _msg); \
      free(_msg); \
    } \
  }

#define GlobusErrorGeneric(reason)                                     \
    globus_error_put(GlobusErrorObjGeneric(reason))                               

#define GlobusErrorObjGeneric(reason)                                  \
    globus_error_construct_error(                                      \
        GLOBUS_NULL,                                                   \
        GLOBUS_NULL,                                                   \
        1,                                                             \
        __FILE__,                                                      \
        "Usage Tester",                                                \
        __LINE__,                                                      \
        "%s",                                                          \
        (reason))

typedef struct space_usage_monitor_s {
  globus_result_t result;
  globus_bool_t done;
  globus_bool_t needs_quit;
  globus_mutex_t mutex;
  globus_cond_t cond;
  gss_cred_id_t cred;
  char * subject;
  char * space;
  gss_name_t name;
  globus_ftp_control_auth_info_t auth;
} space_usage_monitor_t;


void
quit_callback(
    void *                                      arg,
    globus_ftp_control_handle_t *               handle,
    globus_object_t *                           error,
    globus_ftp_control_response_t *             resp)
{
    space_usage_monitor_t *monitor = (space_usage_monitor_t*)arg;

    globus_mutex_lock(&monitor->mutex);
    {
        monitor->result = error ? globus_error_put(error) : GLOBUS_SUCCESS;
        if (error) {fprintf(stderr, "Failed to quit\n");}
        monitor->done = GLOBUS_TRUE;
        globus_cond_signal(&monitor->cond);
    }
    globus_mutex_unlock(&monitor->mutex);
    return;
}


void
usage_response_callback(
    void *                                      arg,
    globus_ftp_control_handle_t *               handle,
    globus_object_t *                           error,
    globus_ftp_control_response_t *             resp)
{
    space_usage_monitor_t *monitor = (space_usage_monitor_t*)arg;

    if (resp == GLOBUS_NULL)
    {
        globus_mutex_lock(&monitor->mutex);
        {
            monitor->result = error ? globus_error_put(error) : GLOBUS_FAILURE;
            monitor->done = GLOBUS_TRUE;
            fprintf(stderr, "Failed to get response callback\n");
            globus_cond_signal(&monitor->cond);
        }
        globus_mutex_unlock(&monitor->mutex);
        return;
    }

    if (resp->code != 250)
    {
        globus_mutex_lock(&monitor->mutex);
        {
            monitor->result = GlobusErrorGeneric(resp->response_buffer);
            monitor->done = GLOBUS_TRUE;
            globus_cond_signal(&monitor->cond);
        }
        globus_mutex_unlock(&monitor->mutex);
        return;
    }

    printf("Response: %s", resp->response_buffer);

    globus_mutex_lock(&monitor->mutex);
    {
        monitor->result = GLOBUS_SUCCESS;
        monitor->done = GLOBUS_TRUE;
        globus_cond_signal(&monitor->cond);
    }
}

void
authenticate_callback(void *arg,
                      globus_ftp_control_handle_t *handle,
                      globus_object_t *err,
                      globus_ftp_control_response_t *resp)
{
    space_usage_monitor_t *monitor = (space_usage_monitor_t*)arg;

    if (resp == GLOBUS_NULL)
    {
        globus_mutex_lock(&monitor->mutex);
        {
            monitor->result = err ? globus_error_put(err) : GlobusErrorGeneric("Failed to authenticate");
            monitor->done = GLOBUS_TRUE;
            globus_cond_signal(&monitor->cond);
        }
        globus_mutex_unlock(&monitor->mutex);
        return;
    }
    if (resp->code != 230)
    {
        globus_mutex_lock(&monitor->mutex);
        {
            monitor->result = GlobusErrorGeneric("Authentication failed.");
            monitor->done = GLOBUS_TRUE;
            globus_cond_signal(&monitor->cond);
        }
        globus_mutex_unlock(&monitor->mutex);
        return;
    }

    globus_result_t result = globus_ftp_control_send_command(handle,
        "SITE USAGE %s\r\n",
        usage_response_callback,
        monitor,
        monitor->space);
    if (result != GLOBUS_SUCCESS)
    {
        globus_mutex_lock(&monitor->mutex);
        {
            monitor->result = result;
            monitor->done = GLOBUS_TRUE;
            globus_cond_signal(&monitor->cond);
        }
        globus_mutex_unlock(&monitor->mutex);
        return;
    }
}


void
connect_callback(void *arg,
                 globus_ftp_control_handle_t *handle,
                 globus_object_t *err,
                 globus_ftp_control_response_t *resp)
{
    space_usage_monitor_t *monitor = (space_usage_monitor_t*)arg;
    if (resp == GLOBUS_NULL)
    {
        globus_mutex_lock(&monitor->mutex);
        {
            monitor->result = GlobusErrorGeneric("Connection to server failed");
            monitor->done = GLOBUS_TRUE;
            fprintf(stderr, "Null response to connection.\n");
            PrintErrorObj(err)
            globus_cond_signal(&monitor->cond);
        }
        globus_mutex_unlock(&monitor->mutex);
        return;
    }
    printf("Login message: %s", resp->response_buffer);

    globus_mutex_lock(&monitor->mutex);
    {
        monitor->needs_quit = GLOBUS_TRUE;
    }
    globus_mutex_unlock(&monitor->mutex);

    if (resp->code != 220)
    {
        globus_mutex_lock(&monitor->mutex);
        {
            monitor->result = GLOBUS_FAILURE;
            monitor->done = GLOBUS_TRUE;
            globus_cond_signal(&monitor->cond);
        }
        globus_mutex_unlock(&monitor->mutex);
        return;
    }

    globus_result_t result = globus_ftp_control_auth_info_init(
                         &monitor->auth,
                         monitor->cred,
                         GLOBUS_FALSE,
                         NULL,
                         NULL,
                         NULL,
                         NULL);
    if (result != GLOBUS_SUCCESS)
    {
        globus_mutex_lock(&monitor->mutex);
        {
            monitor->result = result;
            monitor->done = GLOBUS_TRUE;
            globus_cond_signal(&monitor->cond);
        }
        globus_mutex_unlock(&monitor->mutex);
        return;
    }

    result = globus_ftp_control_authenticate(
                     handle,
                     &monitor->auth,
                     GLOBUS_TRUE,
                     authenticate_callback,
                     monitor);
    if (result != GLOBUS_SUCCESS)
    {
        globus_mutex_lock(&monitor->mutex);
        {
            monitor->result = result;
            monitor->done = GLOBUS_TRUE;
            globus_cond_signal(&monitor->cond);
        }
        globus_mutex_unlock(&monitor->mutex);
        return;
    }
}


int
main(int argc, char *argv[])
{
    space_usage_monitor_t monitor;
    monitor.done = GLOBUS_FALSE;
    monitor.needs_quit = GLOBUS_FALSE;
    monitor.result = GLOBUS_FAILURE;
    globus_mutex_init(&monitor.mutex, GLOBUS_NULL);
    globus_cond_init(&monitor.cond, GLOBUS_NULL);
    memset(&monitor.auth, '\0', sizeof(monitor.auth));
    monitor.cred = GSS_C_NO_CREDENTIAL;
    monitor.subject = NULL;

    globus_ftp_control_handle_t handle;
    globus_result_t result = GLOBUS_FAILURE;
    OM_uint32 maj, min;

    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s hostname space\n", argv[0]);
        goto fail_args;
    }
    monitor.space = argv[2];

    result = globus_module_activate(GLOBUS_FTP_CONTROL_MODULE);
    if (result != GLOBUS_SUCCESS)
    {
        PrintError(result)
        goto fail_activate;
    }

    {
        gss_buffer_desc buffer;

        maj = gss_acquire_cred(
                    &min,
                    GSS_C_NO_NAME,
                    0,
                    GSS_C_NO_OID_SET,
                    GSS_C_BOTH,
                    &monitor.cred,
                    NULL,
                    NULL);
        if (maj != GSS_S_COMPLETE)
        {
            fprintf(stderr, "Failed to acquire credential.\n");
            goto fail_cred;
        }

        maj = gss_inquire_cred(&min,
                    monitor.cred,
                    &monitor.name,
                    NULL,
                    NULL,
                    NULL);
        if (maj != GSS_S_COMPLETE)
        {
            fprintf(stderr, "Failed to inquire credential.\n");
            goto fail_cred;
        }

        maj = gss_display_name(
                    &min,
                    monitor.name,
                    &buffer,
                    NULL);
        if (maj != GSS_S_COMPLETE)
        {
            fprintf(stderr, "Failed to get credential display name.\n");
            goto fail_cred;
        }

        monitor.subject = buffer.value;
    }

    result = globus_ftp_control_handle_init(&handle);
    if (result != GLOBUS_SUCCESS)
    {
        PrintError(result)
        goto fail_init;
    }

    result = globus_ftp_control_connect(&handle, argv[1], 2811, connect_callback, &monitor);
    if (result != GLOBUS_SUCCESS)
    {
        PrintError(result)
        goto fail_connect;
    }

    globus_mutex_lock(&monitor.mutex);
    {
        fprintf(stderr, "Waiting for connection\n");
        while(!monitor.done)
        {
            globus_cond_wait(&monitor.cond, &monitor.mutex);
        }
    }
    globus_mutex_unlock(&monitor.mutex);

    fprintf(stderr, "Usage query completed.\n");

    result = monitor.result;
    if (result != GLOBUS_SUCCESS)
    {
        PrintError(result)
    }

    if (monitor.needs_quit)
    {
        monitor.done = GLOBUS_FALSE;
        result = globus_ftp_control_quit(
            &handle,
            quit_callback,
            &monitor
        );
        if (result == GLOBUS_SUCCESS)
        {
            globus_mutex_lock(&monitor.mutex);
            {
                fprintf(stderr, "Waiting for quit\n");
                while(!monitor.done)
                {
                    globus_cond_wait(&monitor.cond, &monitor.mutex);
                }
            }
            globus_mutex_unlock(&monitor.mutex);
            fprintf(stderr, "Quit command sent.\n");
        }
    }

fail_connect:
    globus_ftp_control_handle_destroy(&handle);
fail_init:
    gss_release_cred(&min, &monitor.cred);
    gss_release_name(&min, &monitor.name);
fail_cred:
fail_activate:
    if (monitor.subject) {free(monitor.subject);}
    globus_mutex_destroy(&monitor.mutex);
    globus_cond_destroy(&monitor.cond);
    globus_module_deactivate(GLOBUS_FTP_CONTROL_MODULE);
fail_args:
    return (result == GLOBUS_SUCCESS) ? 0 : 1;
}

