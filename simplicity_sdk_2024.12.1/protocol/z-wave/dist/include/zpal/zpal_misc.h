/**
 * @file
 * Defines a platform abstraction layer for the Z-Wave miscellaneous
 * functions, not covered by other modules.
 *
 * @copyright 2021 Silicon Laboratories Inc.
 */

#ifndef ZPAL_MISC_H_
#define ZPAL_MISC_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup zpal
 * @brief
 * Z-Wave Platform Abstraction Layer.
 * @{
 * @addtogroup zpal-misc
 * @brief
 * Defines a platform abstraction layer for the Z-Wave miscellaneous
 * functions, not covered by other modules.
 *
 * @{
 */

/**
 * @brief Product id struct.
 */
typedef struct {
  uint16_t app_manufacturer_id;   ///< Manufacturer ID identifiers. (MFG_ID_XXX_XXX)
  uint16_t app_product_id;        ///< Product ID. (PRODUCT_ID_XXX_XXX)
  uint16_t app_product_type;      ///< Product type Id. (PRODUCT_TYPE_ID_ZWAVE_XXX_XXX)
  uint16_t app_icon_type;         ///< Z-Wave+ Icon Type identifiers. (ICON_TYPE_XXX_XXX)
  uint8_t generic_type;           ///< Generic Device Class identifier. (GENERIC_TYPE_XXX_XXX)
  uint8_t specyfic_type;          ///< Specific Device Class identifier. (SPECYFIC_TYPE_XXX_XXX)
  uint8_t requested_security_key; ///< Bitmask for security keys. (SECURITY_KEY_SX_XXX)
} zpal_product_id_t;

/**
 * @brief Defines for identifying the secure element type supported by the chip.
 */
typedef enum {
  ZPAL_CHIP_SE_UNKNOWN,
  ZPAL_CHIP_SE_MID,
  ZPAL_CHIP_SE_HIGH
} zpal_chip_se_type_t;

/**
 * @brief Manufacturer's reset information
 */
typedef uint16_t zpal_soft_reset_info_t;


/**
 * @brief Manufacturer ID used by zpal_reboot_with_info.
 */
typedef uint16_t zpal_soft_reset_mfid_t;

static const zpal_soft_reset_info_t ZPAL_RESET_REQUESTED_BY_SAPI     = 0x0000;
static const zpal_soft_reset_info_t ZPAL_RESET_UNHANDLED_RADIO_EVENT = 0x0001;
static const zpal_soft_reset_info_t ZPAL_RESET_RADIO_ASSERT          = 0x0002;
static const zpal_soft_reset_info_t ZPAL_RESET_ASSERT_PTR            = 0x0003;
static const zpal_soft_reset_info_t ZPAL_RESET_EVENT_FLUSH_MEMORY    = 0x0004;
static const zpal_soft_reset_info_t ZPAL_RESET_INFO_DEFAULT          = 0xFFFF;

/**
 * @brief Perform a system reboot and provide information about the context.
 *
 * @param[in] manufacturer_id manufacturer ID identifier.
 * @param[in] reset_info the information to pass to boot.
 */
void zpal_reboot_with_info(const zpal_soft_reset_mfid_t manufacturer_id,
                           const zpal_soft_reset_info_t reset_info);

/**
 * @brief Prepare for shutdown handler.
 */
void zpal_initiate_shutdown_handler(void);

/**
 * @brief Shutdown handler.
 */
void zpal_shutdown_handler(void);

/**
 * @brief Get serial number length.
 *
 * @return Serial number length.
 */
size_t zpal_get_serial_number_length(void);

/**
 * @brief Get serial number.
 *
 * @param[out] serial_number Serial number.
 */
void zpal_get_serial_number(uint8_t *serial_number);

/**
 * @brief Check if in ISR context.
 *
 * @return True if the CPU is in handler mode (currently executing an interrupt handler).
 *         False if the CPU is in thread mode.
 */
bool zpal_in_isr(void);

/**
 * @brief Get chip type.
 *
 * @return Chip type.
 */
uint8_t zpal_get_chip_type(void);

/**
 * @brief Get chip revision.
 *
 * @return Chip revision.
 */
uint8_t zpal_get_chip_revision(void);

/**
 * @brief Get application version.
 *
 * @return Application version.
 *
 * @note This function exists in PAL to allow use app version by external module (e.g. bootloader).
 */
uint32_t zpal_get_app_version(void);

/**
 * @brief Get major part of application version.
 *
 * @return Major part of application version.
 *
 * @note This function exists in PAL to allow use app version by external module (e.g. bootloader).
 */
uint8_t zpal_get_app_version_major(void);

/**
 * @brief Get minor part of application version.
 *
 * @return Minor part of application version.
 *
 * @note This function exists in PAL to allow use app version by external module (e.g. bootloader).
 */
uint8_t zpal_get_app_version_minor(void);

/**
 * @brief Get patch part of application version.
 *
 * @return Patch part of application version.
 *
 * @note This function exists in PAL to allow use app version by external module (e.g. bootloader).
 */
uint8_t zpal_get_app_version_patch(void);

/**
 * @brief Get product id.
 *
 * @param[out] product_id Product id.
 *
 * @note This function exists in PAL to allow use product id by external module (e.g. bootloader).
 */
void zpal_get_product_id(zpal_product_id_t *product_id);

/**
 * @brief Initialize debug output.
 */
void zpal_debug_init(void);

/**
 * @brief Output debug logs.
 *
 * @param[out] data   Pointer to debug data.
 * @param[in]  length Length of debug data.
 */
void zpal_debug_output(const uint8_t *data, uint32_t length);

/**
 * @brief Disable interrupts.
 */
void zpal_disable_interrupts(void);

/**
 * @brief Get secure element type supported in the chip.
 *
 * @return Secure element type supported.
 *
 */
zpal_chip_se_type_t zpal_get_secure_element_type(void);

/**
 * @brief Set vendor specific location for storing
 * keys persistently in wrapped or plain form based
 * on the secure element type supported by the chip.
 *
 * @param[in] attributes of the key
 */
void zpal_psa_set_location_persistent_key(const void *attributes);

/**
 * @brief Set vendor specific location for storing
 * keys in volatile memory, in wrapped or plain form based
 * on the secure element type supported by the chip.
 *
 * @param[in] attributes of the key
 */
void zpal_psa_set_location_volatile_key(const void *attributes);

/**
 * @brief Follows the same behavior as printf, allows
 * vendors to use different backends for printing (UART, SWO, RTT, etc.)
 * @param[in] format of the string to print, 
 * follows the same format as the standard printf
 */

void zpal_printf(char *format, ...);

/**
 * @} //zpal-misc
 * @} //zpal
 */

#ifdef __cplusplus
}
#endif

#endif /* ZPAL_MISC_H_ */
