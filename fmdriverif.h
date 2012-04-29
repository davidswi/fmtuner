// File: fmdriver_if.h -- FM tuner driver userland interface
// Author: David Switzer
// Project: FmTuner, WebKit-based FM tuner UI
// (c) 2012, David Switzer

#ifndef FMDRIVERIF_H
#define FMDRIVERIF_H

#include <pthread.h>
#include <stdbool.h>

enum fmdriver_power_state
{
	FM_POWER_OFF,
	FM_POWER_ON,
	FM_POWER_REBOOT,
	FM_POWER_SLEEP,
	FM_POWER_WAKE
};

enum fmdriver_event_id
{
	FM_EVENT_POWER,
	FM_EVENT_TUNE,
	FM_EVENT_SEEK,
	FM_EVENT_SCAN,
	FM_EVENT_VOL,
	FM_EVENT_RDS
};

enum rds_field
{
	RDS_FIELD_PS,
	RDS_FIELD_PI,
	RDS_FIELD_PTY,
	RDS_FIELD_PTYN,
	RDS_FIELD_RT
};

struct rds_data
{
	enum rds_field field;
	int data_length;
	unsigned char *data;
};		

// Since all tuner requests are async and RDS data arrives async as well, we define
// an event payload struct for the FM driver interface. The interface client provides a
// pthread condition variable to be signalled on when a driver event occurs 
struct fmdriver_event
{
	enum fmdriver_event_id event_id;
	int status_code;
	int data_len;
	unsigned char *event_data;
};		

// Driver open/close
// On open, the client provides a pthread condition variable for receiving callbacks on
// If the condition callback is NULL, all requests will block until complete.
int fmdriverif_open(int tuner_id, pthread_cond_t *callback_cond, int *if_handle_ptr);
int fmdriverif_close(int if_handle);

// Driver requests -- async unless the interface was opened with a NULL condition variable
int fmdriverif_powerrequest(int if_handle, enum fmdriver_power_state req_state);
int fmdriverif_tunerequest(int if_handle, int tune_freq);
int fmdriverif_seekrequest(int if_handle, bool seek_up);
int fmdriverif_scanrequest(int if_handle, bool stop_scan);
int fmdriverif_volrequest(int if_handle, int vol_level); // 0-100

// Event data access -- reads the next event from the event FIFO. Typically, the client will 
// have a worker thread that waits until its condition variable is signalled, then calls this
// function to retrieve the event data
int fmdriverif_read_event(int if_handle, struct fmdriver_event *event);

#endif

