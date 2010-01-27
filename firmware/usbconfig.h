#ifndef __usbconfig_h_included__
#define __usbconfig_h_included__

#if PCB_REVISION == 1
  #define USB_CFG_IOPORTNAME      D
  #define USB_CFG_DMINUS_BIT      4
  #define USB_CFG_DPLUS_BIT       2
  #define USB_CFG_CLOCK_KHZ       (F_CPU/1000)
  #define USB_CFG_CHECK_CRC       0
#endif
#if PCB_REVISION == 2
  #define USB_CFG_IOPORTNAME      B
  #define USB_CFG_DMINUS_BIT      1
  #define USB_CFG_DPLUS_BIT       0
  #define USB_CFG_CLOCK_KHZ       (F_CPU/1000)
  #define USB_CFG_CHECK_CRC       0
#endif

#define USB_CFG_HAVE_INTRIN_ENDPOINT    0
#define USB_CFG_HAVE_INTRIN_ENDPOINT3   0
#define USB_CFG_EP3_NUMBER              3
#define USB_CFG_IMPLEMENT_HALT          0
#define USB_CFG_SUPPRESS_INTR_CODE      0
#define USB_CFG_INTR_POLL_INTERVAL      10
#define USB_CFG_IS_SELF_POWERED         0
#define USB_CFG_MAX_BUS_POWER           40
#define USB_CFG_IMPLEMENT_FN_WRITE      1
#define USB_CFG_IMPLEMENT_FN_READ       0
#define USB_CFG_IMPLEMENT_FN_WRITEOUT   0
/* Define this to 1 if you want to use interrupt-out (or bulk out) endpoints.
 * You must implement the function usbFunctionWriteOut() which receives all
 * interrupt/bulk data sent to any endpoint other than 0. The endpoint number
 * can be found in 'usbRxToken'.
 */
#define USB_CFG_HAVE_FLOWCONTROL        0
#define USB_CFG_LONG_TRANSFERS          1
#define USB_COUNT_SOF                   0
#define USB_CFG_CHECK_DATA_TOGGLING     0
#define USB_CFG_HAVE_MEASURE_FRAME_LENGTH   0
#define USB_USE_FAST_CRC                0

/* -------------------------- Device Description --------------------------- */
/* Same as above for the device name. If you don't want a device name, undefine
 * the macros. See the file USB-IDs-for-free.txt before you assign a name if
 * you use a shared VID/PID.
 */
/*#define USB_CFG_SERIAL_NUMBER   'N', 'o', 'n', 'e' */
/*#define USB_CFG_SERIAL_NUMBER_LEN   0 */
/* Same as above for the serial number. If you don't want a serial number,
 * undefine the macros.
 * It may be useful to provide the serial number through other means than at
 * compile time. See the section about descriptor properties below for how
 * to fine tune control over USB descriptors such as the string descriptor
 * for the serial number.
 */
#define USB_CFG_DEVICE_CLASS        0xff    /* set to 0 if deferred to interface */
#define USB_CFG_DEVICE_SUBCLASS     0
#define USB_CFG_INTERFACE_CLASS     0
#define USB_CFG_INTERFACE_SUBCLASS  0
#define USB_CFG_INTERFACE_PROTOCOL  0

/* ------------------- Fine Control over USB Descriptors ------------------- */
/* If you don't want to use the driver's default USB descriptors, you can
 * provide our own. These can be provided as (1) fixed length static data in
 * flash memory, (2) fixed length static data in RAM or (3) dynamically at
 * runtime in the function usbFunctionDescriptor(). See usbdrv.h for more
 * information about this function.
 * Descriptor handling is configured through the descriptor's properties. If
 * no properties are defined or if they are 0, the default descriptor is used.
 * Possible properties are:
 *   + USB_PROP_IS_DYNAMIC: The data for the descriptor should be fetched
 *     at runtime via usbFunctionDescriptor(). If the usbMsgPtr mechanism is
 *     used, the data is in FLASH by default. Add property USB_PROP_IS_RAM if
 *     you want RAM pointers.
 *   + USB_PROP_IS_RAM: The data returned by usbFunctionDescriptor() or found
 *     in static memory is in RAM, not in flash memory.
 *   + USB_PROP_LENGTH(len): If the data is in static memory (RAM or flash),
 *     the driver must know the descriptor's length. The descriptor itself is
 *     found at the address of a well known identifier (see below).
 * List of static descriptor names (must be declared PROGMEM if in flash):
 *   char usbDescriptorDevice[];
 *   char usbDescriptorConfiguration[];
 *   char usbDescriptorHidReport[];
 *   char usbDescriptorString0[];
 *   int usbDescriptorStringVendor[];
 *   int usbDescriptorStringDevice[];
 *   int usbDescriptorStringSerialNumber[];
 * Other descriptors can't be provided statically, they must be provided
 * dynamically at runtime.
 *
 * Descriptor properties are or-ed or added together, e.g.:
 * #define USB_CFG_DESCR_PROPS_DEVICE   (USB_PROP_IS_RAM | USB_PROP_LENGTH(18))
 *
 * The following descriptors are defined:
 *   USB_CFG_DESCR_PROPS_DEVICE
 *   USB_CFG_DESCR_PROPS_CONFIGURATION
 *   USB_CFG_DESCR_PROPS_STRINGS
 *   USB_CFG_DESCR_PROPS_STRING_0
 *   USB_CFG_DESCR_PROPS_STRING_VENDOR
 *   USB_CFG_DESCR_PROPS_STRING_PRODUCT
 *   USB_CFG_DESCR_PROPS_STRING_SERIAL_NUMBER
 *   USB_CFG_DESCR_PROPS_HID
 *   USB_CFG_DESCR_PROPS_HID_REPORT
 *   USB_CFG_DESCR_PROPS_UNKNOWN (for all descriptors not handled by the driver)
 *
 * Note about string descriptors: String descriptors are not just strings, they
 * are Unicode strings prefixed with a 2 byte header. Example:
 * int  serialNumberDescriptor[] = {
 *     USB_STRING_DESCRIPTOR_HEADER(6),
 *     'S', 'e', 'r', 'i', 'a', 'l'
 * };
 */

#define USB_CFG_DESCR_PROPS_DEVICE                  0
#define USB_CFG_DESCR_PROPS_CONFIGURATION           0
#define USB_CFG_DESCR_PROPS_STRINGS                 0
#define USB_CFG_DESCR_PROPS_STRING_0                0
#define USB_CFG_DESCR_PROPS_STRING_VENDOR           0
#define USB_CFG_DESCR_PROPS_STRING_PRODUCT          0
#define USB_CFG_DESCR_PROPS_STRING_SERIAL_NUMBER    0
#define USB_CFG_DESCR_PROPS_HID                     0
#define USB_CFG_DESCR_PROPS_HID_REPORT              0
#define USB_CFG_DESCR_PROPS_UNKNOWN                 0

/* #define USB_INTR_CFG            MCUCR */
/* #define USB_INTR_CFG_CLR        0 */
/* #define USB_INTR_ENABLE         GIMSK */
/* #define USB_INTR_ENABLE_BIT     INT0 */
/* #define USB_INTR_PENDING        GIFR */
#if PCB_REVISION == 1
  #define USB_INTR_PENDING_BIT    INTF0
  #define USB_INTR_CFG_SET        ((1 << ISC01) | (1 << ISC00) | (1 << ISC11)| (0 << ISC10)) 
#endif
#if PCB_REVISION == 2
  #define USB_INTR_CFG            PCICR
  #define USB_INTR_CFG_SET        (1 << PCIE0) 
  #define USB_INTR_CFG_CLR        0
  #define USB_INTR_ENABLE         PCMSK0
  #define USB_INTR_ENABLE_BIT     PCINT0
  #define USB_INTR_PENDING        PCIFR
  #define USB_INTR_PENDING_BIT    PCIF0
  #define USB_INTR_VECTOR         SIG_PIN_CHANGE0
#endif

#endif /* __usbconfig_h_included__ */
