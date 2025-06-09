/**
 * @file board_config.h
 * @brief Board configuration selector based on Kconfig settings
 * 
 * This file includes the appropriate board configuration header based on
 * the CONFIG_ESPHAL_BOARD_TYPE setting from menuconfig.
 */

#pragma once

#include "sdkconfig.h"

// Include appropriate board configuration based on Kconfig selection
#if defined(CONFIG_ESPHAL_BOARD_REV_A_REFRIGERATOR)
    #include "rev_a_refrigerator.h"
#elif defined(CONFIG_ESPHAL_BOARD_REV_B_RIPENING_CHAMBER)
    #include "rev_b_ripening_chamber.h"
#elif defined(CONFIG_ESPHAL_BOARD_REV_C_DISPLAY_UNIT)
    #include "rev_c_display_unit.h"
#else
    #error "No board type selected in menuconfig! Please run 'idf.py menuconfig' and select ESPhal Board Type."
#endif

// Validate that essential configurations are defined
#ifndef BOARD_NAME
    #error "Board configuration must define BOARD_NAME"
#endif

#ifndef GPIO_OUTPUTS
    #error "Board configuration must define GPIO_OUTPUTS array"
#endif

#ifndef ONEWIRE_BUSES
    #error "Board configuration must define ONEWIRE_BUSES array"
#endif

#ifndef ADC_CHANNELS
    #error "Board configuration must define ADC_CHANNELS array"
#endif
