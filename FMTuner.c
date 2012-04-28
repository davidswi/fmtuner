// File: FMTuner.c -- interface implementation for Javascript FMTuner object
// Author: David Switzer
// Project: FmTuner, WebKit-based FM tuner UI
// (c) 2012, David Switzer

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

#include "videodev.h"
#include "FMTuner.h"

// device fspec for Si470x FM tuner driver
#define SI470X_DEVICE		"/dev/radio0"
// tuner IDs
#define PRIMARY_TUNER_ID	0

// FM radio region enum
enum fm_region
{
	FM_REGION_AMERICAS,
	FM_REGION_EUAFRICA,
	FM_REGION_JPN,
	FM_REGION_OTHER		// 50kHz spacing
}; 

// FMTuner object state management
struct fm_tuner_state
{	
	int tuner_fd, ret;
	struct video_audio aud_info;
	struct video_tuner tuner_info;

	// Current region
	enum fm_region region;

	// Current station properties	
	float freq; // Fequency in MHz (e.g. 101.5)
	char PICode[6]; // PI code represented in decimal string form
	char PS[9]; // Program Service (max. 8 chars + NULL terminator)		
	char PTY[3]; // Program Type code (0-31) in decimal string form
	char PTYN[9]; // Program Type Name (max. 8 chars + NULL terminator)
	char RT[65]; // Radio text (max. 64 chars + NULL terminator) 	
};

// private functions
bool is_valid_freq(struct fm_tuner_state *tuner_state, float freq);
int tuner_set_region(struct fm_tuner_state *tuner_state, enum fm_region region);
int tuner_set_freq(struct fm_tuner_state *tuner_state, float freq);
int tuner_set_volume(struct fm_tuner_state *tuner_state, int volume);

bool is_valid_freq(struct fm_tuner_state *tuner_state, float freq)
{
	bool valid_freq = false;	

	// We assume the low and high range values for the tuner have been set
	// for the current region and we check that the frequency falls in
	// the range
	if (freq >= tuner_state->tuner_info.rangelow && freq <= tuner_state->tuner_info.rangehigh)
	{
		// If the current region is Americas, we ensure the frequency is centered on an
		// odd kHz mark
		float wholeMHz = floor(freq);
		int step = ((int)(freq - wholeMHz)) * 10;
		if (tuner_state->tuner_info.region == FM_REGION_AMERICAS)
		{
			if (step % 2 > 0)
			{
				valid_freq = true;
			}
		}
		else
		{
			valid_freq = true;
		}
	}
	
	return valid_freq;				
}

int tuner_set_region(struct fm_tuner_state *tuner_state, enum fm_region region)
{
	// Looks like we need some additional plumbing in the Si470x driver to change band
	// and spacing from userland. I'll do a bit more digging and if that's the case,
	// I'll add the logic to the driver for this. For now, let's just return a reasonable
	// error
	return ENOSYS;
}
	

int tuner_set_freq(struct fm_tuner_state *tuner_state, float freq)
{
	int ret = ERANGE;

	// Validate the frequency
	if (is_valid_freq(tuner_state, freq)
	{
		// Tell the tuner to switch to this station
	}

	return ret;
}
// Initialization/finalization

void FMTuner_initCB(JSContextRef ctx, JSObjectRef object)
{
	struct fm_tuner_state *tuner_state = (struct fm_tuner_state *)JSObjectGetPrivate(object);
	int fd;

	if (fm_tuner_state != NULL)
	{
		// Attempt to open the FM tuner driver
		fd = open(SI470X_DEVICE, O_RDONLY);
		if (fd < 0)
		{
			perror("FMTuner_initCB -- failed to open tuner driver");
 		}
		else
		{
			// Set the tuner handle
			tuner_fd = fd;			
			// Get the current audio info
			ret = ioctl(tuner_fd, VIDIOCGAUDIO, &(tuner_state->aud_info);
			if (ret < 0)
			{
				perror("FMTuner_initCB -- ioctl VIDIOCGAUDIO failed");
			}
			else
			{
				// Set tuner number
				tuner_state->tuner_info.tuner = PRIMARY_TUNER_ID;
				ret = ioctl(tuner_fd, VIDIOCGTUNER, &(tuner_state->tuner_info);
				if (ret < 0)
				{
					perror("FMTuner_initCB -- ioctl VIDIOCGTUNER failed");
				}
			}	
		}
	}	
}

void FMTuner_finalizeCB(JSObjectRef object)
{
	struct fm_tuner_state *tuner_state = (struct fm_tuner_state *)JSObjectGetPrivate(object);
	
	if (tuner_state != NULL)
	{
		if (tuner_state->tuner_fd != NULL)
		{		
			close(tuner_state->tuner_fd);
		}
		free(tuner_state);
	}
}

// Property get/set

// FMTuner has the following properties that cause tuner driver requests on write:
// Frequency (R/W) for getting/setting the current frequency
// Region (R/W) for getting/setting the current frequency band and spacing
// Volume (R/W) on a scale from 0-100 to control the audio output volume for the radio source
// There are also RDS accessor properties:
// PICode (read-only) for reading the current frequency's PI code
// PS (read-only) for reading the Program Service Name (e.g. KISS FM)
// PTY (read-only) for reading the Program Type code (e.g. 14 for Jazz in North America, Classical in Europe)
// PTYN (read-only) for reading the Program Type Name (e.g. Concert)
// RT (read-only) for reading the Radio Text string (e.g. Wynton Marsalis Live on Bourbon Street)  

bool FMTuner_hasPropCB(JSContextRef ctx, JSObjectRef object, JSStringRef propName)
{
	if (JSStringIsEqualToUTF8CString(propertyName, "Frequency") ||
	    JSStringIsEqualToUTF8CString(propertyName, "PICode") ||
	    JSStringIsEqualToUTF8CString(propertyName, "PS") ||
	    JSStringIsEqualToUTF8CString(propertyName, "PTY") ||
	    JSStringIsEqualToUTF8CString(propertyName, "PTYN") ||
	    JSStringIsEqualToUTF8CString(propertyName, "RT"))
	{
		return true;
	}
	else
	{
		return false;
	}
}	    

JSValueRef FMTuner_getPropCB(JSContextRef ctx, JSObjectRef object, JSStringRef propName, JSValueRef *exception)
{
	struct fm_tuner_state *tuner_state = (struct fm_tuner_state *)JSObjectGetPrivate(object);	

	if (JSStringIsEqualToUTF8CString(propName, "Frequency"))
	{
		return JSValueMakeNumber(ctx, tuner_state->freq);
	}
	
	if (JSStringIsEqualToUTF8CString(propName, "PICode"))
	{		
		JSStringRef PICodeStr = JSStringCreateWithUTF8CString(tuner_state->PICode);

		return JSValueMakeString(ctx, PICodeStr);
	}

	if (JSStringIsEqualToUTF8CString(propName, "PS"))
	{
		JSStringRef PSStr = JSStringCreateWithUTF8CString(tuner_state->PS);

		return JSValueMakeString(ctx, PSStr);
	}
	
	if (JSStringIsEqualToUTF8CString(propName, "PTY"))
	{
		JSStringRef PTYStr = JSStringCreateWithUTF8CString(tuner_state->PTY);
		
		return JSValueMakeString(ctx, PTYStr);
	}	
	
	if (JSStringIsEqualToUTF8CString(propName, "PTYN"))
	{ 
		JSStringRef PTYNStr = JSStringCreateWithUTF8CString(tuner_state->PTYN);

		return JSValueMakeString(ctx, PTYNStr);
	}

	if (JSStringIsEqualToUTF8CString(propName, "RT"))
	{
		JSStringRef RTStr = JSStringCreateWithUTF8CString(tuner_state->RT);

		return JSValueMakeString(ctx, RTStr);
	}
}


bool FMTuner_setPropCB(JSContextRef ctx, JSObjectRef object, JSStringRef propName, JSValueRef value, JSValueRef *exception)
{
	struct fm_tuner_state *tuner_state = (struct fm_tuner_state *)JSObjectGetPrivate(object);	
	
	// Only the frequency can be set, and when it is, we execute a tuning request
	if (JSStringIsEqualToUTF8CString(propName, "Frequency"))
	{
		float req_freq = JSValueToNumber(value);
		
		// Kick off a request to the tuner and set the property if the request succeeds
		if (tuner_set_freq(tuner_state, req_freq)
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
}

// Methods
// The methods exposed by FMTuner are:
// Power(on/off)
// Seek(direction)
// Scan(on/off)

JSValueRef FMTuner_callAsFnCB(JSContextRef ctx, JSObjectRef thisObject, size_t argCount, const JSValueRef arguments[], JSValueRef *exception)
{	
}

// Constructor
JSObjectRef FMTuner_callAsCtorCB(JSContextRef ctx, JSObjectRef thisObject, size_t argCount, const JSValueRef arguments[], JSValueRef *exception)
{
}

// Instance
bool FMTuner_hasInstCB(JSContextRef ctx, JSObjectRef ctor, JSValueRef possibleInst, JSValueRef *exception)
{
}

// WebKit registration
int FMTuner_addClass(JSGlobalContextRef ctx)
{
}
