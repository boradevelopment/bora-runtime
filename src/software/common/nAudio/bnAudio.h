#pragma once
#include <functional>
#include "AudioAbstractions.h"
#include "Utils.h"

struct IAudioCommand
{
    virtual ~IAudioCommand() = default;
    virtual void Execute(IAudioDevice* graphics) = 0;
};

using AudioFunction = std::function<void(IAudioDevice*)>;
using AudioCommandVector = std::vector<std::unique_ptr<IAudioCommand>>;
using AudioCommandVectorPlain = std::vector<IAudioCommand*>;

class bnWindow;

class bnAudio
{
public:
    bnAudio(bnWindow* Window);
    ~bnAudio();
protected:
    bnWindow* window;
    AudioCommandVector commands;
};

