/*
 * Traspi GPIO Module
 * (2012) Mirco Gysin
 *
 *  Based on 74x164 Driver from
 *  Copyright (C) 2010 Gabor Juhos <juhosg@openwrt.org>
 *  Copyright (C) 2010 Miguel Gaio <miguel.gaio@efixo.com>
 */
 
#include <linux/types.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/spi/spi.h>
#include <linux/of.h>
#include <linux/init.h>
#include <linux/pm.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/delay.h>

struct traspi_chip_platform_data {
	
	unsigned	base;
};

struct traspi_chip {
	struct spi_device	*spi;
	struct gpio_chip	gpio_chip;
	char			phys[32];
	char			name[32];
	struct mutex		lock;
	u8			port_config;
};

static void traspi_set_value(struct gpio_chip *,unsigned, int);
static int update_traspi_output(struct traspi_chip *chip);

static struct traspi_chip *gpio_to_chip(struct gpio_chip *gc)
{
	return container_of(gc, struct traspi_chip, gpio_chip);
}

static void traspi_set_value(struct gpio_chip *gc,
		unsigned offset, int val)
{
	struct traspi_chip *chip = gpio_to_chip(gc);

	mutex_lock(&chip->lock);
	if (val)
		chip->port_config |= (1 << offset);
	else
		chip->port_config &= ~(1 << offset);

	update_traspi_output(chip);
	mutex_unlock(&chip->lock);
}

static int update_traspi_output(struct traspi_chip *chip)
{
	return spi_write(chip->spi,
			 &chip->port_config, sizeof(chip->port_config));
}

static int __devinit traspi_probe(struct spi_device *spi)
{
	struct traspi_chip *chip;
	struct traspi_chip_platform_data *pdata;
	int ret;	
	struct device_node *np = spi->dev.of_node, *child;
	int count = 0;


	pdata = kzalloc(sizeof(struct traspi_chip_platform_data), GFP_KERNEL);
	
	spi->dev.platform_data = pdata;
	
	pdata->base = 12;
	printk("TRASPI: PROBE\n");
	
	if(pdata == NULL)
	{
		printk("PDATA IS NULL\n");
		return -EINVAL;
	}
	
	if (!pdata || !pdata->base) {
		printk("TRASPI: incorrect or missing platform data\n");
		return -EINVAL;
	}

	spi->bits_per_word = 8;

	ret = spi_setup(spi);
	if (ret < 0)
		return ret;

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	//init mutex 
	mutex_init(&chip->lock);
	
	dev_set_drvdata(&spi->dev, chip);

	chip->spi = spi;

	chip->gpio_chip.label = "my_traspi_label",
	chip->gpio_chip.base = pdata->base;
	chip->gpio_chip.ngpio = 8;
	chip->gpio_chip.set = traspi_set_value;
	chip->gpio_chip.can_sleep = 1;
	chip->gpio_chip.dev = &spi->dev;
	chip->gpio_chip.owner = THIS_MODULE;

	chip->port_config = 0x55;	
	ret = update_traspi_output(chip);
	
	mdelay(1000);
	
	chip->port_config = 0xAA;	
	ret = update_traspi_output(chip);
	
	mdelay(1000);
	
	chip->port_config = 0x55;	
	ret = update_traspi_output(chip);
	
	mdelay(1000);
	if (ret) {
		printk("TRASPI: Failed writing: %d\n", ret);
		goto exit_destroy;
	}

	ret = gpiochip_add(&chip->gpio_chip);
	if (ret)
		goto exit_destroy;

	return ret;

exit_destroy:
	dev_set_drvdata(&spi->dev, NULL);
	mutex_destroy(&chip->lock);
	kfree(chip);
	return ret;	
}

static int traspi_suspend(struct device *dev)
{
	return -EINVAL;
}


static int __devinit traspi_of_probe(struct spi_device *spi,
	struct traspi_chip *tc)
{		
	return 0;
}

static int __devexit traspi_remove(struct spi_device *spi)
{
	return 0;
}

static const struct of_device_id traspi_match[] = {
	{ .compatible = "mg,traspi", },
	{},
}
MODULE_DEVICE_TABLE(of, traspi_match);

static SIMPLE_DEV_PM_OPS(traspi_pm, traspi_suspend, traspi_resume);


static struct spi_driver traspi_driver = {
	.driver = {
		.name   = "traspi",
		.bus    = &spi_bus_type,
		.owner  = THIS_MODULE,
		.pm     = &traspi_pm,
		.of_match_table = traspi_match,
	},
	.probe		= traspi_probe,
	.remove		= __devexit_p(traspi_remove),
};

static int __init traspi_init(void)
{
	printk("init traspi driver\n");
	return spi_register_driver(&traspi_driver);
}
module_init(traspi_init);

static void __exit traspi_exit(void)
{
	spi_unregister_driver(&traspi_driver);
}
module_exit(traspi_exit);

MODULE_DESCRIPTION("TRASPI GPIO DRIVER");
MODULE_LICENSE("GPL");
