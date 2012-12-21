//#ifdef ALSA // @todo : uncomment

#include <glib.h>
#include "AriaCore.h"
#include "Midi/Players/Alsa/AlsaPort.h"
#include <iostream>
#include <wx/wx.h>
#include <wx/filename.h>
#include "IO/IOUtils.h"
#include "Dialogs/WaitWindow.h"
#include "PreferencesData.h"

static const wxString SOFT_SYNTH_COMMAND = wxT("fluidsynth");
static const wxString SOFT_SYNTH_NAME = wxT("FluidSynth");
static const int SOFT_SYNTH_BASIC_TIMER = 300;
static const int SOFT_SYNTH_TIMER = 1000; // 1s
static const int SOFT_SYNTH_SLICE = 50000000; // in bytes - 50 Mo


namespace AriaMaestosa
{

MidiDevice::MidiDevice(MidiContext* context, int client_arg, int port_arg, const char* name_arg) :
	name(name_arg, wxConvUTF8)
{
    client = client_arg;
    port = port_arg;
    MidiDevice::midiContext = context;
}

bool MidiDevice::open()
{
    address.client = client;
    address.port = port;

    snd_seq_port_subscribe_alloca(&midiContext->subs);

    snd_seq_port_subscribe_set_sender(midiContext->subs, &midiContext->address);
    snd_seq_port_subscribe_set_dest(midiContext->subs, &address);

    const int success = snd_seq_subscribe_port(midiContext->sequencer, midiContext->subs);
    if(success != 0)
    {
        wxMessageBox( _("Failed to open ALSA port.") );
        return false;
    }
    return true;
}

wxString MidiDevice::getFullName()
{
    return wxString::Format(wxT("%i:%i "), client, port) + name;
}


void MidiDevice::close()
{
    if (midiContext != NULL)
    {
        snd_seq_unsubscribe_port(midiContext->sequencer, midiContext->subs);
        snd_seq_drop_output(midiContext->sequencer);
        snd_seq_free_queue(midiContext->sequencer, midiContext->queue);
        snd_seq_close(midiContext->sequencer);
    }
}


#if 0
#pragma mark -
#endif

MidiContext::MidiContext()
{
    queue = -1;
    device = NULL;
}

MidiContext::~MidiContext()
{
    g_array_free(destlist, TRUE);
}


void MidiContext::closeDevice()
{
    if (device != NULL)
    {
        device->close();
        device = NULL;
    }
}

bool MidiContext::openDevice(bool launchSoftSynth)
{
    if (launchSoftSynth)
    {
        wxString soundBank;
        wxString soundFontPath;
        bool soundFontExists;
        
        soundBank = PreferencesData::getInstance()->getValue(SETTING_ID_SOUNDBANK);
        soundFontPath = (soundBank == SYSTEM_BANK ? DEFAULT_SOUNDFONT_PATH : soundBank);
        soundFontExists = wxFileExists(soundFontPath);
        if (isExeRunning(SOFT_SYNTH_COMMAND))
        {
            std::cout << SOFT_SYNTH_NAME.mb_str() << " appears to be already running. " << std::endl;
            
            if (soundFontExists)
            {
                wxString grepCommand = wxT("ps x --cols 255 | grep ") + SOFT_SYNTH_COMMAND;
                
                // @todo : compare current soundfont path (in params) with path in prefs
                
                wxString command;
                char output[256];
                FILE* commandOutput;
                wxString outputString;

                commandOutput = popen(grepCommand.mb_str(), "r");
                memset(output,0,256);
                if (commandOutput != NULL)
                {
                    fread(output, 1, 255, commandOutput);
                    outputString = wxString::FromUTF8(output);
                
                    // @todo extract data from fullOutput
                    // relaunch if soundfont has changed
                    
                    //wxString killCommand = wxT("killall ") + SOFT_SYNTH_COMMAND;
                    
                    pclose(commandOutput);
                }
            }
        }
        else
        {
            if (soundFontExists)
            {
                wxULongLong fileSize;
            
                fileSize = wxFileName::GetSize(soundFontPath);
                if (fileSize!=wxInvalidSize)
                {
                    int filePartCount;
                    
                    WaitWindow::show((wxWindow*)getMainFrame(), _("Loading ") + SOFT_SYNTH_NAME, true);
                    
                    std::cout << "Launching " << SOFT_SYNTH_NAME.mb_str() << " ALSA deamon" << std::endl;
                    runSoftSynth(soundFontPath);
                    
                    wxMilliSleep(SOFT_SYNTH_BASIC_TIMER);
                    
                    filePartCount = (int)fileSize.ToULong() / SOFT_SYNTH_SLICE + 1;
                    
                    for (int i=0 ; i<filePartCount ; i++)
                    {
                        WaitWindow::setProgress(i * 100 / filePartCount);
                        wxMilliSleep(SOFT_SYNTH_TIMER); 
                    }
                
                    WaitWindow::hide();
                }
                else
                {
                    std::cout << "Soundfont could not be read" << std::endl;
                }
            }
        }
    }
    
    findDevices();
    
    // -------------- ask user to choose a midi port -----------------
    const int deviceAmount = getDeviceAmount();

    if (deviceAmount < 1)
    {
        wxMessageBox( _("No midi port found, no sound will be available.") );
        return false;
    }

    bool success = false;
    
        
    MidiDevice* d = NULL;
    
    wxString port = PreferencesData::getInstance()->getValue(SETTING_ID_MIDI_OUTPUT);
    if (port == DEFAULT_PORT)
    {
        int deviceCount = devices.size();
        if (deviceCount==1)
        {
            // default is to take the first and only device (no choice)
            setDevice(&d, 0);
        }
        else if (deviceCount>1)
        {
            wxString lowerCaseName;
            
            // More than one device : select fist MIDI port which is not "midi through" port
            bool portSelected = false;
            for (int n=0 ; n<(int)devices.size() && !portSelected ; n++)
            {
                lowerCaseName=devices[n].name.Lower();
                if (lowerCaseName.Find(wxT("through"))==wxNOT_FOUND)
                {
                    // This is not a Midi Through port
                    portSelected = true;
                    setDevice(&d, n);
                }
            }
            
            // Fail-over
            if (!portSelected)
            {
                setDevice(&d, 0);
            }
        }
    }
    else
    {
        wxString a_str = port.BeforeFirst(wxT(':'));
        wxString b_str = port.BeforeFirst(wxT(' ')).AfterLast(wxT(':'));
        
        long a, b;
        if (!a_str.ToLong(&a) || !b_str.ToLong(&b))
        {
            wxMessageBox( wxT("Invalid midi output selected") );
            fprintf(stderr, "Invalid midi output selected  <%s:%s>\n", (const char*)a_str.utf8_str(),
                    (const char*)b_str.utf8_str());
            return false;
        }
        
        d = getDevice(a,b);
        
        // @todo : fail-over based upon "fluidsynth" string 
    }
    
    if (d == NULL)
    {
        success = false;
    }
    else
    {
        success = openDevice(d);
    }
    
    if (!success)
    {
        wxString errorMsg = _("The selected ALSA device (%s) could not be opened.");
        errorMsg.Replace(wxT("%s"), port);
        wxMessageBox( errorMsg );
    }
    
    return success;
}


MidiDevice* MidiContext::getDevice(int i)
{
    if(i<0 or i>(int)devices.size()) return NULL;
    return &devices[i];
}


MidiDevice* MidiContext::getDevice(int client, int port)
{
    for (int n=0; n<(int)devices.size(); n++)
    {
        if(devices[n].client == client and devices[n].port == port) return &devices[n];
    }
    return NULL;
}


void MidiContext::findDevices()
{
    devices.clearAndDeleteAll();
    snd_seq_client_info_t* clientInfo;

    if (snd_seq_open(&sequencer, DEFAULT_PORT.mb_str(), SND_SEQ_OPEN_OUTPUT, 0) < 0)
    {
        return;
    }

    snd_seq_client_info_alloca(&clientInfo);
    snd_seq_client_info_set_client(clientInfo, -1);

    // iterate through clients
    while (snd_seq_query_next_client(sequencer, clientInfo) >= 0)
    {
        snd_seq_port_info_t* pinfo;

        snd_seq_port_info_alloca(&pinfo);
        snd_seq_port_info_set_client(pinfo, snd_seq_client_info_get_client(clientInfo));
        snd_seq_port_info_set_port(pinfo, -1);

        // and now through ports
        while (snd_seq_query_next_port(sequencer, pinfo) >= 0)
        {
            unsigned int capability = snd_seq_port_info_get_capability(pinfo);

            if ((capability & SND_SEQ_PORT_CAP_SUBS_WRITE) == 0)
            {
                continue;
            }

            int client  = (snd_seq_port_info_get_addr(pinfo))->client;
            int port    = (snd_seq_port_info_get_addr(pinfo))->port;

            std::cout << client << ":" << port << " --> " << snd_seq_port_info_get_name(pinfo) << std::endl;
            devices.push_back( new MidiDevice(this, client, port, snd_seq_port_info_get_name(pinfo)) );

        }

    }

    if (snd_seq_set_client_name(sequencer, "Aria") < 0)
    {
        return;
    }

    //midiContext.address.port = snd_seq_create_simple_port(midiContext.sequencer, "Aria Port 0", SND_SEQ_PORT_CAP_SUBS_WRITE, SND_SEQ_PORT_TYPE_APPLICATION);
    address.port =  snd_seq_create_simple_port(sequencer, "Aria Port 0",
                     SND_SEQ_PORT_CAP_WRITE |
                     SND_SEQ_PORT_CAP_SUBS_WRITE |
                     SND_SEQ_PORT_CAP_READ,
                     SND_SEQ_PORT_TYPE_APPLICATION);
                     //SND_SEQ_PORT_TYPE_MIDI_GENERIC);

    address.client = snd_seq_client_id (sequencer);
    snd_seq_set_client_pool_output (sequencer, 1024);

    destlist = g_array_new(0, 0, sizeof(snd_seq_addr_t));
}


int MidiContext::getDeviceAmount()
{
    return devices.size();
}

// 'timerStarted' is not used by this class
// it's just a conveniant place to keep this info
bool MidiContext::isPlaying()
{
    return timerStarted;
}

void MidiContext::setPlaying(bool playing)
{
    timerStarted = playing;
}


bool MidiContext::openDevice(MidiDevice* device)
{
    MidiContext::device = device;
    if (device == NULL) return false;
    return device->open();
}


bool MidiContext::isExeRunning(const wxString& command)
{
     // start by checking if exe is already running
    FILE * commandOutput;
    char output[128];
    int amountRead = 1;
    std::string fullOutput;


    commandOutput = popen("ps -A", "r");
    if (commandOutput != NULL)
    {
        while (amountRead > 0)
        {
            amountRead = fread(output, 1, 127, commandOutput);
            if (amountRead <= 0)
            {
                break;
            }
            else
            {
                output[amountRead] = '\0';
                fullOutput += output;
                //std::cout << output << std::endl;
            }
        } // end while
        
        pclose(commandOutput);
    } // end if
    

    const bool isRunning = fullOutput.find(command.mb_str()) != std::string::npos;
    
    return isRunning;
}


/* obsoleted
void MidiContext::runTimidity()
{
    //-iA => Launch TiMidity++ as ALSA sequencer client
    //-Os => Output to ALSA
    wxString cmd(SOFT_SYNTH_COMMAND + wxT(" -iA -Os"));
    wxExecute(cmd);
    wxMilliSleep(500); // let the timidity deamon some time to start
}
*/


void MidiContext::runSoftSynth(const wxString& soundfontPath)
{
    wxString cmd(SOFT_SYNTH_COMMAND 
        + wxT(" -a alsa -l --server -i ") + soundfontPath);
    
    wxExecute(cmd, wxEXEC_ASYNC);
}


void MidiContext::setDevice(MidiDevice** d, int index)
{
    wxString name;
    
    *d = getDevice(index);
    PreferencesData::getInstance()->setValue(SETTING_ID_MIDI_OUTPUT, (*d)->getFullName());
}

}

//#endif // @todo : uncomment
