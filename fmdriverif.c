// File: fmdriver_if.c -- FM tuner driver userland interface implementation
// Author: David Switzer
// Project: FmTuner, WebKit-based FM tuner UI
// (c) 2012, David Switzer

#include "fmdriverif.h"
#include <math.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <sys/ioctl.h>
#include <limits.h>
#include <semaphore.h>

#include "videodev.h"


// device fspec for FM tuner driver
#define RADIO_DEVICE		"/dev/radio"

#define EVENT_FIFO_CAPACITY	32

struct fmdriverif_state
{	
	pthread_cond_t *cond;			// Interface client condition callback
	pthread_mutex_t event_fifo_mutex;	// Mutex controlling access to event fifo state
	sem_t event_fifo_slots;			// Counting semaphore controlling read/write from/to fifo

	struct fmdriver_event *event_fifo;	
	int event_fifo_head;			// Next write slot
	int event_fifo_tail;			// Next read slot 
	
	int tuner_fd;				// File system handle to tuner driver
};

int fmdriverif_open(int tuner_id, pthread_cond_t *callback_cond, int *if_handle_ptr)
{
	char radio_driver_path[64];
	char radio_device_id[4];
	int fd, ret;
	
	if (tuner_id < 0 || tuner_id > 9)
		return EINVAL;

	// Convert the tuner id to a string
	sprintf(radio_device_id, "%d", tuner_id);

	// Construct the driver path with tuner_id
	strcpy(radio_driver_path, RADIO_DEVICE);
	strcat(radio_driver_path, radio_device_id);

	// Attempt to open the FM tuner driver
	fd = open(radio_driver_path, O_RDONLY);
	if (fd < 0)
	{
		perror("fmdriverif_open() -- failed to open tuner driver");
		return errno;
 	}

	// Attempt to allocate a driver state struct
	struct fmdriverif_state *driver_state = (struct fmdriverif_state *)malloc(sizeof(struct fmdriverif_state));
	if (driver_state == NULL)
	{		
		perror("fmdriverif_open() -- failed to allocate driver state");
		return ENOMEM;
	}

	// Set the tuner driver fd
	driver_state->tuner_fd = fd;

	// Set the condition variable
	driver_state->cond = callback_cond;

	// Allocate the FIFO
	driver_state->event_fifo = (struct fmdriver_event *)malloc(sizeof(struct fmdriver_event) * EVENT_FIFO_CAPACITY);
	if (driver_state->event_fifo == NULL)
	{
		perror("fmdriverif_open() -- failed to allocate event FIFO");
		return ENOMEM;
	}

	// Init the FIFO mutex (use default attributes)
	ret = pthread_mutex_init(&(driver_state->event_fifo_mutex), NULL);
	if (ret != 0)
	{
		perror("fmdriverif_open() -- failed to initialize event mutex");
		return ret;
	}

	// Init the FIFO semaphore
	ret = sem_init(&(driver_state->event_fifo_slots), false, EVENT_FIFO_CAPACITY);
	if (ret != 0)
	{
		perror("fmdriverif_open() -- failed to initialize FIFO semaphore");
	}
	
	// Initialize FIFO head and tail
	driver_state->event_fifo_head = driver_state->event_fifo_tail = 0;

	return 0;
}

int fmdriverif_close(int if_handle)
{
	return 0;
}

int fmdriverif_powerrequest(int if_handle, enum fmdriver_power_state req_state)
{
	return 0;
}

int fmdriverif_tunerequest(int if_handle, int tune_freq)
{
	return 0;
}

int fmdriverif_seekrequest(int if_handle, bool seek_up)
{
	return 0;
}

int fmdriverif_scanrequest(int if_handle, bool stop_scan)
{
	return 0;
}

int fmdriverif_volrequest(int if_handle, int vol_level)
{
	return 0;
}

int fmdriverif_read_event(int if_handle, struct fmdriver_event *event)
{
	return 0;
}

// end of file

