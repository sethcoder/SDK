/*===============================================================================================
 OUTPUT_MP3.DLL
 Copyright (c), Firelight Technologies Pty, Ltd 2004-2005.

 Shows how to write an FMOD output plugin that writes the output to an mp3 file using 
 lame_enc.dll.

 Most of this source code that does the encoding is taken from the LAME encoder example.
 An FMOD Ex output plugin is created by declaring the FMODGetOutputDescription function,
 then filling out a FMOD_OUTPUT_DESCRIPTION and returning a pointer to it.
 FMOD will then call those functions when appropriate.

 To get the output from FMOD so that you can write it to your sound device (or LAME encoder
 function in this case), FMOD_OUTPUT::readfrommixer is called to run the mixer.

 We acknowledge that we are using LAME, which originates from www.mp3dev.org.
 LAME is under the LGPL and as an external FMOD plugin with full source code for the interface
 it is allowable under the LGPL to be distributed in this fashion.

===============================================================================================*/

#include "../../../api/inc/fmod.h"
#include "../../../api/inc/fmod_output.h"
#include "../../../api/inc/fmod_errors.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>

#include "BladeMP3EncDll.h"

typedef struct
{
    FILE               *mFP;    
	HINSTANCE	        mDLL;
	HBE_STREAM	        hbeStream;
	PBYTE               pMP3Buffer;
	PSHORT              pWAVBuffer;
	BE_VERSION	        Version;
    DWORD		        dwMP3Buffer;
    DWORD               dwSamples;

    BEINITSTREAM        beInitStream;
    BEENCODECHUNK       beEncodeChunk;
    BEDEINITSTREAM      beDeinitStream;
    BECLOSESTREAM       beCloseStream;
    BEVERSION           beVersion;
    BEWRITEVBRHEADER    beWriteVBRHeader;

} OutputMP3;

FMOD_OUTPUT_DESCRIPTION mp3output;

FMOD_RESULT F_CALLBACK OutputMP3_GetNumDriversCallback(FMOD_OUTPUT *output, int *numdrivers);
FMOD_RESULT F_CALLBACK OutputMP3_GetDriverNameCallback(FMOD_OUTPUT *output, int id, char *name, int namelen);
FMOD_RESULT F_CALLBACK OutputMP3_InitCallback(FMOD_OUTPUT *output, int selecteddriver, FMOD_INITFLAGS flags, int *outputrate, int outputchannels, FMOD_SOUND_FORMAT *outputformat, int dspbufferlength, int dspnumbuffers, const void *hwnd);
FMOD_RESULT F_CALLBACK OutputMP3_CloseCallback(FMOD_OUTPUT *output);
FMOD_RESULT F_CALLBACK OutputMP3_UpdateCallback(FMOD_OUTPUT *output);
FMOD_RESULT F_CALLBACK OutputMP3_GetHandleCallback(FMOD_OUTPUT *output, void **handle);


#ifdef __cplusplus
extern "C" {
#endif

/*
    FMODGetOutputDescription is mandantory for every fmod plugin.  This is the symbol the registerplugin function searches for.
    Must be declared with F_API to make it export as stdcall.
*/
F_DECLSPEC F_DLLEXPORT FMOD_OUTPUT_DESCRIPTION * F_API FMODGetOutputDescription()
{
    memset(&mp3output, 0, sizeof(FMOD_OUTPUT_DESCRIPTION));

    mp3output.name          = "FMOD MP3 Output";
    mp3output.version       = 0x00010000;
    mp3output.polling       = 0;                                    /* False = no thread is created.  Plugin must drive FMOD's mixer */
    mp3output.getnumdrivers = OutputMP3_GetNumDriversCallback;
    mp3output.getdrivername = OutputMP3_GetDriverNameCallback;
    mp3output.getdrivercaps = 0;
    mp3output.init          = OutputMP3_InitCallback;
    mp3output.close         = OutputMP3_CloseCallback;
    mp3output.update        = OutputMP3_UpdateCallback;
    mp3output.gethandle     = OutputMP3_GetHandleCallback;
    mp3output.getposition   = 0;                                    /* Not a polling output so getposition is never called */
    mp3output.lock          = 0;                                    /* Not a polling output so lock is never called */
    mp3output.unlock        = 0;                                    /* Not a polling output so unlock is never called */

    return &mp3output;
}

#ifdef __cplusplus
}
#endif


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]
    FMOD_OK

	[REMARKS]

    [PLATFORMS]
    Win32, Win64, Linux, Macintosh, XBox, PlayStation 2, GameCube

	[SEE_ALSO]
]
*/
FMOD_RESULT F_CALLBACK OutputMP3_GetNumDriversCallback(FMOD_OUTPUT *output, int *numdrivers)
{
    *numdrivers = 1;

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]
    FMOD_OK

	[REMARKS]

    [PLATFORMS]
    Win32, Win64, Linux, Macintosh, XBox, PlayStation 2, GameCube

	[SEE_ALSO]
]
*/
FMOD_RESULT F_CALLBACK OutputMP3_GetDriverNameCallback(FMOD_OUTPUT *output, int id, char *name, int namelen)
{
    strncpy(name, "fmodoutput.mp3", namelen);

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]
    FMOD_OK

	[REMARKS]

    [PLATFORMS]
    Win32, Win64, Linux, Macintosh, XBox, PlayStation 2, GameCube

	[SEE_ALSO]
]
*/
FMOD_RESULT F_CALLBACK OutputMP3_InitCallback(FMOD_OUTPUT *output, int selecteddriver, FMOD_INITFLAGS flags, int *outputrate, int outputchannels, FMOD_SOUND_FORMAT *outputformat, int dspbufferlength, int dspnumbuffers, const void *hwnd)
{
    OutputMP3 *mp3;
	BE_CONFIG	beConfig = {0,};
	BE_ERR		err = 0;
    char        filename[256];

    if (outputchannels > 2)
    {
        return FMOD_ERR_TOOMANYCHANNELS;
    }

    mp3 = output->userdata = calloc(sizeof(OutputMP3), 1);
    if (!mp3)
    {
        return FMOD_ERR_MEMORY;
    }
    
    *outputformat = FMOD_SOUND_FORMAT_PCM16;

	mp3->mDLL = LoadLibrary("lame_enc.dll");
	if (!mp3->mDLL)
	{
		return FMOD_ERR_PLUGIN_RESOURCE;
	}

	/*
        Get Interface functions from the DLL
    */
	mp3->beInitStream	    = (BEINITSTREAM)     GetProcAddress(mp3->mDLL, TEXT_BEINITSTREAM);
	mp3->beEncodeChunk	    = (BEENCODECHUNK)    GetProcAddress(mp3->mDLL, TEXT_BEENCODECHUNK);
	mp3->beDeinitStream	    = (BEDEINITSTREAM)   GetProcAddress(mp3->mDLL, TEXT_BEDEINITSTREAM);
	mp3->beCloseStream	    = (BECLOSESTREAM)    GetProcAddress(mp3->mDLL, TEXT_BECLOSESTREAM);
	mp3->beVersion		    = (BEVERSION)        GetProcAddress(mp3->mDLL, TEXT_BEVERSION);
	mp3->beWriteVBRHeader   = (BEWRITEVBRHEADER) GetProcAddress(mp3->mDLL, TEXT_BEWRITEVBRHEADER);

	// Check if all interfaces are present
	if(!mp3->beInitStream || !mp3->beEncodeChunk || !mp3->beDeinitStream || !mp3->beCloseStream || !mp3->beVersion || !mp3->beWriteVBRHeader)
	{
		return FMOD_ERR_PLUGIN;
	}

	// Get the version number
	mp3->beVersion( &mp3->Version );

    memset(&beConfig,0,sizeof(beConfig));					// clear all fields

	// use the LAME config structure
	beConfig.dwConfig = BE_CONFIG_LAME;

	// this are the default settings for testcase.wav

    // FMOD NOTE : The 'hwnd' param could be used to pass in info about the bitrate and encoding settings.

	beConfig.format.LHV1.dwStructVersion	= 1;
	beConfig.format.LHV1.dwStructSize		= sizeof(beConfig);		
	beConfig.format.LHV1.dwSampleRate		= *outputrate;           // INPUT FREQUENCY
	beConfig.format.LHV1.dwReSampleRate		= 0;					// DON"T RESAMPLE
	beConfig.format.LHV1.nMode				= outputchannels == 1 ? BE_MP3_MODE_MONO : BE_MP3_MODE_JSTEREO;	// OUTPUT IN STREO
	beConfig.format.LHV1.dwBitrate			= 128;					// MINIMUM BIT RATE
	beConfig.format.LHV1.nPreset			= LQP_R3MIX;		    // QUALITY PRESET SETTING
	beConfig.format.LHV1.dwMpegVersion		= MPEG1;				// MPEG VERSION (I or II)
	beConfig.format.LHV1.dwPsyModel			= 0;					// USE DEFAULT PSYCHOACOUSTIC MODEL 
	beConfig.format.LHV1.dwEmphasis			= 0;					// NO EMPHASIS TURNED ON
	beConfig.format.LHV1.bOriginal			= TRUE;					// SET ORIGINAL FLAG
	beConfig.format.LHV1.bWriteVBRHeader	= TRUE;					// Write INFO tag

//	beConfig.format.LHV1.dwMaxBitrate		= 320;					// MAXIMUM BIT RATE
//	beConfig.format.LHV1.bCRC				= TRUE;					// INSERT CRC
//	beConfig.format.LHV1.bCopyright			= TRUE;					// SET COPYRIGHT FLAG	
//	beConfig.format.LHV1.bPrivate			= TRUE;					// SET PRIVATE FLAG
//	beConfig.format.LHV1.bWriteVBRHeader	= TRUE;					// YES, WRITE THE XING VBR HEADER
//	beConfig.format.LHV1.bEnableVBR			= TRUE;					// USE VBR
//	beConfig.format.LHV1.nVBRQuality		= 5;					// SET VBR QUALITY
	beConfig.format.LHV1.bNoRes				= TRUE;					// No Bit resorvoir

// Preset Test
//	beConfig.format.LHV1.nPreset			= LQP_PHONE;

	// Init the MP3 Stream
	err = mp3->beInitStream(&beConfig, &mp3->dwSamples, &mp3->dwMP3Buffer, &mp3->hbeStream);

	// Check result
	if(err != BE_ERR_SUCCESSFUL)
	{
		return FMOD_ERR_PLUGIN;
	}

	// Allocate MP3 buffer
	mp3->pMP3Buffer = malloc(mp3->dwMP3Buffer);
	if(!mp3->pMP3Buffer)
	{
        return FMOD_ERR_MEMORY;
	}

	// Allocate WAV buffer
	mp3->pWAVBuffer = malloc(mp3->dwSamples * sizeof(SHORT));
	if (!mp3->pWAVBuffer)
	{
        return FMOD_ERR_MEMORY;
	}

    if (!hwnd)
    {
        strncpy(filename, "fmodoutput.mp3", 256);
    }
    else
    {
        strncpy(filename, (char *)hwnd, 256);
    }

    mp3->mFP = fopen(filename, "wb");
    if (!mp3->mFP)
    {
        return FMOD_ERR_FILE_NOTFOUND;
    }

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]
    FMOD_OK

	[REMARKS]

    [PLATFORMS]
    Win32, Win64, Linux, Macintosh, XBox, PlayStation 2, GameCube

	[SEE_ALSO]
]
*/
FMOD_RESULT F_CALLBACK OutputMP3_CloseCallback(FMOD_OUTPUT *output)
{
    OutputMP3 *mp3 = output->userdata;
	DWORD dwRead=0;
	DWORD dwWrite=0;
	DWORD dwDone=0;
	DWORD dwFileSize=0;   
    BE_ERR err;

	// Deinit the stream
	err = mp3->beDeinitStream(mp3->hbeStream, mp3->pMP3Buffer, &dwWrite);

	// Check result
	if(err != BE_ERR_SUCCESSFUL)
	{
		mp3->beCloseStream(mp3->hbeStream);

		return FMOD_ERR_PLUGIN;
	}

	// Are there any bytes returned from the DeInit call?
	// If so, write them to disk
	if( dwWrite )
	{
		if( fwrite( mp3->pMP3Buffer, 1, dwWrite, mp3->mFP ) != dwWrite )
		{
			return FMOD_ERR_FILE_BAD;
		}
	}

	// close the MP3 Stream
	mp3->beCloseStream( mp3->hbeStream );

    if (mp3->pWAVBuffer)
    {
        free(mp3->pWAVBuffer);
        mp3->pWAVBuffer = 0;
    }

    if (mp3->pMP3Buffer)
    {
        free(mp3->pMP3Buffer);
        mp3->pMP3Buffer = 0;
    }

    if (mp3->mFP)
    {
        fclose(mp3->mFP);
        mp3->mFP = 0;
    }

    if (mp3)
    {
        free(mp3);
        output->userdata = 0;
    }

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]
    FMOD_OK

	[REMARKS]

    [PLATFORMS]
    Win32, Win64, Linux, Macintosh, XBox, PlayStation 2, GameCube

	[SEE_ALSO]
]
*/
FMOD_RESULT F_CALLBACK OutputMP3_UpdateCallback(FMOD_OUTPUT *output)
{
    FMOD_RESULT result;
    OutputMP3 *mp3 = output->userdata;
	DWORD dwRead=0;
	DWORD dwWrite=0;
	DWORD dwDone=0;
	DWORD dwFileSize=0;   
    BE_ERR err;

    /*
        Update the mixer to the interleaved buffer.
    */
    result = output->readfrommixer(output, mp3->pWAVBuffer, mp3->dwSamples / sizeof(SHORT));
    if (result != FMOD_OK)
    {
        return FMOD_OK;
    }
          
    dwRead = mp3->dwSamples * sizeof(SHORT);

	{
		// Encode samples
		err = mp3->beEncodeChunk(mp3->hbeStream, dwRead / sizeof(SHORT), mp3->pWAVBuffer, mp3->pMP3Buffer, &dwWrite);

		// Check result
		if(err != BE_ERR_SUCCESSFUL)
		{
			mp3->beCloseStream(mp3->hbeStream);
			return FMOD_ERR_PLUGIN;
		}
		
		// write dwWrite bytes that are returned in tehe pMP3Buffer to disk
		if (fwrite(mp3->pMP3Buffer, 1, dwWrite, mp3->mFP) != dwWrite)
		{
			return FMOD_ERR_FILE_BAD;
		}

		dwDone += dwRead*sizeof(SHORT);
	}
   
    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]
    FMOD_OK

	[REMARKS]

    [PLATFORMS]
    Win32, Win64, Linux, Macintosh, XBox, PlayStation 2, GameCube

	[SEE_ALSO]
]
*/
FMOD_RESULT F_CALLBACK OutputMP3_GetHandleCallback(FMOD_OUTPUT *output, void **handle)
{
    OutputMP3 *mp3 = output->userdata;

    *handle = mp3->mFP;

    return FMOD_OK;
}


