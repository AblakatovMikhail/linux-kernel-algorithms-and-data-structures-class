#include <linux/module.h>

#include "mock_device.h"

static struct mock_device mock_device;

static int mock_driver_init(void)
{
	int error;

	error = mock_device_init(&mock_device);
	if (error)
		return error;

	return 0;
}

static void mock_driver_exit(void)
{
}

module_init(mock_driver_init);
module_exit(mock_driver_exit);
