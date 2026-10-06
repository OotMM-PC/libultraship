#include "ship/audio/Audio.h"

#ifdef __APPLE__
#include "ship/audio/CoreAudioAudioPlayer.h"
#endif

#include "ship/Context.h"
#include "ship/controller/controldeck/ControlDeck.h"

namespace Ship {

Audio::~Audio() {
    SPDLOG_TRACE("destruct audio");
}

std::shared_ptr<AudioPlayer> Audio::CreateAudioPlayer(AudioBackend backend) {
    switch (backend) {
#ifdef _WIN32
        case AudioBackend::WASAPI:
            return std::make_shared<WasapiAudioPlayer>(this->mAudioSettings);
#endif
#ifdef __APPLE__
        case AudioBackend::COREAUDIO:
            return std::make_shared<CoreAudioAudioPlayer>(this->mAudioSettings);
#endif
        case AudioBackend::SDL:
            return std::make_shared<SDLAudioPlayer>(this->mAudioSettings);
        default:
            return std::make_shared<NullAudioPlayer>(this->mAudioSettings);
    }
}

void Audio::InitAudioPlayer() {
    mAudioPlayer = CreateAudioPlayer(GetCurrentAudioBackend());
    if (mAudioPlayer->Init()) {
        return;
    }

    for (const AudioBackend fallback : *mAvailableAudioBackends) {
        if (fallback == GetCurrentAudioBackend()) {
            continue;
        }
        mAudioPlayer = CreateAudioPlayer(fallback);
        if (mAudioPlayer->Init()) {
            SPDLOG_WARN("Audio backend {} failed to start; using {} for this session",
                        static_cast<int>(GetCurrentAudioBackend()), static_cast<int>(fallback));
            mAudioBackend = fallback;
            return;
        }
    }
}

void Audio::Init() {
    mAvailableAudioBackends = std::make_shared<std::vector<AudioBackend>>();
#ifdef _WIN32
    mAvailableAudioBackends->push_back(AudioBackend::WASAPI);
#endif
#ifdef __APPLE__
    mAvailableAudioBackends->push_back(AudioBackend::COREAUDIO);
#endif
    mAvailableAudioBackends->push_back(AudioBackend::SDL);
    mAvailableAudioBackends->push_back(AudioBackend::NUL);

#if !defined(NDEBUG) || defined(OOTMM_RUNTIME_TEST)
    if (const char* directory = std::getenv("OOTMM_RUNTIME_TEST_DIR"); directory != nullptr && directory[0] != '\0') {
        mAudioBackend = AudioBackend::NUL;
        return;
    }
#endif
    SetCurrentAudioBackend(Context::GetInstance()->GetConfig()->GetCurrentAudioBackend());
}

std::shared_ptr<AudioPlayer> Audio::GetAudioPlayer() {
    return mAudioPlayer;
}

AudioBackend Audio::GetCurrentAudioBackend() {
    return mAudioBackend;
}

void Audio::SetCurrentAudioBackend(AudioBackend backend) {
    mAudioBackend = backend;
    Context::GetInstance()->GetConfig()->SetCurrentAudioBackend(GetCurrentAudioBackend());
    Context::GetInstance()->GetConfig()->Save();

    InitAudioPlayer();
}

std::shared_ptr<std::vector<AudioBackend>> Audio::GetAvailableAudioBackends() {
    return mAvailableAudioBackends;
}

void Audio::SetAudioChannels(AudioChannelsSetting channels) {
    if (mAudioSettings.ChannelSetting != channels) {
        mAudioSettings.ChannelSetting = channels;
        // Reinitialize the existing audio player with the new channel configuration
        if (mAudioPlayer) {
            mAudioPlayer->SetAudioChannels(channels);
        }
    }
}

AudioChannelsSetting Audio::GetAudioChannels() const {
    return mAudioSettings.ChannelSetting;
}

} // namespace Ship
