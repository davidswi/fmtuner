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
#define IFSTATE_GOOD		0xDCBAABCD

// Driver state definition
struct fmdriverif_state
{	
	int sig;				// Magic number indicating struct is good
	pthread_cond_t *cond;			// Interface client condition callback
	pthread_mutex_t event_fifo_mutex;	// Mutex controlling access to event fifo state
	sem_t event_fifo_slots;			// Counting semaphore controlling read/write from/to fifo

	struct fmdriver_event *event_fifo[EVENT_FIFO_CAPACITY];	
	int event_fifo_head;			// Next write slot
	int event_fifo_tail;			// Next read slot
	bool fifo_clear;			// If true, we are clearing the FIFO 
	
	int tuner_fd;				// File system handle to tuner driver
};


// FIFO functions
int fifo_enqueue(struct fmdriverif_state *driver_state, struct fmdriver_event *evt);
int fifo_dequeue(struct fmdriverif_state *driver_state, struct fmdriver_event *evt);
int fifo_clear(struct fmdriverif_state *driver_state);

// Private methods
int fifo_enqueue(struct fmdriverif_state *driver_state, struct fmdriver_event *evt)
{
	int ret;
	
	// Acquire a free slot
	ret = sem_wait(&(driver_state->event_fifo_slots));
	if (ret != 0)
	{
		perror("fifo_enqueue() -- failed to wait on FIFO slot");
		return errno;
	}

	// lock access to the fifo state
	ret = pthread_mutex_lock(&(driver_state->event_fifo_mutex));
	if (ret != 0)
	{
		fprintf(stderr, "fifo_enqueue() -- failed on pthread_mutex_lock %d\n", ret);
		return ret;
	}

	// Don't enqueue if the fifo has been cleared -- we are probably shutting down the interface
	if (!driver_state->fifo_clear)
	{
		// Put the event at the back of the fifo
		driver_state->event_fifo[driver_state->event_fifo_tail] = evt;
		// Move tail index -- modulo takes care of wrapping and semaphore count takes care of
		// overwriting
		driver_state->event_fifo_tail = (driver_state->event_fifo_tail + 1) % EVENT_FIFO_CAPACITY;
	}

	// Unlock access
	ret = pthread_mutex_unlock(&(driver_state->event_fifo_mutex));
	if (ret != 0)
	{
		fprintf(stderr, "fifo_enqueue() -- failed on pthread_mutex_unlock %d\n", ret);
	}

	return ret;
}

int fifo_dequeue(struct fmdriverif_state *driver_state, struct fmdriver_event *evt)
{
	int free_slots, ret;
 	
	// Find out whether we have any items to dequeue by getting the semaphore count
	ret = sem_getvalue(&(driver_state->event_fifo_slots), &free_slots);
	if (ret != 0)
	{
		perror("fifo_dequeue() -- failed on sem_getvalue()");
		return errno;
	}
	
	// Semaphore count < 0 means the fifo is full and the enqueue thread is blocked
	// waiting on a free slot, so we have to check for this condition 	

	if (free_slots < 0 || EVENT_FIFO_CAPACITY - free_slots > 0)
	{
		// There are items to dequeue, so lock fifo access
		ret = pthread_mutex_lock(&(driver_state->event_fifo_mutex));
		if (ret != 0)
		{
			fprintf(stderr, "fifo_dequeue() -- failed on pthread_mutex_lock %d\n", ret);
			return ret;
		}	
			
		// Take the event from the head
		evt = driver_state->event_fifo[driver_state->event_fifo_head];
		// Mark the pointer as NULL  -- consumer is responsible for freeing when done
		driver_state->event_fifo[driver_state->event_fifo_head] = NULL;	
		// Move the head to the next in line
		driver_state->event_fifo_head = (driver_state->event_fifo_head + 1) % EVENT_FIFO_CAPACITY;
			
		// Unlock fifo access
		ret = pthread_mutex_unlock(&(driver_state->event_fifo_mutex));
		if (ret != 0)
		{
			fprintf(stderr, "fifo_dequeue() -- failed on pthread_mutex_unlock %d\n", ret);
			return ret;
		}

		// Increment the semaphore because we've freed a slot
		ret = sem_post(&(driver_state->event_fifo_slots));
		if (ret != 0)
		{
			perror("fifo_dequeue() -- failed on sem_post");
			return errno;
		}
	}

	return ret;
}

int fifo_clear(struct fmdriverif_state *driver_state)
{
	int ret, free_slots, filled_slots;	

	ret = sem_getvalue(&(driver_state->event_fifo_slots), &free_slots);
	if (ret != 0)
	{
		perror("fifo_clear() -- failed on sem_getvalue()");
		return errno;
	}

	if (free_slots < 0)
	{
		filled_slots = EVENT_FIFO_CAPACITY;
	}
	else
	{
		filled_slots = EVENT_FIFO_CAPACITY - free_slots;
	}

	if (filled_slots > 0)
	{
		ret = pthread_mutex_lock(&(driver_state->event_fifo_mutex));
		if (ret != 0)
		{
			fprintf(stderr, "fifo_clear() -- failed on pthread_mutex_lock %d\n", ret);
			return ret;
		}	
					
		// Set clear flag to prevent further enqueueing
		driver_state->fifo_clear = true;

		// Step through the fifo, freeing the enqueued items
		while (filled_slots > 0)
		{
			// We free any event slots that are not NULL			
			if (driver_state->event_fifo[driver_state->event_fifo_head] != NULL)
			{
				struct fmdriver_event *evt = driver_state->event_fifo[driver_state->event_fifo_head];
				free(evt);
				driver_state->event_fifo[driver_state->event_fifo_head] = NULL;
			}
			driver_state->event_fifo_head = (driver_state->event_fifo_head + 1) % EVENT_FIFO_CAPACITY;
			filled_slots--;
		}			
		
		// Done, unlock
		ret = pthread_mutex_unlock(&(driver_state->event_fifo_mutex));
		if (ret != 0)
		{
			fprintf(stderr, "fifo_clear() -- failed on pthread_mutex_unlock %d\n", ret);
			return ret;
		}

		// And post to the semaphore to release the blocked enqueue thread
		ret = sem_post(&(driver_state->event_fifo_slots));
		if (ret != 0)
		{
			perror("fifo_clear() -- failed to post to fifo_semaphore");
			return errno;
		}
	}	
		
	return ret;
}

int fmdriverif_open(int tuner_id, pthread_cond_t *callback_cond, unsigned long *if_handle_ptr)
{
	char radio_driver_path[64];
	char radio_device_id[4];
	int fd, ret;

	// Set the interface handle pointer to 0 in case there is an error during open
	*if_handle_ptr = 0;
	
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

	// Init the FIFO mutex (use default attributes)
	ret = pthread_mutex_init(&(driver_state->event_fifo_mutex), NULL);
	if (ret != 0)
	{
		fprintf(stderr, "fmdriverif_open() -- failed to initialize event mutex %d\n", ret);
		return ret;
	}

	// Init the FIFO semaphore
	ret = sem_init(&(driver_state->event_fifo_slots), false, EVENT_FIFO_CAPACITY);
	if (ret != 0)
	{
		perror("fmdriverif_open() -- failed to initialize FIFO semaphore");
		return ret;
	}
	
	// Initialize FIFO head and tail
	driver_state->event_fifo_head = driver_state->event_fifo_tail = 0;
	// Not clearing fifo
	driver_state->fifo_clear = false;

	// Zero out the FIFO slots
	memset(driver_state->event_fifo, 0, (EVENT_FIFO_CAPACITY * sizeof(struct fmdriver_event)));

	// Set the good magic number
	driver_state->sig = IFSTATE_GOOD;

	// Cast the driver state pointer to an int and return to caller
	*if_handle_ptr = (unsigned long)driver_state; 		

	return 0;
}

int fmdriverif_close(unsigned long if_handle)
{	
	struct fmdriverif_state *driver_state;
	int ret;		
	
	if (if_handle == 0)
		return EINVAL;

	// Cast the handle to state pointer
	driver_state = (struct fmdriverif_state *)if_handle;

	// Check the sig
	if (driver_state->sig != IFSTATE_GOOD)
		return EINVAL;

	// Free any event structs that are sitting in the fifo
	ret = fifo_clear(driver_state);

	// Now the enqueue thread will not be waiting on the semaphore because
	// the fifo has been cleared and the clear flag is set, preventing any new
	// events from being enqueued. So we can safely destroy it.
	ret = sem_destroy(&(driver_state->event_fifo_slots));
	if (ret != 0)
	{
		perror("fmdriverif_close() -- failed on sem_destroy");
	}

	// Close the tuner driver handle
	ret = close(driver_state->tuner_fd);
	if (ret != 0)
	{
		perror("fmdriverif_close() -- failed on close");
	}

	// Destroy the mutex
	ret = pthread_mutex_destroy(&(driver_state->event_fifo_mutex));
	if (ret != 0)
	{
		fprintf(stderr, "fmdriverif_close() -- failed on pthread_mutex_destroy %d\n", ret);
	}

	// Free the driver state
	free(driver_state);		 	

	return ret;
}

int fmdriverif_powerrequest(unsigned long if_handle, enum fmdriver_power_state req_state)
{
	return 0;
}

int fmdriverif_tunerequest(unsigned long if_handle, int tune_freq)
{
	return 0;
}

int fmdriverif_seekrequest(unsigned long if_handle, bool seek_up)
{
	return 0;
}

int fmdriverif_scanrequest(unsigned long if_handle, bool stop_scan)
{
	return 0;
}

int fmdriverif_volrequest(unsigned long if_handle, int vol_level)
{
	return 0;
}

int fmdriverif_read_event(unsigned long if_handle, struct fmdriver_event *event)
{
	return 0;
}

// end of file

