zephyr_sources_ifdef(CONFIG_SPI_DW		spi_dw.c)
zephyr_sources_ifdef(CONFIG_SPI_INTEL		spi_intel.c)
zephyr_sources_ifdef(CONFIG_SPI_STM32		spi_ll_stm32.c)
zephyr_sources_ifdef(CONFIG_SPI_MCUX_DSPI	spi_mcux_dspi.c)
zephyr_sources_ifdef(CONFIG_SPI_SAM0		spi_sam0.c)
zephyr_sources_ifdef(CONFIG_NRFX_SPI		spi_nrfx_spi.c)

zephyr_sources_ifdef(CONFIG_USERSPACE		spi_handlers.c)
