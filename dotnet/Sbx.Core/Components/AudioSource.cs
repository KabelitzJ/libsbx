using System.Runtime.InteropServices;
using Sbx.Core;

namespace Sbx.Core.Components
{

  /** How a spatial source fades between MinDistance and MaxDistance. */
  public enum AudioRolloff : uint
  {
    /** Natural falloff, never quite silent; constant past MaxDistance. */
    Inverse = 0,
    /** Reaches silence at MaxDistance. */
    Linear = 1,
  }

  /**
   * Mirrors scripting::audio_source_data field for field (bools are uints there). AudioSource's
   * properties read and write it whole; use it directly to change several fields in one call.
   */
  [StructLayout(LayoutKind.Sequential)]
  public struct AudioSourceSettings
  {
    public float Volume;
    public float Pitch;
    public float MinDistance;
    public float MaxDistance;
    public float DopplerLevel;
    /** 0 is most important; when too many sounds play at once, the highest values go silent first. */
    public uint Priority;
    public AudioRolloff Rolloff;
    private uint _loop;
    private uint _playOnAwake;
    private uint _spatial;

    public bool Loop
    {
      get => _loop != 0;
      set => _loop = value ? 1u : 0u;
    }

    public bool PlayOnAwake
    {
      get => _playOnAwake != 0;
      set => _playOnAwake = value ? 1u : 0u;
    }

    /** Positioned in 3D at the node; otherwise heard evenly at the listener (music, UI). */
    public bool Spatial
    {
      get => _spatial != 0;
      set => _spatial = value ? 1u : 0u;
    }

  } // struct AudioSourceSettings

  public class AudioSource : Component
  {

    /** The whole authored configuration, as a copy: read, change, assign back. */
    public AudioSourceSettings Settings
    {
      get
      {
        AudioSourceSettings value;
        unsafe { InternalCalls.AudioSource_GetSettings(UUID, &value); }
        return value;
      }
      set
      {
        unsafe { InternalCalls.AudioSource_SetSettings(UUID, &value); }
      }
    }

    public AudioClip? Clip
    {
      get
      {
        ulong uuid;
        unsafe { uuid = InternalCalls.AudioSource_GetClip(UUID); }
        return AudioClip.FromHandle(uuid) as AudioClip;
      }
      set
      {
        unsafe { InternalCalls.AudioSource_SetClip(UUID, value?.UUID ?? 0); }
      }
    }

    /** The mixer group this source plays through, by name (Project Settings > Audio). Setting an unknown name logs an error and keeps the current group. */
    public string OutputGroup
    {
      get
      {
        string? name;
        unsafe { name = InternalCalls.AudioSource_GetOutputGroup(UUID); }
        return name ?? "Master";
      }
      set { unsafe { InternalCalls.AudioSource_SetOutputGroup(UUID, value); } }
    }

    public float Volume
    {
      get => Settings.Volume;
      set { var settings = Settings; settings.Volume = value; Settings = settings; }
    }

    public float Pitch
    {
      get => Settings.Pitch;
      set { var settings = Settings; settings.Pitch = value; Settings = settings; }
    }

    public bool Loop
    {
      get => Settings.Loop;
      set { var settings = Settings; settings.Loop = value; Settings = settings; }
    }

    public bool Spatial
    {
      get => Settings.Spatial;
      set { var settings = Settings; settings.Spatial = value; Settings = settings; }
    }

    public float MinDistance
    {
      get => Settings.MinDistance;
      set { var settings = Settings; settings.MinDistance = value; Settings = settings; }
    }

    public float MaxDistance
    {
      get => Settings.MaxDistance;
      set { var settings = Settings; settings.MaxDistance = value; Settings = settings; }
    }

    /** Starts the clip from the beginning, restarting it if already playing. */
    public void Play()
    {
      unsafe { InternalCalls.AudioSource_Play(UUID); }
    }

    public void Pause()
    {
      unsafe { InternalCalls.AudioSource_Pause(UUID); }
    }

    /** Resumes after Pause, where it left off. */
    public void UnPause()
    {
      unsafe { InternalCalls.AudioSource_UnPause(UUID); }
    }

    public void Stop()
    {
      unsafe { InternalCalls.AudioSource_Stop(UUID); }
    }

    /** Plays clip once on top of whatever this source is playing, from its position and output group. Clip and Volume of the source stay as they are. */
    public void PlayOneShot(AudioClip clip, float volumeScale = 1.0f)
    {
      unsafe { InternalCalls.AudioSource_PlayOneShot(UUID, clip.UUID, volumeScale); }
    }

    public bool IsPlaying
    {
      get { unsafe { return InternalCalls.AudioSource_GetIsPlaying(UUID); } }
    }

    /** Playback position in seconds. Setting it jumps there. */
    public float Time
    {
      get { unsafe { return InternalCalls.AudioSource_GetTime(UUID); } }
      set { unsafe { InternalCalls.AudioSource_SetTime(UUID, value); } }
    }

  } // class AudioSource

} // namespace Sbx.Core.Components
