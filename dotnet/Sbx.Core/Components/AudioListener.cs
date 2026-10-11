using Sbx.Core;

namespace Sbx.Core.Components
{

  /** Where the scene is heard from. Without one, the active camera listens. */
  public class AudioListener : Component
  {

    /** Scales everything this listener hears. */
    public float Volume
    {
      get { unsafe { return InternalCalls.AudioListener_GetVolume(UUID); } }
      set { unsafe { InternalCalls.AudioListener_SetVolume(UUID, value); } }
    }

  } // class AudioListener

} // namespace Sbx.Core.Components
