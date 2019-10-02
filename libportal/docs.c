/* This file collects documentation that has no good home
 * in any of the 'real' source files.
 */

/**
 * SECTION:parent
 * @title: XdpParent
 * @short_description: parent window abstraction
 *
 * The XdpParent struct provides an abstract way to represent
 * a window, without introducing a dependency on a toolkit
 * library.
 *
 * An XdpParent implementation for GTK+ is included in the
 * portal-gtk.h header file, in the form of inline functions.
 * To create a XdpParent for a GTK+ window, use
 * xdp_parent_new_gtk().
 */

/**
 * xdp_parent_new_gtk:
 * @window: a #GtkWindow
 *
 * Creates a XdpParent for a GtkWindow.
 *
 * Returns: a newly created #XdpParent. Free with xdp_parent_free()
 */

/**
 * xdp_parent_free:
 * @parent: a #XdpParent
 *
 * Frees an #XdpParent when it is no longer needed.
 */

/**
 * XdpPortal:
 *
 * The XdpPortal struct contains only private fields and should not
 * be accessed directly.
 */

/**
 * XdpSession:
 *
 * The XdpSession struct contains only private fields and should not
 * be accessed directly.
 */
