#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent() :
keyboardComponent(keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard),
startTime(juce::Time::getMillisecondCounterHiRes() * 0.001)
{
    setOpaque(true);
    
    addAndMakeVisible (midiInputListLabel);
    midiInputListLabel.setText ("MIDI Input:", juce::dontSendNotification);
    midiInputListLabel.attachToComponent (&midiInputList, true);
    
    addAndMakeVisible(midiInputList);
    midiInputList.setTextWhenNoChoicesAvailable("No MIDI Inputs Availible");
    auto midiInputs = juce::MidiInput::getAvailableDevices(); //checks the computer for connected devices
    
    juce::StringArray midiInputNames;
    for(auto i : midiInputs) //iterates through availible devices and adds their names to the list
        midiInputNames.add(i.name);
    midiInputList.addItemList(midiInputNames, 1);
    midiInputList.onChange = [this]
    {
        setMidiInput (midiInputList.getSelectedItemIndex());
    };
    for(auto i : midiInputs)
    {
        if(deviceMgr.isMidiInputDeviceEnabled(i.identifier)){
            setMidiInput(midiInputs.indexOf (i));
            break;
        }
    }
    if(midiInputList.getSelectedId() == 0) //default if no device is selected (hence the break statement in the selector)
        setMidiInput(0);
    
    addAndMakeVisible (keyboardComponent);
    keyboardState.addListener (this);
    
    addAndMakeVisible (midiMessagesBox);
    midiMessagesBox.setMultiLine (true);
    midiMessagesBox.setReturnKeyStartsNewLine (true);
    midiMessagesBox.setReadOnly (true);
    midiMessagesBox.setScrollbarsShown (true);
    midiMessagesBox.setCaretVisible (false);
    midiMessagesBox.setPopupMenuEnabled (true);
    midiMessagesBox.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0x32ffffff));
    midiMessagesBox.setColour (juce::TextEditor::outlineColourId, juce::Colour (0x1c000000));
    midiMessagesBox.setColour (juce::TextEditor::shadowColourId, juce::Colour (0x16000000));

    setSize (600, 400);
    setAudioChannels(2, 2);
}

MainComponent::~MainComponent()
{
    keyboardState.removeListener (this);
    deviceManager.removeMidiInputDeviceCallback (juce::MidiInput::getAvailableDevices()[midiInputList.getSelectedItemIndex()].identifier, this);
    shutdownAudio();
}

//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    // This function will be called when the audio device is started, or when
    // its settings (i.e. sample rate, block size, etc) are changed.

    // You can use this function to initialise any resources you might need,
    // but be careful - it will be called on the audio thread, not the GUI thread.

    // For more details, see the help for AudioProcessor::prepareToPlay()
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    // Your audio-processing code goes here!

    // For more details, see the help for AudioProcessor::getNextAudioBlock()

    // Right now we are not producing any data, in which case we need to clear the buffer
    // (to prevent the output of random noise)
    bufferToFill.clearActiveBufferRegion();
}

void MainComponent::releaseResources()
{
    // This will be called when the audio device stops, or when it is being
    // restarted due to a setting change.

    // For more details, see the help for AudioProcessor::releaseResources()
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    // You can add your drawing code here!
}

void MainComponent::resized()
{
    auto area = getLocalBounds();
    midiInputList    .setBounds (area.removeFromTop (36).removeFromRight (getWidth() - 150).reduced (8));
    keyboardComponent.setBounds (area.removeFromTop (80).reduced(8));
    midiMessagesBox  .setBounds (area.reduced (8));
}

void MainComponent::setMidiInput(int inputIndex)
{
    auto list = juce::MidiInput::getAvailableDevices();
    
    deviceMgr.removeMidiInputDeviceCallback(list[lastInputIndex].identifier, this);
    
    auto newInput = list[inputIndex];
    if(!deviceMgr.isMidiInputDeviceEnabled(newInput.identifier))
    {
        deviceMgr.setMidiInputDeviceEnabled(newInput.identifier, true);
    }
    deviceMgr.addMidiInputDeviceCallback(newInput.identifier, this);
    midiInputList.setSelectedId(inputIndex + 1, juce::dontSendNotification);
    
    lastInputIndex = inputIndex;
}

void MainComponent::handleIncomingMidiMessage(juce::MidiInput *source, const juce::MidiMessage &message)
{
    const juce::ScopedValueSetter<bool> scopedInputFlag(isAddingFromMidi, true);
    keyboardState.processNextMidiEvent(message);
    postMessageToList(message, source->getName());
}

void MainComponent::handleNoteOn(juce::MidiKeyboardState *, int midiChannel, int midiNoteNumber, float velocity)
{
    if(!isAddingFromMidi)
    {
        auto m = juce::MidiMessage::noteOn(midiChannel, midiNoteNumber, velocity);
        m.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
        postMessageToList(m, "On-screen Keyboard");
    }
}

void MainComponent::handleNoteOff(juce::MidiKeyboardState *, int midiChannel, int midiNoteNumber, float /*velocity*/)
{
    if(!isAddingFromMidi)
    {
        auto m = juce::MidiMessage::noteOff(midiChannel, midiNoteNumber);
        m.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
        postMessageToList(m, "On-screen Keyboard");
    }
}

void MainComponent::postMessageToList(const juce::MidiMessage &message, const juce::String &source)
{
    (new IncomingMessageCallback (this, message, source))->post();
}

void MainComponent::addMessageToList(const juce::MidiMessage &message, const juce::String &source)
{
    auto time = message.getTimeStamp() - startTime;
    
    auto hours   = ((int) (time / 3600.0)) % 24;
    auto minutes = ((int) (time / 60.0)) % 60;
    auto seconds = ((int) time) % 60;
    auto millis  = ((int) (time * 1000.0)) % 1000;
    
    auto timecode = juce::String::formatted ("%02d:%02d:%02d.%03d",
                                                 hours,
                                                 minutes,
                                                 seconds,
                                                 millis);
 
        auto description = getMidiMessageDescription(message);
 
        juce::String midiMessageString (timecode + "  -  " + description + " (" + source + ")"); // [7]
        logMessage (midiMessageString);
}
