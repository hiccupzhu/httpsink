#ifndef __GST_HTTPSINK_H__
#define __GST_HTTPSINK_H__

#include <stdio.h>

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>

G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define GST_TYPE_HTTPSINK \
  (gst_httpsink_get_type())
#define GST_HTTPSINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_HTTPSINK,Gsthttpsink))
#define GST_HTTPSINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_HTTPSINK,GsthttpsinkClass))
#define GST_IS_HTTPSINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_HTTPSINK))
#define GST_IS_HTTPSINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_HTTPSINK))

typedef struct _Gsthttpsink      Gsthttpsink;
typedef struct _GsthttpsinkClass GsthttpsinkClass;

struct _Gsthttpsink
{
  GstBaseSink parent;

  int   listenfd;
  int   port;
  int   kdpfd;
  guint64 current_pos;

  GMutex *mutex;
  GCond  *cond;

  int   stoped;
  GList* senders;
  struct sockaddr_in server;

  int  thread;
  gboolean silent;
};

struct _GsthttpsinkClass 
{
    GstBaseSinkClass parent_class;
};

GType gst_httpsink_get_type (void);

G_END_DECLS

#endif /* __GST_HTTPSINK_H__ */
