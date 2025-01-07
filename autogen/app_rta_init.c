#include "sl_component_catalog.h"
#ifdef SL_CATALOG_KERNEL_PRESENT
#include "app_rta_internal.h"
#else // SL_CATALOG_KERNEL_PRESENT
#include "app_rta_internal_bm.h"
#endif // SL_CATALOG_KERNEL_PRESENT
#include "app_button_press_internal.h"

void app_rta_init_contributors(void)
{
  app_button_press_init();
}

void app_rta_ready(void)
{
}
