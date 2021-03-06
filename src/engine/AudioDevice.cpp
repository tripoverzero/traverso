/*
Copyright (C) 2005-2006 Remon Sijrier

This file is part of Traverso

Traverso is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA.

$Id: AudioDevice.cpp,v 1.56 2009/03/04 20:54:00 r_sijrier Exp $
*/

#include "AudioDevice.h"
#include "AudioDeviceThread.h"

#if defined (ALSA_SUPPORT)
#include "AlsaDriver.h"
#endif

#if defined (JACK_SUPPORT)
RELAYTOOL_JACK
#include "JackDriver.h"
#endif

#if defined (PORTAUDIO_SUPPORT)
#include "PADriver.h"
#endif

#if defined (PULSEAUDIO_SUPPORT)
#include "PulseAudioDriver.h"
#endif

#if defined (COREAUDIO_SUPPORT)
#include "CoreAudioDriver.h"
#endif


#include "Driver.h"
#include "Client.h"
#include "AudioChannel.h"
#include "AudioBus.h"
#include "Tsar.h"
//#include <sys/mman.h>

// Always put me below _all_ includes, this is needed
// in case we run with memory leak detection enabled!
#include "Debugger.h"

/*! 	\class AudioDevice
	\brief An Interface to the 'real' audio device, and the hearth of the libtraversoaudiobackend
 
	AudioDevice is accessed by the audiodevice() function. You need to first initialize the 'device' by 
	calling AudioDevice::set_parameters(int rate, nframes_t bufferSize, QString driverType);
	This will initialize the real audiodevice in case of the Alsa driver, or connect to the jack deamon. 
	In the latter case, the rate and bufferSize don't do anything, since they are provided by the jack itself
	
	This class and/or related classes depend on RingBuffer, Tsar and FastDelegate which are found in libtraversocore.
	The signal/slot feature as supplied by Qt is also used, which makes the Qt dependency a bit deeper, though
	it shouldn't be to hard to get rid of it if you like to use the libtraversoaudiobackend in an application not 
	using Qt, or if you don't want a dependency to Qt.
	
	Using the audiobackend in an application is as simple as:
	
	\code
	#include <AudioDevice.h>
	
	main()
	{
		myApp = new MyApp();
		myApp->execute();
		return;
	}
		
	MyApp::MyApp() 
		: QApplication
	{	
		setup_audiobackend();
		connect_to_audiodevice();
	}
		
	
	void MyApp::setup_audiobackend()
	{
		int rate = 44100;
		int bufSize = 1024;
		QString driver = "ALSA";
		audiodevice().set_parameters(rate, bufSize, driver);
	}
	\endcode
	
	
	The AudioDevice instance now has set up it's own audio thread, or uses the one created by jack.
	This thread will continuously run, and process the callback functions of the registered Client's
	
	Connecting your application to the audiodevice is done by creating an instance of Client, and 
	setting the right callback function. The Client is added to the audiodevice in a thread save way, 
	without using any locking mechanisms.
	
	\code
	void MyApp::connect_to_audiodevice()
	{
		m_client = new Client("MyApplication");
		m_client->set_process_callback( MakeDelegate(this, &MyApp::process) );
		audiodevice().add_client(m_client);
	}
	\endcode
	
	Finally, we want to do some processing in the process callback, e.g.
	
	\code
	int MyApp::process(nframes_t nframes)
	{
		AudioBus* captureBus = audiodevice().get_capture_bus("Capture 1");
		AudioBus* playbackBus = audiodevice().get_playback_bus("Playback 1");
		
		// Just copy the captured audio to the playback buses.
		for (int i=0; i<captureBuses->get_channel_count(); ++i) {
			memcpy(captureBus->get_channel(i)->get_buffer(nframes), playbackBus->get_channel(i)->get_buffer(nframes), nframes); 
		}
		
		return 1;
	}
	\endcode
	
	*/

/**
 * A global function, used to get the AudioDevice instance. Due the nature of singletons, the 
   AudioDevice intance will be created automatically!
 * @return The AudioDevice instance, it will be automatically created on first call
 */
AudioDevice& audiodevice()
{ 
	static AudioDevice device;
	return device;
}

AudioDevice::AudioDevice()
{
	m_runAudioThread = false;
	driver = 0;
	audioThread = 0;
	m_bufferSize = 1024;
	m_xrunCount = 0;
	m_cpuTime = new RingBufferNPT<trav_time_t>(4096);

	m_driverType = tr("No Driver Loaded");

#if defined (JACK_SUPPORT)
	if (libjack_is_present) {
		availableDrivers << "Jack";
	}
#endif

#if defined (ALSA_SUPPORT)
	availableDrivers << "ALSA";
#endif
	
#if defined (PORTAUDIO_SUPPORT)
	availableDrivers << "PortAudio";
#endif

#if defined (PULSEAUDIO_SUPPORT)
	availableDrivers << "PulseAudio";
#endif

#if defined (COREAUDIO_SUPPORT)
	availableDrivers << "CoreAudio";
#endif

	
	availableDrivers << "Null Driver";
	
	// tsar is a singleton, so initialization is done on first tsar() call
	// Tsar makes use of a QTimer to cleanup the processed events.
	// The QTimer _has_ to be started from the GUI thread, so we 'initialize'
	// tsar here _before_ the AudioDevice is setup, since tsar is being called
	// from here by for example the jack driver which could initialize tsar 
	// from within the _jack client thread_ which makes the whole thing _fail_
	tsar();
	
	connect(this, SIGNAL(xrunStormDetected()), this, SLOT(switch_to_null_driver()));
	connect(&m_xrunResetTimer, SIGNAL(timeout()), this, SLOT(reset_xrun_counter()));
	
	m_xrunResetTimer.start(30000);
}

AudioDevice::~AudioDevice()
{
	PENTERDES;

	shutdown();
	
	if (audioThread) {
		delete audioThread;
	}
	
	delete m_cpuTime;

	free_memory();
}

void AudioDevice::free_memory()
{
	foreach(AudioBus* bus, captureBuses) {
		delete bus;
	}

	foreach(AudioBus* bus, playbackBuses) {
		delete bus;
	}

	captureChannels.clear();
	playbackChannels.clear();
	captureBuses.clear();
	playbackBuses.clear();
}

/**
 * 
 * Not yet implemented 
 */
void AudioDevice::show_descriptors( )
{
	// Needs to be implemented
}

void AudioDevice::set_buffer_size( nframes_t size )
{
	Q_ASSERT(size > 0);
	m_bufferSize = size;
}

void AudioDevice::set_sample_rate( nframes_t rate )
{
	m_rate = rate;
}

void AudioDevice::set_bit_depth( uint depth )
{
	m_bitdepth = depth;
}

int AudioDevice::run_cycle( nframes_t nframes, float delayed_usecs )
{
	nframes_t left;

	if (nframes != m_bufferSize) {
		printf ("late driver wakeup: nframes to process = %ld\n", (long)nframes);
	}


	/* run as many cycles as it takes to consume nframes (Should be 1 cycle!!)*/
	for (left = nframes; left >= m_bufferSize; left -= m_bufferSize) {
		if (run_one_cycle (m_bufferSize, delayed_usecs) < 0) {
			qCritical ("cycle execution failure, exiting");
			return -1;
		}
	}

	post_process();

	return 1;
}

int AudioDevice::run_one_cycle( nframes_t nframes, float  )
{

	if (driver->read(nframes) < 0) {
		qDebug("driver read failed!");
		return -1;
	}

	apill_foreach(Client* client, Client, m_clients) {
		client->process(nframes);
	}
/*	for (int i=0; i<m_clients.size(); ++i) {
		if (m_clients.at(i)->process(nframes) < 0) {
			// ?
		}
	}*/
	
	if (driver->write(nframes) < 0) {
		qDebug("driver write failed!");
		return -1;
	}


	return 0;
}

void AudioDevice::delay( float  )
{
}

/**
 * 
 * @return The amount of Capture Buses, 0 if no Capture Buses are available 
 */
uint AudioDevice::capture_buses_count( ) const
{
	return captureChannels.size();
}

/**
 * 
 * @return The amount of PlayBack Buses, 0 if no PlayBack Buses are available 
 */
uint AudioDevice::playback_buses_count( ) const
{
	return playbackChannels.size();
}

/**
 * This function is used to initialize the AudioDevice's audioThread with the supplied
 * rate, bufferSize and driver type. In case the AudioDevice allready was configured,
 * it will stop the AudioDeviceThread and emits the stopped() signal,
 * re-inits the AlsaDriver with the new paramaters, when succesfull emits the driverParamsChanged() signal,
 * restarts the AudioDeviceThread and emits the started() signal
 * 
 * @param rate The new sample rate, only used for the AlsaDriver
 * @param bufferSize The period buffer size, only used for the AlsaDriver
 * @param driverType The Driver Type, can be ALSA, Jack or the Null Driver
 */
void AudioDevice::set_parameters( int rate, 
				nframes_t bufferSize, 
				const QString& driverType,
				bool capture,
				bool playback,
				const QString& cardDevice,
				const QString& ditherShape)
{
	PENTER;

	m_rate = rate;
	m_bufferSize = bufferSize;
	m_xrunCount = 0;
	m_ditherShape = ditherShape;

	shutdown();

	if (create_driver(driverType, capture, playback, cardDevice) < 0) {
		set_parameters(rate, bufferSize, "Null Driver");
		return;
	}
	
	driver->attach();
	
	setup_buses();

	emit driverParamsChanged();

	m_runAudioThread = 1;
	
	if ((driverType == "ALSA") || (driverType == "Null Driver")) {
		
		printf("Starting AudioDeviceThread..... ");
		
		
		if (!audioThread) {
			audioThread = new AudioDeviceThread(this);
		}

		// m_cycleStartTime/EndTime are set before/after the first cycle.
		// to avoid a "100%" cpu usage value during audioThread startup, set the
		// m_cycleStartTime here!
		m_cycleStartTime = get_microseconds();

		// When the audiothread fails for some reason we catch it in audiothread_finished()
		// by connecting the finished signal of the audio thread!
		connect(audioThread, SIGNAL(finished()), this, SLOT(audiothread_finished()));
		
		// Start the audio thread, the driver->start() will be called from there!!
		audioThread->start();
		
		// It appears this check is a little silly because it always returns true
		// this close after calling the QThread::start() function :-(
		if (audioThread->isRunning()) {
			printf("Running!\n");
		}
	}
	 
#if defined (JACK_SUPPORT)
	// This will activate the jack client
	if (libjack_is_present) {
		if (driverType == "Jack") {
			
			if (driver->start() == -1) {
				// jack driver failed to start, fallback to Null Driver:
				set_parameters(rate, bufferSize, "Null Driver");
				return;
			}
			
			connect(&jackShutDownChecker, SIGNAL(timeout()), this, SLOT(check_jack_shutdown()));
			jackShutDownChecker.start(500);
		}
	}
#endif
		
	if (driverType == "PortAudio"|| (driverType == "PulseAudio") || (driverType == "CoreAudio")) {
		if (driver->start() == -1) {
			// PortAudio driver failed to start, fallback to Null Driver:
			set_parameters(rate, bufferSize, "Null Driver");
			return;
		}
	}

	emit started();
}

int AudioDevice::create_driver(QString driverType, bool capture, bool playback, const QString& cardDevice)
{

#if defined (JACK_SUPPORT)
	if (libjack_is_present) {
		if (driverType == "Jack") {
			driver = new JackDriver(this, m_rate, m_bufferSize);
			if (driver->setup(capture, playback) < 0) {
				message(tr("Audiodevice: Failed to create the Jack Driver"), WARNING);
				delete driver;
				driver = 0;
				return -1;
			}
			m_driverType = driverType;
			return 1;
		}
	}
#endif

#if defined (ALSA_SUPPORT)
	if (driverType == "ALSA") {
		driver =  new AlsaDriver(this, m_rate, m_bufferSize);
		if (((AlsaDriver*)driver)->setup(capture,playback, cardDevice, m_ditherShape) < 0) {
			message(tr("Audiodevice: Failed to create the ALSA Driver"), WARNING);
			delete driver;
			driver = 0;
			return -1;
		}
		m_driverType = driverType;
		return 1;
	}
#endif

#if defined (PORTAUDIO_SUPPORT)
	if (driverType == "PortAudio") {
		driver = new PADriver(this, m_rate, m_bufferSize);
		if (driver->setup(capture, playback, cardDevice) < 0) {
			message(tr("Audiodevice: Failed to create the PortAudio Driver"), WARNING);
			delete driver;
			driver = 0;
			return -1;
		}
		m_driverType = driverType;
		return 1;
	}
#endif
	
#if defined (PULSEAUDIO_SUPPORT)
	if (driverType == "PulseAudio") {
		driver = new PulseAudioDriver(this, m_rate, m_bufferSize);
		if (driver->setup(capture, playback, cardDevice) < 0) {
			message(tr("Audiodevice: Failed to create the PulseAudio Driver"), WARNING);
			delete driver;
			driver = 0;
			return -1;
		}
		m_driverType = driverType;
		return 1;
	}
#endif


#if defined (COREAUDIO_SUPPORT)
	if (driverType == "CoreAudio") {
		driver = new CoreAudioDriver(this, m_rate, m_bufferSize);
		if (driver->setup(capture, playback, cardDevice) < 0) {
			message(tr("Audiodevice: Failed to create the CoreAudio Driver"), WARNING);
			delete driver;
			driver = 0;
			return -1;
		}
		m_driverType = driverType;
		return 1;
	}
#endif

	
	if (driverType == "Null Driver") {
		printf("Creating Null Driver...\n");
		driver = new Driver(this, m_rate, m_bufferSize);
		m_driverType = driverType;
		return 1;
	}

	return -1;
}


AudioChannel* AudioDevice::register_capture_channel(const QByteArray& chanName, const QString& audioType, int flags, uint , uint channel )
{
	AudioChannel* chan = new AudioChannel(chanName, audioType, flags, channel);
	captureChannels.insert(chanName, chan);
	return chan;
}

AudioChannel* AudioDevice::register_playback_channel(const QByteArray& chanName, const QString& audioType, int flags, uint , uint channel )
{
	AudioChannel* chan = new AudioChannel(chanName, audioType, flags, channel);
	playbackChannels.insert(chanName, chan);
	return chan;
}


/**
 * Stops the AudioDevice's AudioThread, free's any related memory.
 
 * Use this to properly shut down the AudioDevice on application exit,
 * or to explicitely release the real 'audiodevice'.
 
 * Use set_parameters() to reinitialize the audiodevice if you want to use it again.
 * 
 * @return 1 on succes, 0 on failure 
 */
int AudioDevice::shutdown( )
{
	PENTER;
	int r = 1;

	emit stopped();

	m_runAudioThread = 0;
	
	if (audioThread) {
		disconnect(audioThread, SIGNAL(finished()), this, SLOT(audiothread_finished()));
		
		// Wait until the audioThread has finished execution. One second
		// should do, if it's still running then, the thread must have gone wild or something....
		if (audioThread->isRunning()) {
			printf("Starting to shutdown AudioThread..\n");
			r = audioThread->wait(1000);
			printf("AudioDeviceThread finished, stopping driver\n");
		}
	}
	
	
	if (driver) {
		driver->stop();
		delete driver;
		driver = 0;
	}
	
	free_memory();

	return r;
}

/**
 * Get the names of all the Capture Buses availble, use the names to get a Bus instance 
 * via get_capture_bus()
 *
 * @return A QStringList with all the Capture Buses names which are available, 
 *		an empty list if no Buses are available.
 */
QStringList AudioDevice::get_capture_buses_names( ) const
{
	QStringList names;
	foreach(AudioBus* bus, captureBuses) {
		names.append(bus->get_name());
	}
	return names;
}

/**
 * Get the names of all the Playback Buses availble, use the names to get a Bus instance 
 * via get_playback_bus()
 *
 * @return A QStringList with all the PlayBack Buses names which are available, 
 *		an empty list if no Buses are available.
 */
QStringList AudioDevice::get_playback_buses_names( ) const
{
	QStringList names;
	foreach(AudioBus* bus, playbackBuses) {
		names.append(bus->get_name());
	}
	return names;
}

void AudioDevice::setup_buses( )
{
	int number = 1;
	QByteArray name;

	AudioChannel* channel;
	
	for (int i=1; i <= captureChannels.size();) {
		name = "Capture " + QByteArray::number(number++);
		AudioBus* bus = new AudioBus(name);
		channel = captureChannels.value("capture_"+QByteArray::number(i++), 0);
		if (channel) {
			bus->add_channel(channel);
		}
		channel = captureChannels.value("capture_"+QByteArray::number(i++), 0);
		if (channel) {
			bus->add_channel(channel);
		}
		captureBuses.insert(name, bus);
	}
// 	PWARN("Capture buses count is: %d", captureBuses.size());

	number = 1;

	for (int i=1; i <= playbackChannels.size();) {
		name = "Playback " + QByteArray::number(number++);
		AudioBus* bus = new AudioBus(name);
		channel = playbackChannels.value("playback_"+QByteArray::number(i++), 0);
		if (channel) {
			bus->add_channel(channel);
		}
		channel = playbackChannels.value("playback_"+QByteArray::number(i++), 0);
		if (channel) {
			bus->add_channel(channel);
		}
		playbackBuses.insert(name, bus);
	}
// 	PWARN("Playback buses count is: %d", playbackBuses.size());
}

/**
 * 
 * @return The real audiodevices sample rate
 */
uint AudioDevice::get_sample_rate( ) const
{
	return m_rate;
}

/**
 * 
 * @return The real bit depth, which is 32 bit float.... FIXME Need to get the real bitdepth as
 *		reported by the 'real audiodevice' 
 */
uint AudioDevice::get_bit_depth( ) const
{
	return m_bitdepth;
}

/**
 * 
 * @return The short description of the 'real audio device' 
 */
QString AudioDevice::get_device_name( ) const
{
	if (driver)
		return driver->get_device_name();
	return tr("No Device Configured");
}

/**
 * 
 * @return The long description of the 'real audio device' 
 */
QString AudioDevice::get_device_longname( ) const
{
	if (driver)
		return driver->get_device_longname();
	return tr("No Device Configured");
}

/**
 * 
 * @return A list of supported Drivers 
 */
QStringList AudioDevice::get_available_drivers( ) const
{
	return availableDrivers;
}

/**
 * 
 * @return The currently used Driver type 
 */
QString AudioDevice::get_driver_type( ) const
{
	return m_driverType;
}

/**
 * 
 * @return The cpu load, call this at least 1 time per second to keep data consistent 
 */
trav_time_t AudioDevice::get_cpu_time( )
{
#if defined (JACK_SUPPORT)
	if (libjack_is_present)
		if (driver && m_driverType == "Jack")
			return ((JackDriver*)driver)->get_cpu_load();
#endif
	
#if defined (PORTAUDIO_SUPPORT)
	if (driver && m_driverType == "PortAudio")
		return ((PADriver*)driver)->get_cpu_load();
#endif
	
	
	trav_time_t currentTime = get_microseconds();
	float totaltime = 0;
	trav_time_t value = 0;
	int read = m_cpuTime->read_space();

	while (read != 0) {
		read = m_cpuTime->read(&value, 1);
		totaltime += value;
	}

	audio_sample_t result = ( (totaltime  / (currentTime - m_lastCpuReadTime) ) * 100 );

	m_lastCpuReadTime = currentTime;

	return result;
}

void AudioDevice::post_process( )
{
	tsar().process_events();
}

void AudioDevice::private_add_client(Client* client)
{
	m_clients.prepend(client);
}

void AudioDevice::private_remove_client(Client* client)
{
	if (!m_clients.remove(client)) {
		printf("AudioDevice:: Client was not in clients list, failed to remove it!\n");
	}
}

/**
 * Adds the client into the audio processing chain in a Thread Save way

 * WARNING: This function assumes the Clients callback function is set to an existing objects function!
 */
void AudioDevice::add_client( Client * client )
{
	THREAD_SAVE_INVOKE(this, client, private_add_client(Client*));
}

/**
 * Removes the client into the audio processing chain in a Thread save way 
 *
 * The clientRemoved(Client* client); signal will be emited after succesfull removal
 * from within the GUI Thread!
 */
void AudioDevice::remove_client( Client * client )
{
	THREAD_SAVE_INVOKE_AND_EMIT_SIGNAL(this, client, private_remove_client(Client*), clientRemoved(Client*));
}

void AudioDevice::mili_sleep(int msec)
{
	audioThread->mili_sleep(msec);
}


void AudioDevice::audiothread_finished() 
{
	if (m_runAudioThread) {
		// AudioThread stopped, but we didn't do it ourselves
		// so something certainly did go wrong when starting the beast
		// Start the Null Driver to avoid problems with Tsar
		PERROR("Alsa/Jack AudioThread stopped, but we didn't ask for it! Something apparently did go wrong :-(");
		set_parameters(44100, m_bufferSize, "Null Driver");
	}
}

void AudioDevice::xrun( )
{
	RT_THREAD_EMIT(this, NULL, bufferUnderRun());
	
	m_xrunCount++;
	if (m_xrunCount > 30) {
		RT_THREAD_EMIT(this, NULL, xrunStormDetected());
	}
}

void AudioDevice::check_jack_shutdown()
{
#if defined (JACK_SUPPORT)
	if (libjack_is_present) {
		JackDriver* jackdriver = qobject_cast<JackDriver*>(driver);
		if (jackdriver) {
			if ( ! jackdriver->is_jack_running()) {
				jackShutDownChecker.stop();
				printf("jack shutdown detected\n");
				message(tr("The Jack server has been shutdown!"), CRITICAL);
				delete driver;
				driver = 0;
				set_parameters(44100, m_bufferSize, "Null Driver");
			}
		}
	}
#endif
}


void AudioDevice::switch_to_null_driver()
{
	message(tr("AudioDevice:: Buffer underrun 'Storm' detected, switching to Null Driver"), CRITICAL);
	message(tr("AudioDevice:: For trouble shooting this problem, please see Chapter 11 from the user manual!"), INFO);
	set_parameters(44100, m_bufferSize, "Null Driver");
}

int AudioDevice::transport_control(transport_state_t state)
{
#if defined (JACK_SUPPORT)
	if (!slaved_jack_driver()) {
		return true;
	}
#endif	

	int result = 0;
	
	apill_foreach(Client* client, Client, m_clients) {
		result = client->transport_control(state);
	}
	
	return result;
}

void AudioDevice::transport_start(Client * client)
{
#if defined (JACK_SUPPORT)
	JackDriver* jackdriver = slaved_jack_driver();
	if (jackdriver) {
		PMESG("using jack_transport_start");
		jack_transport_start(jackdriver->get_client());
		return;
	}
#endif
	
	transport_state_t state;
	state.tranport = TransportRolling;
	state.isSlave = false;
	state.realtime = false;
	state.location = TimeRef(); // get from client!!
	
	client->transport_control(state);
}

void AudioDevice::transport_stop(Client * client)
{
#if defined (JACK_SUPPORT)
	JackDriver* jackdriver = slaved_jack_driver();
	if (jackdriver) {
		PMESG("using jack_transport_stop");
		jack_transport_stop(jackdriver->get_client());
		return;
	}
#endif
	
	transport_state_t state;
	state.tranport = TransportStopped;
	state.isSlave = false;
	state.realtime = false;
	state.location = TimeRef(); // get from client!!
	
	client->transport_control(state);
}

// return 0 if valid request, non-zero otherwise.
int AudioDevice::transport_seek_to(Client* client, TimeRef location)
{
#if defined (JACK_SUPPORT)
	JackDriver* jackdriver = slaved_jack_driver();
	if (jackdriver) {
		PMESG("using jack_transport_locate");
		nframes_t frames = location.to_frame(get_sample_rate());
		return jack_transport_locate(jackdriver->get_client(), frames);
	}
#endif
	
	transport_state_t state;
	state.tranport = TransportStarting;
	state.isSlave = false;
	state.realtime = false; 
	state.location = location;
	
	client->transport_control(state);
	
	return 0;
}

#if defined (JACK_SUPPORT)
JackDriver* AudioDevice::slaved_jack_driver()
{
	if (libjack_is_present) {
		JackDriver* jackdriver = qobject_cast<JackDriver*>(driver);
		if (jackdriver && jackdriver->is_slave()) {
			return jackdriver;
		}
	}
	
	return 0;
}
#endif

TimeRef AudioDevice::get_buffer_latency()
{
	return TimeRef(m_bufferSize, m_rate);
}

void AudioDevice::set_driver_properties(QHash< QString, QVariant > & properties)
{
	m_driverProperties = properties;
#if defined (JACK_SUPPORT)
	if (libjack_is_present) {
		JackDriver* jackdriver = qobject_cast<JackDriver*>(driver);
		if (jackdriver) {
			jackdriver->update_config();
		}
	}
#endif
}

QVariant AudioDevice::get_driver_property(const QString& property, QVariant defaultValue)
{
	return m_driverProperties.value(property, defaultValue);
}

