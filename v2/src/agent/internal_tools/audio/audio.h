#ifndef AUDIO_H
#define AUDIO_H

class Audio
{
    private:
        bool inicialized = false;
        
    public:
        Audio();
        ~Audio();

        bool inicialize();

        bool speak(const std::string &prhase);

        void shutdown();
}

#endif AUDIO_H