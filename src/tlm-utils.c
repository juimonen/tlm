#include "tlm-utils.h"

void
g_clear_string (gchar **str)
{
    if (str && *str) {
        g_free (*str);
        str = NULL;
    }
}

const gchar *
tlm_user_get_home_dir (const gchar *username)
{
    (void) username;
    return "/home/avalluri";
}

const gchar *
tlm_user_get_shell (const gchar *username)
{
    (void) username;
    return "/bin/bash";
}
