#ifndef RECOMP_AUDIO_CAPTURE_XAPO_H
#define RECOMP_AUDIO_CAPTURE_XAPO_H

struct IXAudio2MasteringVoice;
void recomp_audio_capture_attach(IXAudio2MasteringVoice *master);
/* Call after DestroyVoice has stopped the audio callback. */
void recomp_audio_capture_save(void);

#endif
