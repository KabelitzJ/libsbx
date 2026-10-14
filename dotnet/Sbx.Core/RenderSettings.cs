namespace Sbx.Core
{

  /** Global rendering toggles (see the native scene_renderer_module). Not a Component -- these apply to the whole scene, not one node. */
  public static class RenderSettings
  {

    /** Draws opaque geometry as wireframe instead of filled triangles. Off by default. */
    public static bool WireframeEnabled
    {
      get { unsafe { return InternalCalls.RenderSettings_GetWireframeEnabled(); } }
      set { unsafe { InternalCalls.RenderSettings_SetWireframeEnabled(value); } }
    }

    /**
     * MSAA samples per pixel: 1 (off), 2, 4 or 8. Clamped to what the GPU supports. Changing it
     * rebuilds the renderer's targets and pipelines next frame, so expect a hitch -- a settings menu
     * thing, not a per-frame one. Changes the open project's value, like Project Settings > Rendering.
     */
    public static uint MsaaSamples
    {
      get { unsafe { return InternalCalls.RenderSettings_GetMsaaSamples(); } }
      set { unsafe { InternalCalls.RenderSettings_SetMsaaSamples(value); } }
    }

  } // class RenderSettings

} // namespace Sbx.Core
