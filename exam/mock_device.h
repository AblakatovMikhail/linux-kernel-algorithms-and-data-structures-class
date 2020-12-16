#include <linux/types.h>

#include <stdint.h>

#define MOCK_THREAD_NAME "mock_device"

struct mock_device {
	uintptr_t fetch_addr;
	size_t fetch_size;

	uintptr_t store_addr;
	size_t store_size;

	union {
		struct {
			uint32_t start_fetching : 1;
		} fields;
		uint32_t reg;
	} control;

	union {
		struct {
			uint32_t fetch_in_progress : 1;
			uint32_t store_in_progress : 1;

			uint32_t store_overflow : 1;

			struct {
				uint32_t fetch_addr : 1;
				uint32_t store_addr : 1;
			} err;

			uint32_t finished : 1;
		} fields;
		uint32_t reg;
	} status;
};

int mock_device_init(struct mock_device *mck_dev);

int mock_device_start(struct mock_device *mck_dev,
		      uintptr_t fetch_addr, size_t fetch_size,
		      uintptr_t store_addr, size_t store_size);
int mock_device_stop(struct mock_device *mck_dev);

int mock_device_fetch_in_progress(struct mock_device *mck_dev);
int mock_device_store_in_progress(struct mock_device *mck_dev);

int mock_device_store_overflown(struct mock_device *mck_dev);
int mock_device_store_continue(uintptr_t store_adr, size_t store_size);

int mock_device_error(struct mock_device *mck_dev);

int mock_device_finished(struct mock_device *mck_dev);

int mock_device_reset(struct mock_device *mck_dev);
