#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/input.h> // for BTN_LEFT
#include <wayland-client.h>
#include <wayland-server.h>
#include <wayland-client-protocol.h>
#include <wayland-cursor.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

struct wl_display *display = NULL;
struct wl_compositor *compositor = NULL;
struct wl_surface *surface;
struct wl_egl_window *egl_window;
struct wl_region *region;
struct wl_callback *callback;
struct wl_shell *shell;
struct wl_shell_surface *shell_surface;

// for cursor
struct wl_shm *shm;
struct wl_cursor_theme *cursor_theme;
struct wl_cursor *default_cursor;
struct wl_surface *cursor_surface;


// input devices
struct wl_seat *seat;
struct wl_pointer *pointer;
//struct wl_touch *touch;
//struct wl_keyboard *keyboard;


EGLDisplay egl_display;
EGLConfig egl_conf;
EGLSurface egl_surface;
EGLContext egl_context;


static void
pointer_handle_enter(void *data, struct wl_pointer *pointer,
                     uint32_t serial, struct wl_surface *surface,
                     wl_fixed_t sx, wl_fixed_t sy)
{
    struct wl_buffer *buffer;
    struct wl_cursor_image *image;

    fprintf(stderr, "Pointer entered surface %p at %d %d\n", surface, sx, sy);
    
    image = default_cursor->images[0];
    buffer = wl_cursor_image_get_buffer(image);
    wl_pointer_set_cursor(pointer, serial,
			  cursor_surface,
			  image->hotspot_x,
			  image->hotspot_y);
    wl_surface_attach(cursor_surface, buffer, 0, 0);
    wl_surface_damage(cursor_surface, 0, 0,
		      image->width, image->height);
    wl_surface_commit(cursor_surface);
}

static void
pointer_handle_leave(void *data, struct wl_pointer *pointer,
                     uint32_t serial, struct wl_surface *surface)
{
    fprintf(stderr, "Pointer left surface %p\n", surface);
}

static void
pointer_handle_motion(void *data, struct wl_pointer *pointer,
                      uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
    printf("Pointer moved at %d %d\n", sx, sy);
}

static void
pointer_handle_button(void *data, struct wl_pointer *wl_pointer,
                      uint32_t serial, uint32_t time, uint32_t button,
                      uint32_t state)
{
    printf("Pointer button\n");
    
    if (button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_PRESSED)
	wl_shell_surface_move(shell_surface,
			      seat, serial);
    
}

static void
pointer_handle_axis(void *data, struct wl_pointer *wl_pointer,
                    uint32_t time, uint32_t axis, wl_fixed_t value)
{
    printf("Pointer handle axis\n");
}

static const struct wl_pointer_listener pointer_listener = {
    pointer_handle_enter,
    pointer_handle_leave,
    pointer_handle_motion,
    pointer_handle_button,
    pointer_handle_axis,
};

static void
seat_handle_capabilities(void *data, struct wl_seat *seat,
                         enum wl_seat_capability caps)
{
    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !pointer) {
	pointer = wl_seat_get_pointer(seat);
	wl_pointer_add_listener(pointer, &pointer_listener, NULL);
    } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && pointer) {
	wl_pointer_destroy(pointer);
	pointer = NULL;
    }
}

static const struct wl_seat_listener seat_listener = {
    seat_handle_capabilities,
};

void
global_registry_handler(void *data, struct wl_registry *registry, uint32_t id,
			const char *interface, uint32_t version)
{
    printf("Got a registry event for %s id %d\n", interface, id);
    if (strcmp(interface, "wl_compositor") == 0) {
        compositor = wl_registry_bind(registry, 
				      id, 
				      &wl_compositor_interface, 
				      1);
    } else if (strcmp(interface, "wl_shell") == 0) {
	shell = wl_registry_bind(registry, id,
				 &wl_shell_interface, 1);
	
    } else if (strcmp(interface, "wl_seat") == 0) {
	seat = wl_registry_bind(registry, id,
				&wl_seat_interface, 1);
	wl_seat_add_listener(seat, &seat_listener, NULL);
    } else if (strcmp(interface, "wl_shm") == 0) {
	shm = wl_registry_bind(registry, id,
				  &wl_shm_interface, 1);
                
	cursor_theme = wl_cursor_theme_load(NULL, 9, shm);
	default_cursor =
	    wl_cursor_theme_get_cursor(cursor_theme, "left_ptr");
    }

}

static void
global_registry_remover(void *data, struct wl_registry *registry, uint32_t id)
{
    printf("Got a registry losing event for %d\n", id);
}

static const struct wl_registry_listener registry_listener = {
    global_registry_handler,
    global_registry_remover
};


static void
redraw(void *data, struct wl_callback *callback, uint32_t time) {
    printf("Redrawing\n");
}

static const struct wl_callback_listener frame_listener = {
    redraw
};

static void
configure_callback(void *data, struct wl_callback *callback, uint32_t  time)
{
    if (callback == NULL)
	redraw(data, NULL, time);
}

static struct wl_callback_listener configure_callback_listener = {
    configure_callback,
};

static void
create_window() {
    egl_window = wl_egl_window_create(surface,
				      480, 360);
    if (egl_window == EGL_NO_SURFACE) {
	fprintf(stderr, "Can't create egl window\n");
	exit(1);
    } else {
	fprintf(stderr, "Created egl window\n");
    }

    egl_surface =
	eglCreateWindowSurface(egl_display,
			       egl_conf,
			       egl_window, NULL);

    if (eglMakeCurrent(egl_display, egl_surface,
		       egl_surface, egl_context)) {
	fprintf(stderr, "Made current\n");
    } else {
	fprintf(stderr, "Made current failed\n");
    }
    
    glClearColor(1.0, 1.0, 0.0, 0.1);
    glClear(GL_COLOR_BUFFER_BIT);
    glFlush();
    

    if (eglSwapBuffers(egl_display, egl_surface)) {
	fprintf(stderr, "Swapped buffers\n");
    } else {
	fprintf(stderr, "Swapped buffers failed\n");
    }
    wl_display_dispatch(display);
    wl_display_roundtrip(display);
}


static void
handle_ping(void *data, struct wl_shell_surface *shell_surface,
	    uint32_t serial)
{
    wl_shell_surface_pong(shell_surface, serial);
    fprintf(stderr, "Pinged and ponged\n");
}

static void
handle_configure(void *data, struct wl_shell_surface *shell_surface,
		 uint32_t edges, int32_t width, int32_t height)
{
}

static void
handle_popup_done(void *data, struct wl_shell_surface *shell_surface)
{
}

static const struct wl_shell_surface_listener shell_surface_listener = {
    handle_ping,
    handle_configure,
    handle_popup_done
};


static void
init_egl() {
    EGLint major, minor, count, n, size;
    EGLConfig *configs;
    EGLBoolean ret;
    int i;
    EGLint config_attribs[] = {
	EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
	EGL_RED_SIZE, 8,
	EGL_GREEN_SIZE, 8,
	EGL_BLUE_SIZE, 8,
	EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
	EGL_NONE
    };

    static const EGLint context_attribs[] = {
	EGL_CONTEXT_CLIENT_VERSION, 2,
	EGL_NONE
    };

    
    egl_display = eglGetDisplay((EGLNativeDisplayType) display);
    if (egl_display == EGL_NO_DISPLAY) {
	fprintf(stderr, "Can't create egl display\n");
	exit(1);
    } else {
	fprintf(stderr, "Created egl display\n");
    }

    if (eglInitialize(egl_display, &major, &minor) != EGL_TRUE) {
	fprintf(stderr, "Can't initialise egl display\n");
	exit(1);
    }
    printf("EGL major: %d, minor %d\n", major, minor);

    if (! eglBindAPI(EGL_OPENGL_ES_API)) {
	fprintf(stderr, "Can't bind API\n");
	exit(1);
    } else {
	fprintf(stderr, "Bound API\n");
    }

    eglGetConfigs(egl_display, NULL, 0, &count);
    printf("EGL has %d configs\n", count);

    configs = calloc(count, sizeof *configs);
    
    ret = eglChooseConfig(egl_display, config_attribs,
			  configs, count, &n);
    
    for (i = 0; i < n; i++) {
	eglGetConfigAttrib(egl_display,
			   configs[i], EGL_BUFFER_SIZE, &size);
	printf("Buffer size for config %d is %d\n", i, size);
	eglGetConfigAttrib(egl_display,
			   configs[i], EGL_RED_SIZE, &size);
	printf("Red size for config %d is %d\n", i, size);
	
	egl_conf = configs[i];
	break;
    }

    egl_context =
	eglCreateContext(egl_display,
			 egl_conf,
			 EGL_NO_CONTEXT, context_attribs);

}

int main(int argc, char **argv) {

    display = wl_display_connect(NULL);
    if (display == NULL) {
	fprintf(stderr, "Can't connect to display\n");
	exit(1);
    }
    printf("connected to display\n");

    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);

    wl_display_dispatch(display);
    wl_display_roundtrip(display);

    if (compositor == NULL) {
	fprintf(stderr, "Can't find compositor\n");
	exit(1);
    } else {
	fprintf(stderr, "Found compositor\n");
    }

    surface = wl_compositor_create_surface(compositor);
    if (surface == NULL) {
	fprintf(stderr, "Can't create surface\n");
	exit(1);
    } else {
	fprintf(stderr, "Created surface %p\n", surface);
    }

    shell_surface = wl_shell_get_shell_surface(shell, surface);
    wl_shell_surface_set_toplevel(shell_surface);

    
    wl_shell_surface_add_listener(shell_surface,
				  &shell_surface_listener, NULL);

    init_egl();
    create_window();

    cursor_surface =
	wl_compositor_create_surface(compositor);

    callback = wl_display_sync(display);
    wl_callback_add_listener(callback, &configure_callback_listener,
			     NULL);

    while (wl_display_dispatch(display) != -1) {
	;
    }

    wl_display_disconnect(display);
    printf("disconnected from display\n");
    
    exit(0);
}
