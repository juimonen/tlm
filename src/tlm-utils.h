#ifndef _TLM_UTILS_H
#define _TLM_UTILS_H

#include <glib.h>

G_BEGIN_DECLS

void g_clear_string (gchar **);

const gchar * tlm_user_get_home_dir (const gchar *username);

const gchar * tlm_user_get_shell (const gchar *username);

G_END_DECLS

#endif /* _TLM_UTILS_H */
