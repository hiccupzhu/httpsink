#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <stdio.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/epoll.h>


static const int MAXEPOLLSIZE = 0x2000; //8K


#include "gsthttpsink.h"

GST_DEBUG_CATEGORY_STATIC (gst_httpsink_debug);
#define GST_CAT_DEFAULT gst_httpsink_debug

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_PORT,
  PROP_SILENT
};

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

static void     gst_http_sink_dispose (GObject * object);
static gboolean gst_http_sink_start (GstBaseSink * sink);
static gboolean gst_http_sink_stop (GstBaseSink * sink);
static gboolean gst_http_sink_event (GstBaseSink * sink, GstEvent * event);
static GstFlowReturn gst_http_sink_render (GstBaseSink * sink, GstBuffer * buffer);


static void gst_httpsink_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_httpsink_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);

static gboolean gst_http_sink_query (GstPad * pad, GstQuery * query);
static gboolean gst_httpsink_set_caps (GstPad * pad, GstCaps * caps);
static GstFlowReturn gst_httpsink_chain (GstPad * pad, GstBuffer * buf);

GST_BOILERPLATE (Gsthttpsink, gst_httpsink, GstBaseSink, GST_TYPE_BASE_SINK);

/* GObject vmethod implementations */

static void
gst_httpsink_base_init (gpointer gclass)
{
    g_print("%s::%d\n", __FUNCTION__, __LINE__);
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  g_print("%s::%d\n", __FUNCTION__, __LINE__);
  gst_element_class_set_details_simple(element_class,
    "httpsink",
    "http-sink",
    "Fot the http service",
    "szhu <<szhu_mail@163.com>>");
  g_print("%s::%d\n", __FUNCTION__, __LINE__);

  gst_element_class_add_pad_template (element_class, gst_static_pad_template_get (&sink_factory));
}

/* initialize the httpsink's class */
static void
gst_httpsink_class_init (GsthttpsinkClass * klass)
{
    GObjectClass *gobject_class;
    GstBaseSinkClass *gstbasesink_class;

    ////thi
    //FIXME
    signal(SIGPIPE, SIG_IGN);

    g_print("%s::%d\n", __FUNCTION__, __LINE__);
    gstbasesink_class = GST_BASE_SINK_CLASS (klass);
    gobject_class = (GObjectClass *) klass;

    gobject_class->dispose = gst_http_sink_dispose;

    gobject_class->set_property = gst_httpsink_set_property;
    gobject_class->get_property = gst_httpsink_get_property;

    g_object_class_install_property (gobject_class, PROP_PORT,
            g_param_spec_string ("port", "listen port",
                    "http listen port", NULL,
                    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class, PROP_SILENT,
            g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
                    FALSE, G_PARAM_READWRITE));

    gstbasesink_class->start = GST_DEBUG_FUNCPTR(gst_http_sink_start);
    gstbasesink_class->stop = GST_DEBUG_FUNCPTR(gst_http_sink_stop);
    gstbasesink_class->render = GST_DEBUG_FUNCPTR(gst_http_sink_render);
    gstbasesink_class->event = GST_DEBUG_FUNCPTR(gst_http_sink_event);
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_httpsink_init (Gsthttpsink * s, GsthttpsinkClass * gclass)
{
    GstPad *pad;

    pad = GST_BASE_SINK_PAD (s);
    //  filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "s");
    gst_pad_set_setcaps_function (pad, GST_DEBUG_FUNCPTR(gst_httpsink_set_caps));
    gst_pad_set_getcaps_function (pad, GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));
    //  gst_pad_set_chain_function (filter->sinkpad,
    //                              GST_DEBUG_FUNCPTR(gst_httpsink_chain));

    //  gst_pad_set_query_function (pad, GST_DEBUG_FUNCPTR (gst_http_sink_query));
    //  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);
    s->senders = NULL;
    s->silent = FALSE;
    s->listenfd = -1;
    s->stoped = 0;
    s->port = 5000;//defaule value
    s->current_pos = 0;
    s->kdpfd = -1;
    s->mutex = g_mutex_new();
    s->cond = g_cond_new();
}

static void
gst_http_sink_dispose (GObject * object){
    Gsthttpsink *s = GST_HTTPSINK(object);
    G_OBJECT_CLASS (parent_class)->dispose (object);

    gst_http_sink_stop(s);

    if(s->listenfd != -1){
        close(s->listenfd);
        s->listenfd = -1;
    }

    if(s->mutex){
        g_mutex_free(s->mutex);
        s->mutex = NULL;
    }

    if(s->cond){
        g_cond_free(s->cond);
        s->cond = NULL;
    }

}

//static gboolean
//gst_http_sink_set_location (Gsthttpsink * sink, const gchar * location){
//
//    return FALSE;
//}

static void
gst_httpsink_set_property (GObject * object, guint prop_id,
        const GValue * value, GParamSpec * pspec)
{
    Gsthttpsink *sink = GST_HTTPSINK (object);

    switch (prop_id) {
    case PROP_PORT:
//        gst_http_sink_set_location(sink, g_value_get_string(value));
        sink->port = g_value_get_int(value);
        break;
    case PROP_SILENT:
        sink->silent = g_value_get_boolean (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gst_httpsink_get_property (GObject * object, guint prop_id,
        GValue * value, GParamSpec * pspec)
{
    Gsthttpsink *sink = GST_HTTPSINK (object);

    switch (prop_id) {
    case PROP_SILENT:
        g_value_set_boolean (value, sink->silent);
        break;
    case PROP_PORT:
        g_value_set_int(value, sink->port);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

/* GstElement vmethod implementations */

/* this function handles the link with other elements */
static gboolean
gst_httpsink_set_caps (GstPad * pad, GstCaps * caps)
{
//  Gsthttpsink *filter;
//  GstPad *otherpad;
//
//  filter = GST_HTTPSINK (gst_pad_get_parent (pad));
//  gst_object_unref (filter);

  return gst_pad_set_caps (pad, caps);
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_httpsink_chain (GstPad * pad, GstBuffer * buf)
{
  Gsthttpsink *filter;

//  g_print("===>>gst_httpsink_chain buf.size=%d\n", buf->size);
//  filter = GST_HTTPSINK (GST_OBJECT_PARENT (pad));
//
//  if (filter->silent == FALSE)
//    g_print ("I'm plugged, therefore I'm in.\n");

  /* just push out the incoming buffer without touching it */
//  return gst_pad_push (filter->srcpad, buf);
  return GST_FLOW_OK;
}

static gboolean
gst_http_sink_query (GstPad * pad, GstQuery * query)
{
    Gsthttpsink *self;
    GstFormat format;

    self = GST_HTTPSINK (GST_PAD_PARENT (pad));

    switch (GST_QUERY_TYPE (query)) {
    case GST_FORMAT_DEFAULT:
    case GST_FORMAT_BYTES:
        gst_query_set_position (query, GST_FORMAT_BYTES, self->current_pos);
        return TRUE;
    case GST_QUERY_FORMATS:
        gst_query_set_formats (query, 2, GST_FORMAT_DEFAULT, GST_FORMAT_BYTES);
        return TRUE;

    case GST_QUERY_URI:
    default:
        return gst_pad_query_default (pad, query);
    }
}

static gboolean send_http_response(int fd){
    char response[1024];
    sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: video/h264\r\n"
            "Server: gsthttpsink-0.1.3\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            );
    int wnum = write(fd, response, strlen(response));
    if(wnum != strlen(response)){
        g_printf("send_http_response failed\n");
        return FALSE;
    }else{
        g_printf("send_http_response successful\n");
        return TRUE;
    }
}

static void* thread_func(void* data){
    struct sockaddr_in cliaddr;
    Gsthttpsink* s = GST_HTTPSINK(GST_ELEMENT(data));
    int n, socklen, sender_fd, nfds, connfd;

    char buf[0x1000];
    struct epoll_event ev;
    struct epoll_event *events;

    events = g_malloc(sizeof(struct epoll_event) * MAXEPOLLSIZE);
    if(events == NULL){
        GST_ERROR("gst_httpsink_init::g_malloc failed\n");
    }

    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = s->listenfd;
    if (epoll_ctl(s->kdpfd, EPOLL_CTL_ADD, s->listenfd, &ev) < 0)     {
        GST_ERROR("epoll set insertion error: fd=%d\n", s->listenfd);
        return ;
    }

    while(!s->stoped){
        nfds = epoll_wait(s->kdpfd, events, 1, -1);
        if (nfds == -1) {
            GST_ERROR("epoll_wait failed\n");
            continue;
        }
        /* 处理所有事件 */
        for (n = 0; n < nfds; ++n){
            //if it is man listen event
            if (events[n].data.fd == s->listenfd) {
                socklen = sizeof(cliaddr);
                connfd = accept(s->listenfd, (struct sockaddr *)&cliaddr, &socklen);
                if (connfd < 0){
                    perror("accept error");
                    continue;
                }



                ev.events = EPOLLIN | EPOLLHUP | EPOLLERR | EPOLLET;
                ev.data.fd = connfd;
                if (epoll_ctl(s->kdpfd, EPOLL_CTL_ADD, connfd, &ev) < 0) {
                    GST_ERROR("add socket '%d' to epoll failed: %s\n", connfd, strerror(errno));
                    return NULL;
                }

//                g_mutex_lock(s->mutex);
//                s->senders = g_list_append(s->senders, connfd);
//                g_mutex_unlock(s->mutex);
//
//                g_cond_signal(s->cond);
            }else{
                g_print("[%d]::receive events-id=0x%X\n", events[n].data.fd, events[n].events);
                if((events[n].events & EPOLLHUP) && (events[n].events & EPOLLERR)){
                    if(events[n].events & EPOLLHUP){ g_print("[%d]::receive EPOLLHUP\n", events[n].data.fd); }
                    if(events[n].events & EPOLLERR){ g_print("[%d]::receive EPOLLERR\n", events[n].data.fd); }
                    GList* list = g_list_find(s->senders, events[n].data.fd);
                    if(list){
                        g_print("Remove/close fd[%d]\n", events[n].data.fd);
                        epoll_ctl(s->kdpfd, EPOLL_CTL_DEL, events[n].data.fd, &ev);
                        g_mutex_lock(s->mutex);
                        s->senders = g_list_remove(s->senders, events[n].data.fd);
                        g_mutex_unlock(s->mutex);
                        close(events[n].data.fd);
                    }

                }

                if(events[n].events & EPOLLIN){
                    int rnum = read(events[n].data.fd, buf, sizeof(buf));
                    if(rnum < 0) { continue; }
                    g_print("read[%d]:\n%s\n", rnum, buf);

                    GList* list = g_list_find(s->senders, events[n].data.fd);
                    if(!list){
                        send_http_response(events[n].data.fd);
                        g_mutex_lock(s->mutex);
                        s->senders = g_list_append(s->senders, events[n].data.fd);
                        g_mutex_unlock(s->mutex);

                        g_cond_signal(s->cond);
                    }
                }
            }
        }
    }
    if(events){
        g_free(events);
        events = NULL;
    }
    return NULL;
}

static gboolean gst_http_sink_start (GstBaseSink * sink){
    Gsthttpsink* s = GST_HTTPSINK(sink); //httpsink

    s->stoped = 0;
    s->listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(s->listenfd == -1){
        GST_ERROR("Open TCP socket failed\n");
        return FALSE;
    }
    int opt = SO_REUSEADDR;
    setsockopt(s->listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));  //设置socket属性

    s->server.sin_family = AF_INET;
    s->server.sin_port = htons(s->port);
    s->server.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(s->listenfd, (struct sockaddr *)&s->server, sizeof(struct sockaddr)) == -1){
        GST_ERROR("Bind error\n");
        return FALSE;
    }

    if (listen(s->listenfd, 5) == -1){
        GST_ERROR("listen() error\n");
        return FALSE;
    }

    s->kdpfd = epoll_create(MAXEPOLLSIZE);


    pthread_t th;
    s->thread = pthread_create(&th, NULL, thread_func, sink);

    return TRUE;
}

static gboolean gst_http_sink_stop (GstBaseSink * sink){
    GList* list;
    int fd, len;
    Gsthttpsink* s = GST_HTTPSINK(sink); //httpsink

    if(s->stoped){ return TRUE; }

    s->stoped = 1;

    g_mutex_lock(s->mutex);
    list = g_list_first(s->senders);
    len = g_list_length(s->senders);
    if( len > 0){
        for(; list != NULL ; list = g_list_next(list)){
            fd = (gint32)list->data;
            close(fd);
        }
        g_list_free(s->senders);
    }
    g_mutex_unlock(s->mutex);

    return TRUE;
}

static gboolean gst_http_sink_event (GstBaseSink * sink, GstEvent * event){
    Gsthttpsink* s = GST_HTTPSINK(sink); //httpsink
    return TRUE;
}

static GstFlowReturn gst_http_sink_render (GstBaseSink * sink, GstBuffer * buffer){
    gint32 wnum = 0;
    int fd, len;
    GList* list;
    Gsthttpsink* s = GST_HTTPSINK(sink); //httpsink

    s->current_pos += buffer->size;

    len = g_list_length(s->senders);
    if( len > 0){
        g_mutex_lock(s->mutex);
        list = g_list_first(s->senders);
        g_mutex_unlock(s->mutex);

        while(list){
            fd = (gint32)list->data;
            dprintf (fd, "%x\r\n", buffer->size);
            wnum = write(fd, buffer->data, buffer->size);
            if(wnum != buffer->size){
//                g_print("Invalid socket 0x%08X\n", fd);
            }
            wnum = write(fd, "\r\n", 2);
            g_mutex_lock(s->mutex);
            list = g_list_next(list);
            g_mutex_unlock(s->mutex);
        }
    }else{
//        g_mutex_lock(s->mutex);
//        g_cond_wait(s->cond, s->mutex);
//        g_mutex_unlock(s->mutex);
    }

    return GST_FLOW_OK;
}




/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
httpsink_init (GstPlugin * httpsink)
{
  GST_DEBUG_CATEGORY_INIT (gst_httpsink_debug, "httpsink", 0, "Template httpsink");

  return gst_element_register (httpsink, "httpsink", GST_RANK_NONE, GST_TYPE_HTTPSINK);
}
/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "httpsink"
#endif

/* gstreamer looks for this structure to register httpsinks
 *
 * exchange the string 'Template httpsink' with your httpsink description
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "httpsink",
    "Template httpsink",
    httpsink_init,
    VERSION,
    "LGPL",
    "GStreamer",
    "http://gstreamer.net/"
)
