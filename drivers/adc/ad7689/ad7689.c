#include <stdlib.h>
#include "ad7689.h"
#include "util.h"
#include "error.h"
#include "print_log.h"
#include "delay.h"

static void _ad7689_config_put(struct ad7689_dev *dev, struct ad7689_config *config)
{
	dev->configs[1] = dev->configs[0];
	if (config)
		dev->configs[0] = *config;
	else
		dev->configs[0] = dev->configs[1];
}

static struct ad7689_config *_ad7689_config_get(struct ad7689_dev *dev)
{
	// dev->configs[1] is the config currently in use. If the current
	// SPI transaction is numbered N, this config was written at N-2
	// and applied at the EOC of N-1.
	return &dev->configs[1];
}

static int32_t _ad7689_rac(struct ad7689_dev *dev, struct ad7689_config *config_in, struct ad7689_config *config_out, uint16_t *data)
{
	int32_t ret;
	uint16_t cfg = 0;
	uint8_t buf[4] = {0, 0, 0, 0};
	uint16_t sz;
	struct ad7689_config *c;

	if (!dev)
		return -EINVAL;

	c = _ad7689_config_get(dev);

	if (config_in) {
		cfg |= field_prep(AD7689_CFG_CFG_MSK, 1);
		cfg |= field_prep(AD7689_CFG_INCC_MSK, config_in->incc);
		cfg |= field_prep(AD7689_CFG_INX_MSK, config_in->inx);
		cfg |= field_prep(AD7689_CFG_BW_MSK, config_in->bw);
		cfg |= field_prep(AD7689_CFG_REF_MSK, config_in->ref);
		cfg |= field_prep(AD7689_CFG_SEQ_MSK, config_in->seq);
		cfg |= field_prep(AD7689_CFG_RB_MSK, config_in->rb);
		cfg <<= 2;
		buf[0] = cfg >> 8;
		buf[1] = cfg;
	}
	sz = !c->rb && config_out ? 4 : 2;
	ret = spi_write_and_read(dev->spi_desc, buf, sz);
	if (ret < 0)
		return ret;
	
	_ad7689_config_put(dev, config_in);

	if (data)
		*data = ((uint16_t)buf[0] << 8) | buf[1];

	if (!c->rb && config_out) {
		cfg = ((uint16_t)buf[2] << 8) | buf[2];
		cfg >>= 2;
		config_out->incc = field_get(AD7689_CFG_INCC_MSK, cfg);
		config_out->inx = field_get(AD7689_CFG_INX_MSK, cfg);
		config_out->bw = field_get(AD7689_CFG_BW_MSK, cfg);
		config_out->ref = field_get(AD7689_CFG_REF_MSK, cfg);
		config_out->seq = field_get(AD7689_CFG_SEQ_MSK, cfg);
		config_out->rb = field_get(AD7689_CFG_RB_MSK, cfg);
	}

	return SUCCESS;
}

int32_t ad7689_init(struct ad7689_dev **dev, struct ad7689_init_param *init_param)
{
        struct ad7689_dev *d;
	int32_t ret;

	d = (struct ad7689_dev *)calloc(1, sizeof(*d));
	if (!d)
		return -ENOMEM;

	d->id = init_param->id;

        ret = spi_init(&d->spi_desc, &init_param->spi_init);
	if (ret < 0)
		goto error;

	// perform two dummy conversions
        ret = ad7689_write_config(d, &init_param->config);
	if (ret < 0)
		goto error;

	ret = ad7689_write_config(d, &init_param->config);
	if (ret < 0)
		goto error;
	
	*dev = d;

        return SUCCESS;
error:
	pr_err("ad7689 initialization failed with status %d\n", ret);
	ad7689_remove(d);
	return ret;

}

int32_t ad7689_write_config(struct ad7689_dev *dev, struct ad7689_config *config)
{
	return _ad7689_rac(dev, config, NULL, NULL);
}

int32_t ad7689_read_config(struct ad7689_dev *dev, struct ad7689_config *config)
{
	int32_t ret = 0;
	struct ad7689_config *c = _ad7689_config_get(dev);
	if (c->rb == false)
		return _ad7689_rac(dev, NULL, config, NULL);

	struct ad7689_config c_in = *c;
	c_in.rb = false;
	ret = _ad7689_rac(dev, &c_in, NULL, NULL);
	if (ret < 0)
		return ret;

	c_in.rb = true;
	ret = _ad7689_rac(dev, &c_in, NULL, NULL);
	if (ret < 0)
		return ret;

	return _ad7689_rac(dev, NULL, config, NULL);
}

int32_t ad7689_read(struct ad7689_dev *dev, uint16_t *data, uint32_t nb_samples)
{
	int32_t ret;
	uint32_t i = 0;

	if (!data)
		return -EINVAL;
	if (!nb_samples)
		return SUCCESS;

	do {
		ret = _ad7689_rac(dev, NULL, NULL, &data[i]);
		if (ret < 0)
			return ret;
		i++;
	} while (i < nb_samples);

	return SUCCESS;
}

int32_t ad7689_start(struct ad7689_dev *dev)
{
	return 0;
}

int32_t ad7689_remove(struct ad7689_dev *dev)
{
        if (!dev)
                return -EINVAL;

        spi_remove(dev->spi_desc);
        free(dev);

        return SUCCESS;
}