/***************************************************************************//**
 *   @file   adf4377.c
 *   @brief  Implementation of adf4377 Driver.
 *   @author Antoniu Miclaus (antoniu.miclaus@analog.com)
********************************************************************************
 * Copyright 2021(c) Analog Devices, Inc.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  - Neither the name of Analog Devices, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *  - The use of this software may or may not infringe the patent rights
 *    of one or more patent holders.  This license does not release you
 *    from the requirement that you obtain separate licenses from these
 *    patent holders to use this software.
 *  - Use of the software either in source or binary form, must be run
 *    on or directly connected to an Analog Devices Inc. component.
 *
 * THIS SOFTWARE IS PROVIDED BY ANALOG DEVICES "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, NON-INFRINGEMENT,
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL ANALOG DEVICES BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, INTELLECTUAL PROPERTY RIGHTS, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/

/******************************************************************************/
/***************************** Include Files **********************************/
/******************************************************************************/
#include <malloc.h>
#include "adf4377.h"
#include "error.h"
#include "delay.h"

/******************************************************************************/
/************************** Functions Implementation **************************/
/******************************************************************************/

/**
 * @brief Writes data to ADF4377 over SPI.
 * @param dev - The device structure.
 * @param reg_addr - The register address.
 * @param data - Data value to write.
 * @return Returns SUCCESS in case of success or negative error code otherwise.
 */
int32_t adf4377_spi_write(struct adf4377_dev *dev, uint8_t reg_addr,
			  uint8_t data)
{
	uint8_t buff[ADF4377_BUFF_SIZE_BYTES];

	if (dev->spi_desc->bit_order) {
		buff[0] = bit_swap_constant_8(reg_addr);
		buff[1] = bit_swap_constant_8(ADF4377_SPI_WRITE_CMD);
		buff[2] = bit_swap_constant_8(data);
	} else {
		buff[0] = ADF4377_SPI_WRITE_CMD;
		buff[1] = reg_addr;
		buff[2] = data;
	}

	return spi_write_and_read(dev->spi_desc, buff, ADF4377_BUFF_SIZE_BYTES);
}

/**
 * @brief Update ADF4377 register.
 * @param dev - The device structure.
 * @param reg_addr - The register address.
 * @param mask - Mask for specific register bits to be updated.
 * @param data - Data read from the device.
 * @return Returns SUCCESS in case of success or negative error code otherwise.
 */
int32_t adf4377_update(struct adf4377_dev *dev, uint8_t reg_addr,
		       uint8_t mask, uint8_t data)
{
	uint8_t read_val;
	int32_t ret;

	ret = adf4377_spi_read(dev, reg_addr, &read_val);
	if (ret != SUCCESS)
		return ret;

	read_val &= ~mask;
	read_val |= data;

	ret = adf4377_spi_write(dev, reg_addr, read_val);

	return ret;
}

/**
 * @brief Reads data from ADF4377 over SPI.
 * @param dev - The device structure.
 * @param reg_addr - The register address.
 * @param data - Data read from the device.
 * @return Returns SUCCESS in case of success or negative error code otherwise.
 */
int32_t adf4377_spi_read(struct adf4377_dev *dev, uint8_t reg_addr,
			 uint8_t *data)
{
	int32_t ret;
	uint8_t buff[ADF4377_BUFF_SIZE_BYTES];

	if (dev->spi_desc->bit_order) {
		buff[0] = bit_swap_constant_8(reg_addr);
		buff[1] = bit_swap_constant_8(ADF4377_SPI_READ_CMD);
		buff[2] = bit_swap_constant_8(ADF4377_SPI_DUMMY_DATA);
	} else {
		buff[0] = ADF4377_SPI_READ_CMD;
		buff[1] = reg_addr;
		buff[2] = ADF4377_SPI_DUMMY_DATA;
	}

	ret = spi_write_and_read(dev->spi_desc, buff, ADF4377_BUFF_SIZE_BYTES);
	if(ret != SUCCESS)
		return ret;

	*data = buff[2];

	return ret;
}

/**
 * @brief ADF4377 SPI Scratchpad check.
 * @param dev - The device structure.
 * @return Returns SUCCESS in case of success or negative error code.
 */
int32_t adf4377_check_scratchpad(struct adf4377_dev *dev)
{
	int32_t ret;
	uint8_t scratchpad;

	ret = adf4377_spi_write(dev, ADF4377_REG(0x0A), ADF4377_SPI_SCRATCHPAD);
	if (ret != SUCCESS)
		return ret;

	ret = adf4377_spi_read(dev, ADF4377_REG(0x0A), &scratchpad);
	if (ret != SUCCESS)
		return ret;

	if(scratchpad != ADF4377_SPI_SCRATCHPAD)
		return FAILURE;

	return SUCCESS;
}

/**
 * @brief Software reset.
 * @param dev - The device structure.
 * @return Returns SUCCESS in case of success or negative error code.
 */
int32_t adf4377_soft_reset(struct adf4377_dev *dev)
{
	int32_t ret;
	uint16_t timeout = 0xFFFF;
	uint8_t data;

	ret = adf4377_update(dev, ADF4377_REG(0x00),
			     ADF4377_SOFT_RESET_MSK | ADF4377_SOFT_RESET_R_MSK,
			     ADF4377_SOFT_RESET(ADF4377_SOFT_RESET_EN) | ADF4377_SOFT_RESET_R(
				     ADF4377_SOFT_RESET_EN));
	if (ret != SUCCESS)
		return ret;

	while(timeout) {
		ret = adf4377_spi_read(dev, ADF4377_REG(0x00), &data);
		if (ret != SUCCESS)
			return ret;

		if(!(data & ADF4377_SOFT_RESET(ADF4377_SOFT_RESET_EN)))
			return SUCCESS;
	}

	return FAILURE;
}

/**
 * @brief Initializes the ADF4377.
 * @param device - The device structure.
 * @param init_param - The structure containing the device initial parameters.
 * @return Returns SUCCESS in case of success or negative error code.
 */
int32_t adf4377_init(struct adf4377_dev **device,
		     struct adf4377_init_param *init_param)
{
	int32_t ret;
	uint8_t chip_type;
	struct adf4377_dev *dev;

	dev = (struct adf4377_dev *)calloc(1, sizeof(*dev));
	if (!dev)
		return -ENOMEM;

	dev->spi3wire = init_param->spi3wire;

	/* GPIO Chip Enable */
	ret = gpio_get(&dev->gpio_ce, init_param->gpio_ce_param);
	if (ret != SUCCESS)
		goto error_dev;

	ret = gpio_direction_output(dev->gpio_ce, GPIO_HIGH);
	if (ret != SUCCESS)
		goto error_dev;

	/* SPI */
	ret = spi_init(&dev->spi_desc, init_param->spi_init);
	if (ret != SUCCESS)
		goto error_gpio;

	/* Software Reset */
	ret = adf4377_soft_reset(dev);
	if (ret != SUCCESS)
		goto error_spi;

	ret = adf4377_spi_write(dev, ADF4377_REG(0x00),
				ADF4377_LSB_FIRST_R(dev->spi_desc->bit_order) |
				ADF4377_LSB_FIRST(dev->spi_desc->bit_order) |
				ADF4377_SDO_ACTIVE_R(dev->spi3wire) |
				ADF4377_SDO_ACTIVE(dev->spi3wire));
	if (ret != SUCCESS)
		goto error_spi;

	ret = adf4377_spi_read(dev, ADF4377_REG(0x03), &chip_type);
	if (ret != SUCCESS)
		goto error_spi;

	if (chip_type != ADF4377_CHIP_TYPE)
		return FAILURE;

	ret = adf4377_check_scratchpad(dev);
	if (ret != SUCCESS)
		goto error_spi;

	*device = dev;

	return ret;

error_spi:
	spi_remove(dev->spi_desc);

error_gpio:
	gpio_remove(dev->gpio_ce);

error_dev:
	free(dev);

	return ret;
}

/**
 * @brief Free resoulces allocated for ADF4377
 * @param dev - The device structure.
 * @return Returns SUCCESS in case of success or negative error code.
 */
int32_t adf4377_remove(struct adf4377_dev *dev)
{
	int32_t ret;

	ret = spi_remove(dev->spi_desc);
	if (ret != SUCCESS)
		return ret;

	ret = gpio_remove(dev->gpio_ce);

	free(dev);

	return ret;
}
