namespace Sbx.Core
{

  /**
   * A WAV/MP3/FLAC/OGG asset as the engine cooks it. A shared, cached asset like Font; nothing to
   * dispose. Usable as a script field: assign it in the Inspector like a Texture2D.
   */
  public sealed class AudioClip : Managed.INativeHandle
  {
    private readonly ulong _uuid;

    public ulong UUID => _uuid;

    ulong Managed.INativeHandle.Handle => _uuid;

    internal AudioClip(ulong uuid)
    {
      _uuid = uuid;
    }

    public static object? FromHandle(ulong handle) => handle != 0 ? new AudioClip(handle) : null;

    /** path is project-relative, e.g. "audio/explosion.wav". */
    public static AudioClip? Load(string path)
    {
      ulong uuid;
      unsafe { uuid = InternalCalls.AudioClip_Load(path); }
      return uuid != 0 ? new AudioClip(uuid) : null;
    }

    /** False for a while after Load: the clip decodes on a background thread. Playing it before then simply starts once it's ready. */
    public bool IsLoaded
    {
      get { unsafe { return InternalCalls.AudioClip_IsLoaded(_uuid); } }
    }

    /** Seconds; 0 until loaded. */
    public float Length
    {
      get { unsafe { return InternalCalls.AudioClip_GetLength(_uuid); } }
    }

    /** 1 or 2 after import; 0 until loaded. */
    public int Channels
    {
      get { unsafe { return (int)InternalCalls.AudioClip_GetChannels(_uuid); } }
    }

    /** Sample rate in Hz; 0 until loaded. */
    public int Frequency
    {
      get { unsafe { return (int)InternalCalls.AudioClip_GetFrequency(_uuid); } }
    }

  } // class AudioClip

} // namespace Sbx.Core
