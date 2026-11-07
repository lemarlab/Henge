/* The engine calls DisplayRouter makes, so the router can be tested on a Mac without the engine. */
void henge_vm_set_display_layer(void *layer);
/* The mode a screen asks of the Mac: pixels, physical mm (0: the engine's own density rule) and Hz. */
void henge_vm_request_display_mode(unsigned int width, unsigned int height, unsigned int width_mm, unsigned int height_mm, unsigned int hz);
