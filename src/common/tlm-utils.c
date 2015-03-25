/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm (Tiny Login Manager)
 *
 * Copyright (C) 2013 Intel Corporation.
 *
 * Contact: Amarnath Valluri <amarnath.valluri@linux.intel.com>
 *          Jussi Laako <jussi.laako@linux.intel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <sys/types.h>
#include <pwd.h>
#include <sys/stat.h>
#include <glib/gstdio.h>
#include <utmp.h>
#include <paths.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/inotify.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <glib/gstdio.h>
#include <glib-unix.h>
#include <security/pam_appl.h>
#include <errno.h>

#include "tlm-utils.h"
#include "tlm-log.h"
#include "tlm-config.h"
#include "tlm-config-general.h"

#define HOST_NAME_SIZE 256

void
g_clear_string (gchar **str)
{
    if (str && *str) {
        g_free (*str);
        *str = NULL;
    }
}

const gchar *
tlm_user_get_name (uid_t user_id)
{
    struct passwd *pwent;

    pwent = getpwuid (user_id);
    if (!pwent)
        return NULL;

    return pwent->pw_name;
}

uid_t
tlm_user_get_uid (const gchar *username)
{
    struct passwd *pwent;

    pwent = getpwnam (username);
    if (!pwent)
        return -1;

    return pwent->pw_uid;
}

gid_t
tlm_user_get_gid (const gchar *username)
{
    struct passwd *pwent;

    pwent = getpwnam (username);
    if (!pwent)
        return -1;

    return pwent->pw_gid;
}

const gchar *
tlm_user_get_home_dir (const gchar *username)
{
    struct passwd *pwent;

    pwent = getpwnam (username);
    if (!pwent)
        return NULL;

    return pwent->pw_dir;
}

const gchar *
tlm_user_get_shell (const gchar *username)
{
    struct passwd *pwent;

    pwent = getpwnam (username);
    if (!pwent)
        return NULL;

    return pwent->pw_shell;
}

gboolean
tlm_utils_delete_dir (
        const gchar *dir)
{
    GDir* gdir = NULL;
    struct stat sent;

    if (!dir || !(gdir = g_dir_open(dir, 0, NULL))) {
        return FALSE;
    }

    const gchar *fname = NULL;
    gint retval = 0;
    gchar *filepath = NULL;
    while ((fname = g_dir_read_name (gdir)) != NULL) {
        if (g_strcmp0 (fname, ".") == 0 ||
            g_strcmp0 (fname, "..") == 0) {
            continue;
        }
        retval = -1;
        filepath = g_build_filename (dir, fname, NULL);
        if (filepath) {
            retval = lstat(filepath, &sent);
            if (retval == 0) {
                /* recurse the directory */
                if (S_ISDIR (sent.st_mode)) {
                    retval = (gint)!tlm_utils_delete_dir (filepath);
                } else {
                    retval = g_remove (filepath);
                }
            }
            g_free (filepath);
        }
        if (retval != 0) {
            g_dir_close (gdir);
            return FALSE;
        }
    }
    g_dir_close (gdir);

    if (g_remove (dir) != 0) {
        return FALSE;
    }

    return TRUE;
}

static gchar *
_get_tty_id (
        const gchar *tty_name)
{
    gchar *id = NULL;
    const gchar *tmp = tty_name;

    while (tmp) {
        if (isdigit (*tmp)) {
            id = g_strdup (tmp);
            break;
        }
        tmp++;
    }
    return id;
}

static gchar *
_get_host_address (
        const gchar *hostname)
{
    gchar *hostaddress = NULL;
    struct addrinfo hints, *info = NULL;

    if (!hostname) return NULL;

    memset (&hints, 0, sizeof (hints));
    hints.ai_flags = AI_ADDRCONFIG;

    if (getaddrinfo (hostname, NULL, &hints, &info) == 0) {
        if (info) {
            if (info->ai_family == AF_INET) {
                struct sockaddr_in *sa = (struct sockaddr_in *) info->ai_addr;
                hostaddress = g_malloc0 (sizeof(struct in_addr));
                memcpy (hostaddress, &(sa->sin_addr), sizeof (struct in_addr));
            } else if (info->ai_family == AF_INET6) {
                struct sockaddr_in6 *sa = (struct sockaddr_in6 *) info->ai_addr;
                hostaddress = g_malloc0 (sizeof(struct in6_addr));
                memcpy (hostaddress, &(sa->sin6_addr),
                        sizeof (struct in6_addr));
            }
            freeaddrinfo (info);
        }
    }
    return hostaddress;
}

static gboolean
_is_tty_same (
        const gchar *tty1_name,
        const gchar *tty2_name)
{
    gchar *tty1 = NULL, *tty2 = NULL;
    gboolean res = FALSE;

    if (tty1_name == tty2_name) return TRUE;
    if (!tty1_name || !tty2_name) return FALSE;

    if (*tty1_name == '/') tty1 = g_strdup (tty1_name);
    else tty1 = g_strdup_printf ("/dev/%s", tty1_name);
    if (*tty2_name == '/') tty2 = g_strdup (tty2_name);
    else tty2 = g_strdup_printf ("/dev/%s", tty2_name);

    res = (g_strcmp0 (tty1_name, tty2_name) == 0);

    g_free (tty1);
    g_free (tty2);
    return res;
}

static gchar *
_get_host_name ()
{
    gchar *name = g_malloc0 (HOST_NAME_SIZE);
    if (gethostname (name, HOST_NAME_SIZE) != 0) {
        g_free (name);
        return NULL;
    }
    return name;
}

void
tlm_utils_log_utmp_entry (const gchar *username)
{
    struct timeval tv;
    pid_t pid;
    struct utmp ut_ent;
    struct utmp *ut_tmp = NULL;
    gchar *hostname = NULL, *hostaddress = NULL;
    const gchar *tty_name = NULL;
    gchar *tty_no_dev_name = NULL, *tty_id = NULL;

    DBG ("Log session entry to utmp/wtmp");

    hostname = _get_host_name ();
    hostaddress = _get_host_address (hostname);
    tty_name = ttyname (0);
    if (tty_name) {
        tty_no_dev_name = g_strdup (strncmp(tty_name, "/dev/", 5) == 0 ?
            tty_name + 5 : tty_name);
    }
    tty_id = _get_tty_id (tty_no_dev_name);
    pid = getpid ();
    utmpname (_PATH_UTMP);

    setutent ();
    while ((ut_tmp = getutent())) {
        if ( (ut_tmp->ut_pid == pid) &&
             (ut_tmp->ut_id[0] != '\0') &&
             (ut_tmp->ut_type == LOGIN_PROCESS ||
                     ut_tmp->ut_type == USER_PROCESS) &&
             (_is_tty_same (ut_tmp->ut_line, tty_name))) {
            break;
        }
    }

    if (ut_tmp) memcpy (&ut_ent, ut_tmp, sizeof (ut_ent));
    else        memset (&ut_ent, 0, sizeof (ut_ent));

    ut_ent.ut_type = USER_PROCESS;
    ut_ent.ut_pid = pid;
    if (tty_id)
        strncpy (ut_ent.ut_id, tty_id, sizeof (ut_ent.ut_id));
    if (username)
        strncpy (ut_ent.ut_user, username, sizeof (ut_ent.ut_user));
    if (tty_no_dev_name)
        strncpy (ut_ent.ut_line, tty_no_dev_name, sizeof (ut_ent.ut_line));
    if (hostname)
        strncpy (ut_ent.ut_host, hostname, sizeof (ut_ent.ut_host));
    if (hostaddress)
        memcpy (&ut_ent.ut_addr_v6, hostaddress, sizeof (ut_ent.ut_addr_v6));

    ut_ent.ut_session = getsid (0);
    gettimeofday (&tv, NULL);
#ifdef _HAVE_UT_TV
    ut_ent.ut_tv.tv_sec = tv.tv_sec;
    ut_ent.ut_tv.tv_usec = tv.tv_usec;
#else
    ut_ent.ut_time = tv.tv_sec;
#endif

    pututline (&ut_ent);
    endutent ();

    updwtmp (_PATH_WTMP, &ut_ent);

    g_free (hostaddress);
    g_free (hostname);
    g_free (tty_no_dev_name);
    g_free (tty_id);
}

static gchar **
_split_command_line_with_regex(const char *command, GRegex *regex) {
  gchar **temp_strv = NULL;
  gchar **temp_iter = NULL, **args_iter = NULL;
  gchar **argv = NULL;

  temp_strv = regex ? g_regex_split (regex, command, G_REGEX_MATCH_NOTEMPTY)
                    : g_strsplit (command, " ", -1);
  if (!temp_strv) {
    WARN("Failed to split command: %s", command);
    return NULL;
  }

  argv = g_new0 (gchar *, g_strv_length (temp_strv));
  for (temp_iter = temp_strv, args_iter = argv;
      *temp_iter != NULL;
      temp_iter++) {
    size_t item_len = 0;
    gchar *item = g_strstrip (*temp_iter);

    item_len = strlen (item);
    if (item_len == 0) {
      continue;
    }
    if ((item[0] == '\"' && item[item_len - 1] == '\"') ||
        (item[0] == '\'' && item[item_len - 1] == '\'')) {
      item[item_len - 1] = '\0';
      memmove (item, item + 1, item_len - 1);
    }
    *args_iter = g_strcompress (item);
    args_iter++;
  }
  g_strfreev (temp_strv);

  return argv;
}

gchar **
tlm_utils_split_command_line(const gchar *command) {
  const gchar *pattern = "('.*?'|\".*?\"|\\S+)";
  GError *error = NULL;
  GRegex *regex = NULL;
  gchar **argv = NULL;
 
  if (!command) {
    WARN("Cannot pase NULL arguments string");
    return NULL;
  }
 
  if (!(regex = g_regex_new(pattern, 0, G_REGEX_MATCH_NOTEMPTY, &error))) {
    WARN("Failed to create regex: %s", error->message);
    g_error_free(error);
  }

  argv = _split_command_line_with_regex (command, regex);

  g_regex_unref (regex);

  return argv;
}

GList *
tlm_utils_split_command_lines (const GList const *commands_list) {
  const gchar *pattern = "('.*?'|\".*?\"|\\S+)";
  GError *error = NULL;
  GRegex *regex = NULL;
  GList *argv_list = NULL;
  const GList *tmp_list = NULL;

  if (!commands_list) {
    return NULL;
  }

  if (!(regex = g_regex_new(pattern, 0, G_REGEX_MATCH_NOTEMPTY, &error))) {
    WARN("Failed to create regex: %s", error->message);
    g_error_free(error);
  }

  for (tmp_list = commands_list; tmp_list; tmp_list = tmp_list->next) {
    argv_list = g_list_append (argv_list, _split_command_line_with_regex (
                    (const gchar *)tmp_list->data, regex));
  }

  g_regex_unref (regex);

  return argv_list;
}

typedef struct {
  int ifd;
  GHashTable *dir_table; /* { gchar*: GList* } */
  GHashTable *wd_table; /* { int: const gchar* } */
  WatchCb cb;
  gpointer userdata;
} WatchInfo;

static WatchInfo*
_watch_info_new (
    int ifd,
    WatchCb cb,
    gpointer userdata)
{
  WatchInfo *info = g_slice_new0 (WatchInfo);
  info->ifd = ifd;
  info->cb = cb;
  info->userdata = userdata;
  info->dir_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  info->wd_table = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, NULL);

  return info;
}

static void
_destroy_dir_table_entry (gpointer key, gpointer value, gpointer userdata) {
  (void)key;
  (void)userdata;
  g_list_free_full ((GList *)value, g_free);
}

static void
_destroy_wd_table_entry (gpointer key, gpointer value, gpointer userdata) {
  (void)value;
  int ifd = GPOINTER_TO_INT(userdata);
  int wd = GPOINTER_TO_INT(key);

  inotify_rm_watch (ifd, wd);
}

static void
_watch_info_free (WatchInfo *info)
{
  if (!info) return;

  if (info->dir_table) {
    g_hash_table_foreach (info->dir_table, _destroy_dir_table_entry, NULL);
    g_hash_table_unref (info->dir_table);
  }
  if (info->wd_table) {
    g_hash_table_foreach (info->wd_table, _destroy_wd_table_entry,
        GINT_TO_POINTER(info->ifd));
    g_hash_table_unref (info->wd_table);
  }
  if (info->ifd) close(info->ifd);

  g_slice_free (WatchInfo, info);
}

typedef enum {
  WATCH_FAILED,
  WATCH_ADDED,
  WATCH_READY
} AddWatchResults;
  
AddWatchResults
_add_watch (int ifd, char *file_path, WatchInfo *info) {
  GList *file_list = NULL;
  int wd = 0;
  gchar *file_name = NULL;
  gchar *dir = NULL;
  AddWatchResults res = WATCH_FAILED;

  if (!file_path) return WATCH_FAILED;

  file_name = g_path_get_basename (file_path);
  if (!file_name) return WATCH_FAILED;

  dir = g_path_get_dirname (file_path);
  if (!dir) {
    g_free (file_name);
    return WATCH_FAILED;
  }
  if ((file_list = (GList *)g_hash_table_lookup (
        info->dir_table, (gconstpointer)dir))) {
    file_list = g_list_append (file_list, file_name);
    g_free (dir);
    return WATCH_ADDED;
  }
  file_list = g_list_append (NULL, file_name);
  g_hash_table_insert (info->dir_table, g_strdup(dir), file_list);

  /* add watch on directory if its not existing */
  if (g_access (dir, 0)) {
    return _add_watch (ifd, dir, info);
  }

  DBG("Adding watch for file '%s' in dir '%s'", file_name, dir);
  if ((wd = inotify_add_watch (ifd, dir, IN_CREATE)) == -1) {
    WARN ("failed to add inotify watch on %s: %s", dir, strerror (errno));
    res = WATCH_FAILED;
    goto remove_and_return;
  }

  if (!g_access (file_path, 0)) {
    /* socket is ready, need not have a inotify watch for this */
    inotify_rm_watch (ifd, wd);
    res = WATCH_READY;
    goto remove_and_return;
  }

  g_hash_table_insert (info->wd_table, GINT_TO_POINTER(wd), dir);

  return WATCH_ADDED;

remove_and_return:
  g_hash_table_remove (info->dir_table, (gconstpointer)dir);
  g_list_free_full (file_list, (GDestroyNotify)g_free);
  
  return res;
}

static gboolean
_inotify_watcher_cb (gint ifd, GIOCondition condition, gpointer userdata)
{
  WatchInfo *info = (WatchInfo *)userdata;
  struct inotify_event *ie = NULL;
  gsize size = sizeof (struct inotify_event) + PATH_MAX + 1;
  guint nwatch = g_hash_table_size (info->wd_table);

  ie = (struct inotify_event *) g_slice_alloc0(size);
  while (nwatch &&
         read (ifd, ie, size) > (ssize_t)sizeof (struct inotify_event)) {
    GList *file_list = NULL;
    GList *element = NULL;
    GList *pending_list = NULL;
    gboolean is_first = FALSE;
    gchar *file_path = NULL;
    const gchar *dir = NULL;

    dir = (gchar *)g_hash_table_lookup (
        info->wd_table, GINT_TO_POINTER(ie->wd));
    if (!dir) continue;

    file_list = g_hash_table_lookup (info->dir_table, (gconstpointer)dir);
    element = g_list_find_custom (file_list,
        (gpointer)ie->name, (GCompareFunc)g_strcmp0);
    if (!element) {
      DBG("Ignoring '%s' file creation", ie->name);
      continue;
    }
    is_first = (file_list == element);

    g_free (element->data);
    file_list = g_list_delete_link(file_list, element);
    if (!file_list) {
      g_hash_table_remove (info->dir_table, dir);
      g_hash_table_remove (info->wd_table, GINT_TO_POINTER(ie->wd));
      inotify_rm_watch (ifd, ie->wd);
      nwatch--;
    } else if (is_first) {
      g_hash_table_insert (info->dir_table, g_strdup (dir), file_list);
    }

    file_path = g_build_filename (dir, ie->name, NULL);
    if ((pending_list = (GList *)g_hash_table_lookup (info->dir_table,
          (gconstpointer)file_path)) != NULL) {
      GList *tmp = NULL;

      // as we are about add real inotify watch, first remove from dir_table
      g_hash_table_steal (info->dir_table, (gconstpointer)file_path);

      // Add watches to all the files depend on this directory
      for (tmp = pending_list; tmp; tmp = tmp->next) {
        gchar *file_name = (gchar *)tmp->data;
        gchar *new_file_path = g_build_filename (file_path, file_name, NULL);
        AddWatchResults res = _add_watch (ifd, new_file_path, info);
        if (res == WATCH_READY) {
          if (info->cb) {
            info->cb (new_file_path, nwatch == 0, NULL, info->userdata);
          }
        } else if (res == WATCH_ADDED) nwatch++;
        else {
          WARN ("Couldn't add watch on '%s'", new_file_path);
        }
        g_free (file_name);
        g_free (new_file_path);
      }
      g_list_free (pending_list);
    } else {
      DBG("%s", file_path);
      if (info->cb) info->cb (file_path, nwatch == 0, NULL, info->userdata);
    }
    g_free (file_path);
  }

  g_slice_free1 (size, ie);

  return nwatch ? G_SOURCE_CONTINUE : G_SOURCE_REMOVE;
}

gchar *
_expand_file_path (const gchar *file_path)
{
  gchar **items =NULL;
  gchar **tmp_item =NULL;
  gchar *expanded_path = NULL;

  if (!file_path) return NULL;

  /* nothing to expand
   * FIXME: we are not considering filename which having \$ in it
   */
  if (g_strrstr (file_path, "$") == NULL) return g_strdup(file_path);

  items = g_strsplit (file_path, G_DIR_SEPARATOR_S, -1);
  /* soemthing wrong in file path */
  if (!items) { return g_strdup (file_path); }

  for (tmp_item = items; *tmp_item; tmp_item++) {
    char *item = *tmp_item;
    if (item[0] == '$') {
      const gchar *env = g_getenv (item+1);
      g_free (item);
      *tmp_item = g_strdup (env ? env : "");
    }
  }
  
  expanded_path = g_strjoinv (G_DIR_SEPARATOR_S, items);

  g_strfreev(items);

  return expanded_path;
}

guint
tlm_utils_watch_for_files (
    const gchar **watch_list,
    WatchCb cb,
    gpointer userdata)
{
  gint nwatch = 0;
  int ifd = 0;
  WatchInfo *w_info = NULL;

  if (!watch_list) return 0;

  if ((ifd = inotify_init1 (IN_NONBLOCK | IN_CLOEXEC)) < 0) {
    WARN("Failed to start inotify: %s", strerror(errno));
    return 0;
  }

  w_info = _watch_info_new (ifd, cb, userdata);

  for (; *watch_list; watch_list++) {
    char *socket_path  = _expand_file_path (*watch_list);
    AddWatchResults res = _add_watch (ifd, socket_path, w_info);
    if (res == WATCH_FAILED) {
      WARN ("Failed to watch for '%s'", socket_path);
    } else if (res == WATCH_READY) {
      gboolean is_final = !nwatch && !*(watch_list + 1);
      if (cb) cb (socket_path, is_final, NULL, userdata);
    } else {
      nwatch++;
    }
  }

  if (nwatch == 0) {
    _watch_info_free (w_info);
    return 0;
  }

  return g_unix_fd_add_full (G_PRIORITY_DEFAULT, ifd, G_IO_IN,
      _inotify_watcher_cb, w_info, (GDestroyNotify)_watch_info_free);
}

typedef struct _TlmLoginInfo
{
    gchar* username;
    gchar* password;
} TlmLoginInfo;

static int func_conv(
    int num_msg,
    const struct pam_message **msgs,
    struct pam_response **resps,
    void *pdata)
{
    int i;
    const char *login_prompt = "login";
    const char *pwd_prompt = "Password";

    if (num_msg <= 0)
        return PAM_CONV_ERR;

    TlmLoginInfo *info = (TlmLoginInfo *)pdata;
    *resps = g_malloc0 (num_msg * sizeof(struct pam_response));

    for (i=0; i<num_msg; ++i) {
        struct pam_response *resp = *resps + i;

        if (resp) {
            if (msgs[i]->msg_style == PAM_PROMPT_ECHO_ON &&
                strncmp(msgs[i]->msg, login_prompt, strlen(login_prompt)) == 0) {
                resp->resp = strndup (info->username, PAM_MAX_RESP_SIZE - 1);
            }
            else if (msgs[i]->msg_style == PAM_PROMPT_ECHO_OFF &&
                     strncmp(msgs[i]->msg, pwd_prompt, strlen(pwd_prompt)) == 0) {
                resp->resp = strndup (info->password, PAM_MAX_RESP_SIZE - 1);
            }
            else {
                resp->resp = NULL;
            }
            resp->resp_retcode = PAM_SUCCESS;
        }
    }
    return PAM_SUCCESS;
}

gboolean
tlm_authenticate_user (
    TlmConfig *config,
    const gchar *username,
    const gchar *password)
{
    pam_handle_t *pam_h = NULL;
    gboolean ret_auth = FALSE;
    int ret;
    const gchar *service = NULL;
    TlmLoginInfo *info = NULL;

    if (!password || !username) {
        WARN("username or password would be NULL");
        return FALSE;
    }

    // If TLM_CONFIG_PAM_AUTHENTICATION_SERVICE is not specified in tlm.conf
    // use "system-auth" as defult.
    service = tlm_config_get_string(config, TLM_CONFIG_GENERAL,
                                    // TLM_CONFIG_PAM_AUTHENTICATION_SERVICE);
                                    TLM_CONFIG_GENERAL_PAM_SERVICE);
    if (!service)
        service = "system-auth";

    info = g_malloc0 (sizeof (*info));
    info->username = strndup (username, PAM_MAX_RESP_SIZE - 1);
    info->password = strndup (password, PAM_MAX_RESP_SIZE - 1);
    const struct pam_conv conv = {func_conv, info};

    ret = pam_start (service, username, &conv, &pam_h);
    if (ret != PAM_SUCCESS) {
        WARN("Failed to pam_start: %d", ret);
        return FALSE;
    }

    ret = pam_authenticate (pam_h, PAM_SILENT);
    if (ret == PAM_SUCCESS)
        ret_auth = TRUE;
    else if (ret == PAM_AUTH_ERR)
        WARN("Failed to get authentication! username: %s", username);
    else
        WARN("Failed to pam_authenticate: %d", ret);

    pam_end(pam_h, ret);

    free(info->username);
    free(info->password);
    g_free(info);
    return ret_auth;
}
