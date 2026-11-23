/*
 * views.h - Read-only screens and widgets (boards, track diagram, charts)
 */
#ifndef UI_VIEWS_H
#define UI_VIEWS_H

#include "core/station.h"
#include "ui/ui.h"

/* Widgets */
const char *view_lamp(SignalAspect a);
UiColor     view_status_color(TrainStatus s);
UiColor     view_aspect_color(SignalAspect a);

/* Building blocks */
void view_splash(void);
void view_header(Station *st, const char *subtitle);
void view_stats(Station *st);
void view_train_header(void);
void view_train_row(const Train *t);
int  view_train_list(Station *st, int include_inactive, int limit);
void view_train_details(Station *st, const Train *t);
void view_track(Station *st);
void view_platform_table(Station *st);
void view_signal_table(Station *st);
void view_occupancy_chart(Station *st);
void view_delay_report(Station *st);
void view_events(Station *st, int n);
void view_events_since(Station *st, long since);
void view_help(void);

/* Full screens */
void view_dashboard(Station *st);

#endif /* UI_VIEWS_H */
