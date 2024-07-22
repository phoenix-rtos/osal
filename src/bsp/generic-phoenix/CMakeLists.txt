######################################################################
#
# CMAKE build recipe for LINUX Board Support Package (BSP)
#
######################################################################

# This basic implementation library should be generic enough to use
# on any Linux-based processor board, as well as a standard development PC.
add_library(osal_generic-phoenix_impl OBJECT
	src/bsp_start.c
	src/bsp_console.c
)

# This BSP only works with "phoenix" OS layer.
# Confirming this reduces risk of accidental misconfiguration
set_property(TARGET osal_generic-phoenix_impl PROPERTY OSAL_EXPECTED_OSTYPE "phoenix")
