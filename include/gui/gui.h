/* include/gui/gui.h
 * Minimal windowing compositor. Arch-independent — built entirely on
 * the arch_gfx_ and arch_mouse_get_state calls, never touches
 * hardware directly.
 */

#ifndef GUI_GUI_H
#define GUI_GUI_H

void gui_init(void);
void gui_update(void);   /* call periodically from the idle loop */

#endif /* GUI_GUI_H */
