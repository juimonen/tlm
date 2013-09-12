#ifndef _TLM_LOG_H
#define _TLM_LOG_H

#include <glib.h>

G_BEGIN_DECLS

void tlm_log_init (void);
void tlm_log_close (void);

#define EXPAND_LOG_MSG(frmt, args...) \
    ("%s +%d:%s", __FILE__, __LINE__, frmt, ##args)

#define INFO(frmt, args...)     g_print(EXPAND_LOG_MSG(frmt, ##args))
#define DBG(frmt, args...)      g_debug(EXPAND_LOG_MSG(frmt, ##args))
#define WARN(frmt, args...)     g_warning(EXPAND_LOG_MSG(frmt, ##args))
#define CRITICAL(frmt, args...) g_critical(EXPAND_LOG_MSG(frmt, ##args))
#define ERR(frmt, args...)      g_error(EXPAND_LOG_MSG(frmt, ##args))

G_END_DECLS

#endif /* _TLM_LOG_H */
