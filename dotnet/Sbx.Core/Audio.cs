using Sbx.Core.Math;

namespace Sbx.Core
{

  /** Scene-wide audio: fire-and-forget sounds and mixer group volumes. Mixer groups are set up under Project Settings > Audio. */
  public static class Audio
  {

    /** Plays clip once at position (3D), routed to Master. Pauses with the simulation like any scene audio. */
    public static void PlayClipAtPoint(AudioClip clip, Vector3 position, float volume = 1.0f)
    {
      unsafe { InternalCalls.Audio_PlayClipAtPoint(clip.UUID, &position, volume); }
    }

    /**
     * Overrides a mixer group's volume for this session, e.g. from a settings menu slider. The
     * project's own value is untouched. Returns false if no group has that name.
     */
    public static bool SetMixerGroupVolume(string group, float volume)
    {
      unsafe { return InternalCalls.Audio_SetMixerGroupVolume(group, volume); }
    }

    /** The group's own volume, including any SetMixerGroupVolume override; 1 for an unknown group. */
    public static float GetMixerGroupVolume(string group)
    {
      unsafe { return InternalCalls.Audio_GetMixerGroupVolume(group); }
    }

    /** Drops a SetMixerGroupVolume override, returning to the project's volume. */
    public static void ResetMixerGroupVolume(string group)
    {
      unsafe { InternalCalls.Audio_ResetMixerGroupVolume(group); }
    }

  } // class Audio

} // namespace Sbx.Core
