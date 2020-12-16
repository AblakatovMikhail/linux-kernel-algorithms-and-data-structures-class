#include <linux/list.h>

struct mock_driver_data {
	struct list_head users;
	struct mutex users_lock;
};

/* Создаёт элемент пользователя и добавляет его в общий список */
static int mock_open (struct inode * inode, struct file * file);
/* Удаляет элемент пользователя из списка и освобождает его */
static int mock_release (struct inode * inode, struct file * file);

/*
 * Если store окончен, что проверяется mock_device_store_in_progress(),
 * считывает данные, сгенерированные устройством. Если предоставленной
 * устройству для записи памяти оказалось недостаточно, что проверяется
 * mock_device_store_overflown(), обрабатывает ситуацию и возобновляет запись
 * с помощью mock_device_store_continue(). Возвращает пользователю все
 * записанные устройством данные.
 */
static ssize_t mock_read (struct file * file, char __user * user_buffer,
			  size_t size, loff_t * offset);
/*
 * Читает переданные пользователем данные, подготавливает данные для
 * устройства, вызывает mock_device_fetch_start(), что приводит к асинхронной
 * обработке устройством (эмулируется потоком ядра). Возвращается после
 * окончания fetch, проверяется с помощью mock_device_fetch_in_progress().
 */
static ssize_t mock_write (struct file * file, const char __user * user_buffer,
			   size_t size, loff_t * offset);
